using System;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using Excel = Microsoft.Office.Interop.Excel;

namespace BrigeLogCopy
{
    /// <summary>
    /// Отчёт Excel «Отчет_Bridge»: заголовки, строка акта, строки операций.
    /// </summary>
    public static class ExcelReportHelper
    {
        private const int HeaderRow = 1;
        private const int ActRow = 2;
        private const int FirstDataRow = 3;
        private const int ColCount = 5;
        private const int SerialColumn = 4;

        /// <param name="actFolderPath">Папка по № акта (внутри неё лежит «Отчет_Bridge.xlsx»).</param>
        public static string GetReportPath(string actFolderPath)
        {
            return Path.Combine(actFolderPath.Trim(), "Отчет_Bridge.xlsx");
        }

        /// <summary>
        /// Ищет в таблице данных (со строки 3) совпадение серийного номера в столбце «Серийный номер».
        /// Файл открывается только для чтения. Если файла нет — false.
        /// </summary>
        public static bool SerialExistsInReport(string reportFullPath, string serialNumber)
        {
            if (string.IsNullOrWhiteSpace(serialNumber) || !File.Exists(reportFullPath))
                return false;

            string needle = NormalizeSerial(serialNumber);
            if (needle.Length == 0)
                return false;

            Excel.Application app = null;
            Excel.Workbook wb = null;

            try
            {
                app = new Excel.Application
                {
                    Visible = false,
                    DisplayAlerts = false,
                    ScreenUpdating = false
                };
                wb = app.Workbooks.Open(reportFullPath, Missing.Value, true);

                var ws = (Excel.Worksheet)wb.Sheets[1];
                const int maxScan = 100000;
                for (int r = FirstDataRow; r < FirstDataRow + maxScan; r++)
                {
                    object v1 = GetCellValue2(ws, r, 1);
                    object v2 = GetCellValue2(ws, r, 2);
                    if (v1 == null && v2 == null)
                        break;

                    object cellSerial = GetCellValue2(ws, r, SerialColumn);
                    if (cellSerial == null)
                        continue;
                    string existing = NormalizeSerial(Convert.ToString(cellSerial, CultureInfo.InvariantCulture));
                    if (existing.Length > 0 && string.Equals(existing, needle, StringComparison.OrdinalIgnoreCase))
                        return true;
                }

                return false;
            }
            finally
            {
                if (wb != null)
                {
                    wb.Close(false);
                    Marshal.FinalReleaseComObject(wb);
                }
                if (app != null)
                {
                    app.Quit();
                    Marshal.FinalReleaseComObject(app);
                }
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
        }

        private static string NormalizeSerial(string s)
        {
            return (s ?? "").Trim();
        }

        /// <summary>
        /// Добавляет строку операции и обновляет строку № акта (объединённые ячейки).
        /// </summary>
        public static void AppendOperationRow(
            string reportFullPath,
            string actNumber,
            string serialNumber,
            string employeeFio)
        {
            if (string.IsNullOrWhiteSpace(reportFullPath))
                throw new ArgumentException("Путь к отчёту не задан.", nameof(reportFullPath));

            string fullPath = Path.GetFullPath(reportFullPath.Trim());
            var dir = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
                Directory.CreateDirectory(dir);

            bool reportExists = File.Exists(fullPath);

            Excel.Application app = null;
            Excel.Workbook wb = null;

            try
            {
                app = new Excel.Application
                {
                    Visible = false,
                    DisplayAlerts = false,
                    ScreenUpdating = false
                };

                if (reportExists)
                    wb = app.Workbooks.Open(fullPath);
                else
                    wb = app.Workbooks.Add();

                var ws = (Excel.Worksheet)wb.Sheets[1];
                ws.Name = "Отчёт";

                if (!reportExists)
                    CreateNewReportStructure(ws, actNumber);
                else
                    EnsureStructureAndUpdateAct(ws, actNumber);

                int nextRow = FindNextDataRow(ws);
                int opNo = GetNextOperationNumber(ws, nextRow);

                ws.Cells[nextRow, 1] = opNo;
                ws.Cells[nextRow, 2] = DateTime.Now.ToString("dd.MM.yyyy HH:mm:ss", CultureInfo.GetCultureInfo("ru-RU"));
                ws.Cells[nextRow, 3] = "Россия";
                ws.Cells[nextRow, 4] = serialNumber ?? "";
                ws.Cells[nextRow, 5] = employeeFio ?? "";

                ApplyTableGridBorders(ws, nextRow);

                if (reportExists)
                    wb.Save();
                else
                    wb.SaveAs(fullPath, Excel.XlFileFormat.xlOpenXMLWorkbook);
            }
            finally
            {
                if (wb != null)
                {
                    try
                    {
                        /* После Save/SaveAs Close(false) иногда не фиксирует xlsx на диске; true — сохранить при закрытии книги. */
                        wb.Close(true);
                    }
                    catch
                    {
                        try { wb.Close(false); } catch { /* ignore */ }
                    }
                    Marshal.FinalReleaseComObject(wb);
                    wb = null;
                }
                if (app != null)
                {
                    app.Quit();
                    Marshal.FinalReleaseComObject(app);
                    app = null;
                }
                GC.Collect();
                GC.WaitForPendingFinalizers();
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }

            WaitForReportFileOnDisk(fullPath);
        }

        /// <summary>Excel иногда отпускает файл с задержкой после Quit.</summary>
        private static void WaitForReportFileOnDisk(string fullPath, int attempts = 40, int delayMs = 100)
        {
            for (int i = 0; i < attempts; i++)
            {
                try
                {
                    if (File.Exists(fullPath))
                    {
                        var len = new FileInfo(fullPath).Length;
                        if (len > 32)
                            return;
                    }
                }
                catch
                {
                    /* файл ещё заблокирован процессом Excel */
                }
                Thread.Sleep(delayMs);
            }
            throw new IOException("Файл отчёта не появился на диске (или пустой): " + fullPath);
        }

        private static void CreateNewReportStructure(Excel.Worksheet ws, string actNumber)
        {
            ws.Cells[HeaderRow, 1] = "№";
            ws.Cells[HeaderRow, 2] = "Дата";
            ws.Cells[HeaderRow, 3] = "производитель";
            ws.Cells[HeaderRow, 4] = "Серийный номер";
            ws.Cells[HeaderRow, 5] = "Исполнитель";

            Excel.Range headerRange = null;
            try
            {
                headerRange = ws.Range[ws.Cells[HeaderRow, 1], ws.Cells[HeaderRow, ColCount]];
                headerRange.Font.Bold = true;
            }
            finally
            {
                if (headerRange != null)
                    Marshal.FinalReleaseComObject(headerRange);
            }

            SetActRow(ws, actNumber);
        }

        private static void EnsureStructureAndUpdateAct(Excel.Worksheet ws, string actNumber)
        {
            string a1 = GetCellText(ws, HeaderRow, 1);
            if (string.IsNullOrWhiteSpace(a1) || !a1.Trim().Equals("№", StringComparison.Ordinal))
                CreateNewReportStructure(ws, actNumber);
            else
                SetActRow(ws, actNumber);
        }

        private static string GetCellText(Excel.Worksheet ws, int row, int col)
        {
            Excel.Range cell = (Excel.Range)ws.Cells[row, col];
            try
            {
                object t = cell.Text;
                return t?.ToString() ?? "";
            }
            finally
            {
                Marshal.FinalReleaseComObject(cell);
            }
        }

        private static object GetCellValue2(Excel.Worksheet ws, int row, int col)
        {
            Excel.Range cell = (Excel.Range)ws.Cells[row, col];
            try
            {
                return cell.Value2;
            }
            finally
            {
                Marshal.FinalReleaseComObject(cell);
            }
        }

        private static void SetActRow(Excel.Worksheet ws, string actNumber)
        {
            Excel.Range actRange = null;
            try
            {
                actRange = ws.Range[ws.Cells[ActRow, 1], ws.Cells[ActRow, ColCount]];
                try
                {
                    if (Convert.ToBoolean(actRange.MergeCells))
                        actRange.UnMerge();
                }
                catch
                {
                    /* ignore */
                }
                actRange.Merge();
                actRange.HorizontalAlignment = Excel.XlHAlign.xlHAlignCenter;
                actRange.Value2 = "№ акта: " + (actNumber ?? "");
            }
            finally
            {
                if (actRange != null)
                    Marshal.FinalReleaseComObject(actRange);
            }
        }

        private static int FindNextDataRow(Excel.Worksheet ws)
        {
            const int maxScan = 100000;
            for (int r = FirstDataRow; r < FirstDataRow + maxScan; r++)
            {
                object v1 = GetCellValue2(ws, r, 1);
                object v2 = GetCellValue2(ws, r, 2);
                if (v1 == null && v2 == null)
                    return r;
            }
            return FirstDataRow + maxScan;
        }

        private static int GetNextOperationNumber(Excel.Worksheet ws, int nextRow)
        {
            int max = 0;
            for (int r = FirstDataRow; r < nextRow; r++)
            {
                object v = GetCellValue2(ws, r, 1);
                if (v == null) continue;
                int n;
                if (v is double d)
                    n = (int)d;
                else if (!int.TryParse(Convert.ToString(v, CultureInfo.InvariantCulture), out n))
                    continue;
                if (n > max) max = n;
            }
            return max + 1;
        }

        /// <summary>
        /// Все границы таблицы: внешний контур и внутренние линии (как сетка).
        /// </summary>
        private static void ApplyTableGridBorders(Excel.Worksheet ws, int lastRow)
        {
            if (lastRow < HeaderRow)
                return;

            Excel.Range tableRange = null;
            try
            {
                tableRange = ws.Range[ws.Cells[HeaderRow, 1], ws.Cells[lastRow, ColCount]];
                Excel.Borders borders = tableRange.Borders;
                try
                {
                    var indices = new[]
                    {
                        Excel.XlBordersIndex.xlEdgeLeft,
                        Excel.XlBordersIndex.xlEdgeTop,
                        Excel.XlBordersIndex.xlEdgeBottom,
                        Excel.XlBordersIndex.xlEdgeRight,
                        Excel.XlBordersIndex.xlInsideVertical,
                        Excel.XlBordersIndex.xlInsideHorizontal
                    };
                    foreach (Excel.XlBordersIndex idx in indices)
                    {
                        Excel.Border side = borders[idx];
                        try
                        {
                            side.LineStyle = Excel.XlLineStyle.xlContinuous;
                            side.Weight = Excel.XlBorderWeight.xlThin;
                        }
                        finally
                        {
                            Marshal.FinalReleaseComObject(side);
                        }
                    }
                }
                finally
                {
                    Marshal.FinalReleaseComObject(borders);
                }
            }
            finally
            {
                if (tableRange != null)
                    Marshal.FinalReleaseComObject(tableRange);
            }
        }
    }
}
