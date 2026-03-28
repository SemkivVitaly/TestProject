#nullable disable
namespace BrigeLogCopy
{
    partial class Form1
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        private void InitializeComponent()
        {
            panel1 = new Panel();
            btnSaveLog = new Button();
            btnPath = new Button();
            tbFilePath = new TextBox();
            tbSerialNumber = new TextBox();
            tbAKT = new TextBox();
            lbFilePath = new Label();
            lbSerialNumber = new Label();
            lbAKT = new Label();
            tbFIO = new TextBox();
            lbFIO = new Label();
            fbd = new FolderBrowserDialog();
            panel1.SuspendLayout();
            SuspendLayout();
            // 
            // panel1
            // 
            panel1.Controls.Add(btnSaveLog);
            panel1.Controls.Add(btnPath);
            panel1.Controls.Add(tbFilePath);
            panel1.Controls.Add(tbSerialNumber);
            panel1.Controls.Add(tbAKT);
            panel1.Controls.Add(lbFilePath);
            panel1.Controls.Add(lbSerialNumber);
            panel1.Controls.Add(lbAKT);
            panel1.Controls.Add(tbFIO);
            panel1.Controls.Add(lbFIO);
            panel1.Dock = DockStyle.Fill;
            panel1.Location = new Point(0, 0);
            panel1.Name = "panel1";
            panel1.Padding = new Padding(12);
            panel1.Size = new Size(784, 261);
            panel1.TabIndex = 0;
            // 
            // btnSaveLog
            // 
            btnSaveLog.Location = new Point(12, 218);
            btnSaveLog.Name = "btnSaveLog";
            btnSaveLog.Size = new Size(280, 28);
            btnSaveLog.TabIndex = 9;
            btnSaveLog.Text = "Сохранить логи";
            btnSaveLog.UseVisualStyleBackColor = true;
            btnSaveLog.Click += btnSaveLog_Click;
            // 
            // btnPath
            // 
            btnPath.Location = new Point(694, 149);
            btnPath.Name = "btnPath";
            btnPath.Size = new Size(75, 26);
            btnPath.TabIndex = 8;
            btnPath.Text = "Выбрать";
            btnPath.UseVisualStyleBackColor = true;
            btnPath.Click += btnPath_Click;
            // 
            // tbFilePath
            // 
            tbFilePath.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            tbFilePath.Location = new Point(12, 152);
            tbFilePath.Name = "tbFilePath";
            tbFilePath.PlaceholderText = "Папка для Отчет_Bridge.xlsx и подпапок по серийнику";
            tbFilePath.Size = new Size(676, 23);
            tbFilePath.TabIndex = 7;
            // 
            // tbSerialNumber
            // 
            tbSerialNumber.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            tbSerialNumber.Location = new Point(12, 108);
            tbSerialNumber.Name = "tbSerialNumber";
            tbSerialNumber.PlaceholderText = "Серийный номер изделия";
            tbSerialNumber.Size = new Size(757, 23);
            tbSerialNumber.TabIndex = 6;
            // 
            // tbAKT
            // 
            tbAKT.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            tbAKT.Location = new Point(12, 64);
            tbAKT.Name = "tbAKT";
            tbAKT.PlaceholderText = "Номер акта";
            tbAKT.Size = new Size(757, 23);
            tbAKT.TabIndex = 5;
            // 
            // lbFilePath
            // 
            lbFilePath.AutoSize = true;
            lbFilePath.Location = new Point(15, 134);
            lbFilePath.Name = "lbFilePath";
            lbFilePath.Size = new Size(145, 15);
            lbFilePath.TabIndex = 4;
            lbFilePath.Text = "Путь к папке сохранения";
            // 
            // lbSerialNumber
            // 
            lbSerialNumber.AutoSize = true;
            lbSerialNumber.Location = new Point(12, 90);
            lbSerialNumber.Name = "lbSerialNumber";
            lbSerialNumber.Size = new Size(104, 15);
            lbSerialNumber.TabIndex = 3;
            lbSerialNumber.Text = "Серийный номер";
            // 
            // lbAKT
            // 
            lbAKT.AutoSize = true;
            lbAKT.Location = new Point(12, 46);
            lbAKT.Name = "lbAKT";
            lbAKT.Size = new Size(48, 15);
            lbAKT.TabIndex = 2;
            lbAKT.Text = "№ Акта";
            // 
            // tbFIO
            // 
            tbFIO.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            tbFIO.Location = new Point(12, 20);
            tbFIO.Name = "tbFIO";
            tbFIO.PlaceholderText = "Фамилия Имя Отчество";
            tbFIO.Size = new Size(757, 23);
            tbFIO.TabIndex = 1;
            // 
            // lbFIO
            // 
            lbFIO.AutoSize = true;
            lbFIO.Location = new Point(12, 2);
            lbFIO.Name = "lbFIO";
            lbFIO.Size = new Size(34, 15);
            lbFIO.TabIndex = 0;
            lbFIO.Text = "ФИО";
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(784, 261);
            Controls.Add(panel1);
            MinimumSize = new Size(640, 300);
            Name = "Form1";
            StartPosition = FormStartPosition.CenterScreen;
            Text = "Копирование логов Bridge";
            panel1.ResumeLayout(false);
            panel1.PerformLayout();
            ResumeLayout(false);
        }

        #endregion

        private Panel panel1;
        private Label lbFilePath;
        private Label lbSerialNumber;
        private Label lbAKT;
        private TextBox tbFIO;
        private Label lbFIO;
        private Button btnSaveLog;
        private Button btnPath;
        private TextBox tbFilePath;
        private TextBox tbSerialNumber;
        private TextBox tbAKT;
        private FolderBrowserDialog fbd;
    }
}
#nullable restore
