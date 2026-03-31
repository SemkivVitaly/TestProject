using System;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
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

        public static string GetReportPath(string baseDirectory)
        {
            return Path.Combine(baseDirectory.Trim(), "Отчет_Bridge.xlsx");
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

            var dir = Path.GetDirectoryName(reportFullPath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
                Directory.CreateDirectory(dir);

            bool reportExists = File.Exists(reportFullPath);

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
                    wb = app.Workbooks.Open(reportFullPath);
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
                    wb.SaveAs(reportFullPath, Excel.XlFileFormat.xlOpenXMLWorkbook);
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
            var a1 = ws.Cells[HeaderRow, 1].Text?.ToString() ?? "";
            if (string.IsNullOrWhiteSpace(a1) || !a1.Trim().Equals("№", StringComparison.Ordinal))
                CreateNewReportStructure(ws, actNumber);
            else
                SetActRow(ws, actNumber);
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
                var v1 = ws.Cells[r, 1].Value2;
                var v2 = ws.Cells[r, 2].Value2;
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
                var v = ws.Cells[r, 1].Value2;
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
