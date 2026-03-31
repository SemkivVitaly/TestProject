using System;
using System.Configuration;
using System.IO;
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
            
            
        }

        private void btnBrowseFolder_Click(object sender, EventArgs e)
        {
            using (var dlg = new FolderBrowserDialog())
            {
                dlg.Description = "Папка для отчёта Excel и архивов логов";
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
            string serial = txtSerial.Text?.Trim() ?? "";
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
            if (serial.Length == 0)
            {
                MessageBox.Show(this, "Укажите серийный номер.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                txtSerial.Focus();
                return;
            }
            if (root.Length == 0)
            {
                MessageBox.Show(this, "Укажите путь к папке для отчётов.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                txtReportsPath.Focus();
                return;
            }

            try
            {
                Path.GetFullPath(root);
            }
            catch
            {
                MessageBox.Show(this, "Некорректный путь к папке.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            string bridgeUrl = ConfigurationManager.AppSettings["BridgeBaseUrl"];
            if (string.IsNullOrWhiteSpace(bridgeUrl))
                bridgeUrl = "http://192.168.2.1";

            btnSaveLogs.Enabled = false;
            btnBrowseFolder.Enabled = false;
            UseWaitCursor = true;
            try
            {
                try
                {
                    await LogArchiveService.SaveLogsFromBridgeAsync(bridgeUrl, root, serial).ConfigureAwait(true);
                }
                catch (Exception exNet)
                {
                    MessageBox.Show(this,
                        "Не удалось скачать логи с моста. Проверьте Wi‑Fi, адрес в App.config и что устройство в сети.\n\n" + exNet.Message,
                        Text,
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                    return;
                }

                try
                {
                    ExcelReportHelper.AppendOperationRow(
                        ExcelReportHelper.GetReportPath(root),
                        act,
                        serial,
                        fio);
                }
                catch (Exception exExcel)
                {
                    MessageBox.Show(this,
                        "Логи сохранены в папку серийного номера, но Excel не обновлён (нужен установленный Microsoft Office):\n" + exExcel.Message,
                        Text,
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                    return;
                }

                MessageBox.Show(this,
                    "Готово: обновлён файл «Отчет_Bridge.xlsx», логи — в подпапке «" +
                    LogArchiveService.SanitizeFolderName(serial) + "».",
                    Text,
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
            }
            finally
            {
                UseWaitCursor = false;
                btnSaveLogs.Enabled = true;
                btnBrowseFolder.Enabled = true;
            }
        }
    }
}
