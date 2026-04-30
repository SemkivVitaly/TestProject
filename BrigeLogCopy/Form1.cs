using System;
using System.Configuration;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BrigeLogCopy
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            gridSerials.AllowUserToAddRows = true;
            gridSerials.AllowUserToDeleteRows = true;
            gridSerials.MultiSelect = false;
            gridSerials.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
        }

        /// <summary>Первая строка с непустым серийным номером (не строка «новая запись»).</summary>
        private bool TryGetFirstSerialRow(out int rowIndex, out string serial)
        {
            rowIndex = -1;
            serial = null;
            foreach (DataGridViewRow row in gridSerials.Rows)
            {
                if (row.IsNewRow)
                    continue;
                string s = row.Cells[colSerial.Index].Value?.ToString()?.Trim() ?? "";
                if (s.Length == 0)
                    continue;
                rowIndex = row.Index;
                serial = s;
                return true;
            }
            return false;
        }

        private void RemoveRowAt(int index)
        {
            if (index < 0 || index >= gridSerials.Rows.Count)
                return;
            if (gridSerials.Rows[index].IsNewRow)
                return;
            gridSerials.Rows.RemoveAt(index);
        }

        private void btnBrowseFolder_Click(object sender, EventArgs e)
        {
            using (var dlg = new FolderBrowserDialog())
            {
                dlg.Description = "Корневая папка: внутри будет создана папка с именем по № акта";
                dlg.SelectedPath = string.IsNullOrWhiteSpace(txtReportsPath.Text)
                    ? Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)
                    : txtReportsPath.Text;
                if (dlg.ShowDialog(this) == DialogResult.OK)
                    txtReportsPath.Text = dlg.SelectedPath;
            }
        }

        private async void btnSaveLogs_Click(object sender, EventArgs e)
        {
            string fio = txtFio.Text?.Trim() ?? "";
            string act = txtActNumber.Text?.Trim() ?? "";
            string root = txtReportsPath.Text?.Trim() ?? "";

            if (fio.Length == 0)
            {
                MessageBox.Show(this, "Укажите ФИО сотрудника.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                txtFio.Focus();
                return;
            }
            if (act.Length == 0)
            {
                MessageBox.Show(this, "Укажите № акта.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                txtActNumber.Focus();
                return;
            }
            if (!TryGetFirstSerialRow(out int serialRowIndex, out string serial))
            {
                MessageBox.Show(this, "Добавьте в таблицу хотя бы один серийный номер (первая заполненная строка сверху).", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                gridSerials.Focus();
                return;
            }
            if (root.Length == 0)
            {
                MessageBox.Show(this, "Укажите путь к папке для отчётов.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                txtReportsPath.Focus();
                return;
            }

            string rootFull;
            try
            {
                rootFull = Path.GetFullPath(root.Trim());
            }
            catch
            {
                MessageBox.Show(this, "Некорректный путь к папке.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            string actFolderName = LogArchiveService.SanitizeFolderName(act);
            string actRoot = Path.Combine(rootFull, actFolderName);
            Directory.CreateDirectory(actRoot);

            string reportPath = ExcelReportHelper.GetReportPath(actRoot);
            bool dupFolder = LogArchiveService.SerialFolderHasSavedContent(actRoot, serial);
            bool dupExcel = false;
            string excelCheckError = null;
            try
            {
                dupExcel = ExcelReportHelper.SerialExistsInReport(reportPath, serial);
            }
            catch (Exception ex)
            {
                excelCheckError = ex.Message;
            }

            if (dupFolder || dupExcel || excelCheckError != null)
            {
                var sb = new StringBuilder();
                if (dupFolder)
                    sb.AppendLine("• Папка для этого серийного номера уже содержит сохранённые файлы логов.");
                if (dupExcel)
                    sb.AppendLine("• Этот серийный номер уже есть в столбце «Серийный номер» в «Отчет_Bridge.xlsx».");
                if (excelCheckError != null)
                    sb.AppendLine("• Не удалось проверить Excel: " + excelCheckError);
                sb.AppendLine();
                sb.AppendLine("Продолжить сохранение?");
                var ask = MessageBox.Show(this, sb.ToString(), Text, MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2);
                if (ask != DialogResult.Yes)
                    return;
            }

            string bridgeUrl = ConfigurationManager.AppSettings["BridgeBaseUrl"];
            if (string.IsNullOrWhiteSpace(bridgeUrl))
                bridgeUrl = "http://192.168.2.1";

            btnSaveLogs.Enabled = false;
            btnBrowseFolder.Enabled = false;
            gridSerials.Enabled = false;
            UseWaitCursor = true;
            try
            {
                LogArchiveService.SnapshotResult snap = null;
                try
                {
                    snap = await LogArchiveService.SaveLogsFromBridgeAsync(bridgeUrl, actRoot, serial).ConfigureAwait(true);
                }
                catch (Exception exNet)
                {
                    MessageBox.Show(this,
                        "Не удалось инициировать загрузку логов с моста (Wi‑Fi, App.config, доступность устройства).\n" +
                        "Отчёт Excel всё равно будет создан или дополнен.\n\n" + exNet.Message,
                        Text,
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                }

                LogArchiveService.QuickExportResult quick = null;
                if (LogArchiveService.IsLogExportEnabled())
                {
                    try
                    {
                        string qDir = LogArchiveService.ResolveLogExportDirectory();
                        quick = await LogArchiveService.ExportQuickLogsToFolderAsync(bridgeUrl, qDir).ConfigureAwait(true);
                    }
                    catch (Exception exQ)
                    {
                        if (quick == null)
                            quick = new LogArchiveService.QuickExportResult();
                        quick.Errors.Add(exQ.Message);
                    }
                }

                bool logsOk = snap != null && snap.AnySaved;
                if (snap != null && !snap.AllSaved)
                {
                    string head = snap.AnySaved
                        ? ("Часть логов не скачалась (" + snap.SavedCount + "/" + snap.TotalCount + "). Не удалось:")
                        : "Логи не скачались. Ошибки:";
                    MessageBox.Show(this,
                        head + "\n  " + string.Join("\n  ", snap.Errors) +
                        "\n\nОтчёт Excel будет создан или дополнен в любом случае.",
                        Text,
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                }

                try
                {
                    ExcelReportHelper.AppendOperationRow(
                        ExcelReportHelper.GetReportPath(actRoot),
                        act,
                        serial,
                        fio);
                }
                catch (Exception exExcel)
                {
                    MessageBox.Show(this,
                        "Не удалось обновить Excel (нужен Microsoft Office).\n" +
                        (logsOk ? "Логи с моста при этом сохранены в папку серийного номера.\n" : "") +
                        "Строка в таблице очереди не удалена — можно повторить попытку.\n\n" + exExcel.Message,
                        Text,
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                    return;
                }

                RemoveRowAt(serialRowIndex);

                int remaining = CountDataRows();
                string reportPathDone = ExcelReportHelper.GetReportPath(actRoot);
                string logsNote;
                if (snap != null && snap.AllSaved)
                    logsNote = "Логи с моста сохранены в папку серийного номера (" + snap.SavedCount + "/" + snap.TotalCount + " эндпоинтов).";
                else if (logsOk)
                    logsNote = "Часть логов сохранена (" + snap.SavedCount + "/" + snap.TotalCount + "). Повторите попытку для недостающих файлов.";
                else
                    logsNote = "Логи с моста не сохранены — при необходимости повторите операцию после настройки сети.";
                string exportNote = "";
                if (quick != null)
                {
                    if (quick.AnyOk)
                    {
                        var lines = new System.Text.StringBuilder();
                        lines.AppendLine("Доп. копии логов:");
                        if (!string.IsNullOrEmpty(quick.UnifiedLogPath))
                            lines.AppendLine("• " + quick.UnifiedLogPath);
                        if (!string.IsNullOrEmpty(quick.Esp32LogPath))
                            lines.AppendLine("• " + quick.Esp32LogPath);
                        if (!string.IsNullOrEmpty(quick.MavlinkJsonPath))
                            lines.AppendLine("• " + quick.MavlinkJsonPath);
                        exportNote = lines.ToString().TrimEnd() + "\n";
                    }
                    if (quick.Errors.Count > 0)
                        exportNote += "Замечания доп. экспорта:\n  " + string.Join("\n  ", quick.Errors) + "\n";
                }
                MessageBox.Show(this,
                    "Готово. Серийный номер: «" + serial + "».\nПапка акта: «" + actFolderName + "».\n" +
                    logsNote + "\n" +
                    exportNote +
                    "Отчёт Excel:\n" + reportPathDone + "\n" +
                    "В очереди осталось записей: " + remaining + ".",
                    Text,
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
            }
            finally
            {
                UseWaitCursor = false;
                gridSerials.Enabled = true;
                btnSaveLogs.Enabled = true;
                btnBrowseFolder.Enabled = true;
            }
        }

        private int CountDataRows()
        {
            int n = 0;
            foreach (DataGridViewRow row in gridSerials.Rows)
            {
                if (row.IsNewRow)
                    continue;
                string s = row.Cells[colSerial.Index].Value?.ToString()?.Trim() ?? "";
                if (s.Length > 0)
                    n++;
            }
            return n;
        }
    }
}
