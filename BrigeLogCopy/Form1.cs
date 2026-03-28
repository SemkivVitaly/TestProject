using System.Runtime.InteropServices;
using System.Text;

namespace BrigeLogCopy;

public partial class Form1 : Form
{
    public Form1()
    {
        InitializeComponent();
    }

    private void btnPath_Click(object sender, EventArgs e)
    {
        using var fbd = new FolderBrowserDialog
        {
            Description = "Выберите папку для Excel-отчёта и папок с логами",
            UseDescriptionForTitle = true
        };
        if (!string.IsNullOrWhiteSpace(tbFilePath.Text) && Directory.Exists(tbFilePath.Text))
            fbd.SelectedPath = tbFilePath.Text;

        if (fbd.ShowDialog(this) == DialogResult.OK)
            tbFilePath.Text = fbd.SelectedPath;
    }

    private async void btnSaveLog_Click(object sender, EventArgs e)
    {
        if (string.IsNullOrWhiteSpace(tbFIO.Text))
        {
            MessageBox.Show(this, "Укажите ФИО исполнителя.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            tbFIO.Focus();
            return;
        }

        if (string.IsNullOrWhiteSpace(tbAKT.Text))
        {
            MessageBox.Show(this, "Укажите № акта.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            tbAKT.Focus();
            return;
        }

        if (string.IsNullOrWhiteSpace(tbSerialNumber.Text))
        {
            MessageBox.Show(this, "Укажите серийный номер.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            tbSerialNumber.Focus();
            return;
        }

        if (string.IsNullOrWhiteSpace(tbFilePath.Text) || !Directory.Exists(tbFilePath.Text))
        {
            MessageBox.Show(this, "Укажите существующую папку для сохранения (кнопка «Выбрать»).", Text,
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
            tbFilePath.Focus();
            return;
        }

        UseWaitCursor = true;
        btnSaveLog.Enabled = false;
        try
        {
            var baseUrl = BridgeHttpClient.DefaultBaseUrl;
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(50));

            var unifiedTask = BridgeHttpClient.GetTextAsync(baseUrl, "/api/log/file", cts.Token);
            var jsonTask = BridgeHttpClient.GetTextAsync(baseUrl, "/api/log", cts.Token);
            await Task.WhenAll(unifiedTask, jsonTask).ConfigureAwait(true);

            var (okU, unified, errU) = await unifiedTask.ConfigureAwait(true);
            var (okJ, json, errJ) = await jsonTask.ConfigureAwait(true);

            if (!okU || !okJ)
            {
                var msg = "Не удалось получить логи с моста.\n" +
                          $"Адрес: {baseUrl}\n" +
                          (!okU ? $"Единый лог: {errU}\n" : "") +
                          (!okJ ? $"JSON: {errJ}" : "");
                MessageBox.Show(this, msg, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            var serialFolder = Path.Combine(tbFilePath.Text.Trim(), SanitizeFolderName(tbSerialNumber.Text.Trim()));
            Directory.CreateDirectory(serialFolder);

            var unifiedPath = Path.Combine(serialFolder, $"единый_лог_{tbSerialNumber.Text}.txt");
            var jsonPath = Path.Combine(serialFolder, $"mavlink_log_{tbSerialNumber.Text}.json");
            await File.WriteAllTextAsync(unifiedPath, unified ?? "", Encoding.UTF8, cts.Token).ConfigureAwait(true);
            await File.WriteAllTextAsync(jsonPath, json ?? "[]", Encoding.UTF8, cts.Token).ConfigureAwait(true);
            

            var excelPath = Path.Combine(tbFilePath.Text.Trim(), $"Отчет_Bridge_{tbAKT.Text}.xlsx");
            try
            {
                ExcelReportHelper.AppendRow(excelPath, tbAKT.Text.Trim(), tbSerialNumber.Text.Trim(), tbFIO.Text.Trim());
            }
            catch (COMException ex)
            {
                MessageBox.Show(this,
                    "Ошибка Excel (Interop). Убедитесь, что установлен Microsoft Excel.\n" + ex.Message,
                    Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, "Ошибка записи Excel: " + ex.Message, Text, MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            MessageBox.Show(this,
                $"Готово.\nОтчёт: {excelPath}\nЛоги: {serialFolder}",
                Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        finally
        {
            btnSaveLog.Enabled = true;
            UseWaitCursor = false;
        }
    }

    private static string SanitizeFolderName(string raw)
    {
        var invalid = Path.GetInvalidFileNameChars();
        var parts = raw.Split(invalid, StringSplitOptions.RemoveEmptyEntries);
        var s = string.Join("_", parts).Trim();
        return string.IsNullOrEmpty(s) ? "NO_SERIAL" : s;
    }
}
