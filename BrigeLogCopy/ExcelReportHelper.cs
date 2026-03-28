using System.Runtime.InteropServices;
using Excel = Microsoft.Office.Interop.Excel;

namespace BrigeLogCopy;

internal static class ExcelReportHelper
{
    private const string Manufacturer = "Россия";
    private const int ExcelLastRow = 1048576;
    private const int LastCol = 5;
    /// <summary>Строка заголовков таблицы.</summary>
    private const int RowHeader = 1;
    /// <summary>Строка «№ Акта» (все столбцы объединены).</summary>
    private const int RowAct = 2;
    /// <summary>Первая строка данных (порядковый №, дата, …).</summary>
    private const int RowFirstData = 3;

    internal static void AppendRow(string excelPath, string actNumber, string serialNumber, string executorFio)
    {
        Excel.Application? app = null;
        Excel.Workbook? wb = null;
        Excel.Worksheet? ws = null;

        try
        {
            app = new Excel.Application { Visible = false, DisplayAlerts = false };
            var fullPath = Path.GetFullPath(excelPath);

            if (File.Exists(fullPath))
            {
                wb = app.Workbooks.Open(fullPath);
                ws = (Excel.Worksheet)wb.Sheets[1];
                EnsureHeaders(ws);
                WriteActMergedRow(ws, actNumber);
                int row = GetNextDataRow(ws);
                FillDataRow(ws, row, serialNumber, executorFio);
                ApplyGridBorders(ws, row);
                wb.Save();
            }
            else
            {
                wb = app.Workbooks.Add();
                ws = (Excel.Worksheet)wb.Sheets[1];
                WriteHeaders(ws);
                WriteActMergedRow(ws, actNumber);
                FillDataRow(ws, RowFirstData, serialNumber, executorFio);
                ApplyGridBorders(ws, RowFirstData);
                wb.SaveAs(fullPath);
            }
        }
        finally
        {
            if (ws != null)
            {
                Marshal.FinalReleaseComObject(ws);
                ws = null;
            }

            if (wb != null)
            {
                wb.Close(SaveChanges: true);
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
        }
    }

    /// <summary>Строка 2: A–E объединены, текст «№ Акта: …».</summary>
    private static void WriteActMergedRow(Excel.Worksheet ws, string actNumber)
    {
        UnmergeRow2IfNeeded(ws);

        Excel.Range? mergeRange = null;
        try
        {
            var topLeft = (Excel.Range)ws.Cells[RowAct, 1];
            var bottomRight = (Excel.Range)ws.Cells[RowAct, LastCol];
            mergeRange = ws.Range[topLeft, bottomRight];
            Marshal.FinalReleaseComObject(topLeft);
            Marshal.FinalReleaseComObject(bottomRight);

            mergeRange.Merge(false);
            mergeRange.Value2 = "№ Акта: " + actNumber;
            mergeRange.HorizontalAlignment = Excel.XlHAlign.xlHAlignLeft;
            mergeRange.VerticalAlignment = Excel.XlVAlign.xlVAlignCenter;
        }
        finally
        {
            if (mergeRange != null)
                Marshal.FinalReleaseComObject(mergeRange);
        }
    }

    private static void UnmergeRow2IfNeeded(Excel.Worksheet ws)
    {
        Excel.Range? c = null;
        try
        {
            c = (Excel.Range)ws.Cells[RowAct, 1];
            if (true.Equals(c.MergeCells))
            {
                Excel.Range? area = null;
                try
                {
                    area = c.MergeArea;
                    area.UnMerge();
                }
                finally
                {
                    if (area != null)
                        Marshal.FinalReleaseComObject(area);
                }
            }
        }
        finally
        {
            if (c != null)
                Marshal.FinalReleaseComObject(c);
        }
    }

    /// <summary>Колонка «№» — порядковый номер записи 1, 2, 3… (строки 1–2 не считаются).</summary>
    private static void FillDataRow(Excel.Worksheet ws, int row, string serialNumber, string executorFio)
    {
        int seq = row - RowAct;
        var cellNo = (Excel.Range)ws.Cells[row, 1];
        try
        {
            cellNo.Value2 = seq;
            cellNo.NumberFormat = "0";
        }
        finally
        {
            Marshal.FinalReleaseComObject(cellNo);
        }

        ws.Cells[row, 2] = DateTime.Now.ToString("dd.MM.yyyy HH:mm:ss");
        ws.Cells[row, 3] = Manufacturer;
        ws.Cells[row, 4] = serialNumber;
        ws.Cells[row, 5] = executorFio;
    }

    private static void ApplyGridBorders(Excel.Worksheet ws, int lastDataRow)
    {
        if (lastDataRow < RowHeader) return;

        Excel.Range? topLeft = null;
        Excel.Range? bottomRight = null;
        Excel.Range? tableRange = null;

        try
        {
            topLeft = (Excel.Range)ws.Cells[RowHeader, 1];
            bottomRight = (Excel.Range)ws.Cells[lastDataRow, LastCol];
            tableRange = ws.Range[topLeft, bottomRight];

            Excel.XlBordersIndex[] edges =
            [
                Excel.XlBordersIndex.xlEdgeLeft,
                Excel.XlBordersIndex.xlEdgeTop,
                Excel.XlBordersIndex.xlEdgeBottom,
                Excel.XlBordersIndex.xlEdgeRight,
                Excel.XlBordersIndex.xlInsideVertical,
                Excel.XlBordersIndex.xlInsideHorizontal
            ];

            foreach (var bi in edges)
            {
                Excel.Border? b = null;
                try
                {
                    b = tableRange.Borders[bi];
                    b.LineStyle = Excel.XlLineStyle.xlContinuous;
                    b.Weight = Excel.XlBorderWeight.xlThin;
                    b.ColorIndex = Excel.XlColorIndex.xlColorIndexAutomatic;
                }
                finally
                {
                    if (b != null)
                        Marshal.FinalReleaseComObject(b);
                }
            }
        }
        finally
        {
            if (tableRange != null)
                Marshal.FinalReleaseComObject(tableRange);
            if (bottomRight != null)
                Marshal.FinalReleaseComObject(bottomRight);
            if (topLeft != null)
                Marshal.FinalReleaseComObject(topLeft);
        }
    }

    private static void WriteHeaders(Excel.Worksheet ws)
    {
        ws.Cells[1, 1] = "№";
        ws.Cells[1, 2] = "Дата";
        ws.Cells[1, 3] = "Производитель";
        ws.Cells[1, 4] = "Серийный номер";
        ws.Cells[1, 5] = "Исполнитель";
    }

    private static void EnsureHeaders(Excel.Worksheet ws)
    {
        Excel.Range? c = null;
        try
        {
            c = (Excel.Range)ws.Cells[1, 1];
            var h1 = Convert.ToString(c.Value2) ?? "";
            if (!h1.Trim().Equals("№", StringComparison.Ordinal))
                WriteHeaders(ws);
        }
        finally
        {
            if (c != null)
                Marshal.FinalReleaseComObject(c);
        }
    }

    /// <summary>Следующая свободная строка данных (не раньше 3-й).</summary>
    private static int GetNextDataRow(Excel.Worksheet ws)
    {
        Excel.Range? bottom = null;
        Excel.Range? last = null;
        try
        {
            bottom = (Excel.Range)ws.Cells[ExcelLastRow, 1];
            last = bottom.End[Excel.XlDirection.xlUp];
            int r = last.Row;
            if (r < RowFirstData)
                return RowFirstData;
            return r + 1;
        }
        finally
        {
            if (last != null) Marshal.FinalReleaseComObject(last);
            if (bottom != null) Marshal.FinalReleaseComObject(bottom);
        }
    }
}
