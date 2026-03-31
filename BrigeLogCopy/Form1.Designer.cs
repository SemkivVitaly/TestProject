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
            this.labelFio = new System.Windows.Forms.Label();
            this.txtFio = new System.Windows.Forms.TextBox();
            this.labelAct = new System.Windows.Forms.Label();
            this.txtActNumber = new System.Windows.Forms.TextBox();
            this.labelSerial = new System.Windows.Forms.Label();
            this.txtSerial = new System.Windows.Forms.TextBox();
            this.labelPath = new System.Windows.Forms.Label();
            this.txtReportsPath = new System.Windows.Forms.TextBox();
            this.btnBrowseFolder = new System.Windows.Forms.Button();
            this.btnSaveLogs = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // labelFio
            // 
            this.labelFio.AutoSize = true;
            this.labelFio.Location = new System.Drawing.Point(18, 18);
            this.labelFio.Name = "labelFio";
            this.labelFio.Size = new System.Drawing.Size(103, 15);
            this.labelFio.TabIndex = 0;
            this.labelFio.Text = "ФИО сотрудника:";
            // 
            // txtFio
            // 
            this.txtFio.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtFio.Location = new System.Drawing.Point(200, 15);
            this.txtFio.Name = "txtFio";
            this.txtFio.Size = new System.Drawing.Size(420, 23);
            this.txtFio.TabIndex = 1;
            // 
            // labelAct
            // 
            this.labelAct.AutoSize = true;
            this.labelAct.Location = new System.Drawing.Point(18, 54);
            this.labelAct.Name = "labelAct";
            this.labelAct.Size = new System.Drawing.Size(51, 15);
            this.labelAct.TabIndex = 2;
            this.labelAct.Text = "№ Акта:";
            // 
            // txtActNumber
            // 
            this.txtActNumber.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtActNumber.Location = new System.Drawing.Point(200, 51);
            this.txtActNumber.Name = "txtActNumber";
            this.txtActNumber.Size = new System.Drawing.Size(420, 23);
            this.txtActNumber.TabIndex = 3;
            // 
            // labelSerial
            // 
            this.labelSerial.AutoSize = true;
            this.labelSerial.Location = new System.Drawing.Point(18, 90);
            this.labelSerial.Name = "labelSerial";
            this.labelSerial.Size = new System.Drawing.Size(107, 15);
            this.labelSerial.TabIndex = 4;
            this.labelSerial.Text = "Серийный номер:";
            // 
            // txtSerial
            // 
            this.txtSerial.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtSerial.Location = new System.Drawing.Point(200, 87);
            this.txtSerial.Name = "txtSerial";
            this.txtSerial.Size = new System.Drawing.Size(420, 23);
            this.txtSerial.TabIndex = 5;
            // 
            // labelPath
            // 
            this.labelPath.AutoSize = true;
            this.labelPath.Location = new System.Drawing.Point(18, 126);
            this.labelPath.Name = "labelPath";
            this.labelPath.Size = new System.Drawing.Size(163, 15);
            this.labelPath.TabIndex = 6;
            this.labelPath.Text = "Корневая папка (внутри — папка акта):";
            // 
            // txtReportsPath
            // 
            this.txtReportsPath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtReportsPath.Location = new System.Drawing.Point(200, 123);
            this.txtReportsPath.Name = "txtReportsPath";
            this.txtReportsPath.Size = new System.Drawing.Size(310, 23);
            this.txtReportsPath.TabIndex = 7;
            // 
            // btnBrowseFolder
            // 
            this.btnBrowseFolder.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnBrowseFolder.Location = new System.Drawing.Point(516, 122);
            this.btnBrowseFolder.Name = "btnBrowseFolder";
            this.btnBrowseFolder.Size = new System.Drawing.Size(104, 25);
            this.btnBrowseFolder.TabIndex = 8;
            this.btnBrowseFolder.Text = "Обзор…";
            this.btnBrowseFolder.UseVisualStyleBackColor = true;
            this.btnBrowseFolder.Click += new System.EventHandler(this.btnBrowseFolder_Click);
            // 
            // btnSaveLogs
            // 
            this.btnSaveLogs.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.btnSaveLogs.Font = new System.Drawing.Font("Segoe UI", 10F, System.Drawing.FontStyle.Bold);
            this.btnSaveLogs.Location = new System.Drawing.Point(18, 168);
            this.btnSaveLogs.Name = "btnSaveLogs";
            this.btnSaveLogs.Size = new System.Drawing.Size(602, 40);
            this.btnSaveLogs.TabIndex = 9;
            this.btnSaveLogs.Text = "Сохранить логи";
            this.btnSaveLogs.UseVisualStyleBackColor = true;
            this.btnSaveLogs.Click += new System.EventHandler(this.btnSaveLogs_Click);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(644, 271);
            this.Controls.Add(this.btnSaveLogs);
            this.Controls.Add(this.btnBrowseFolder);
            this.Controls.Add(this.txtReportsPath);
            this.Controls.Add(this.labelPath);
            this.Controls.Add(this.txtSerial);
            this.Controls.Add(this.labelSerial);
            this.Controls.Add(this.txtActNumber);
            this.Controls.Add(this.labelAct);
            this.Controls.Add(this.txtFio);
            this.Controls.Add(this.labelFio);
            this.Font = new System.Drawing.Font("Segoe UI", 9F);
            this.MinimumSize = new System.Drawing.Size(520, 310);
            this.Name = "Form1";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "BrigeLogCopy — отчёт Bridge";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label labelFio;
        private System.Windows.Forms.TextBox txtFio;
        private System.Windows.Forms.Label labelAct;
        private System.Windows.Forms.TextBox txtActNumber;
        private System.Windows.Forms.Label labelSerial;
        private System.Windows.Forms.TextBox txtSerial;
        private System.Windows.Forms.Label labelPath;
        private System.Windows.Forms.TextBox txtReportsPath;
        private System.Windows.Forms.Button btnBrowseFolder;
        private System.Windows.Forms.Button btnSaveLogs;
    }
}
