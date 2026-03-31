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
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle1 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
            this.labelFio = new System.Windows.Forms.Label();
            this.txtFio = new System.Windows.Forms.TextBox();
            this.labelAct = new System.Windows.Forms.Label();
            this.txtActNumber = new System.Windows.Forms.TextBox();
            this.labelPath = new System.Windows.Forms.Label();
            this.txtReportsPath = new System.Windows.Forms.TextBox();
            this.btnBrowseFolder = new System.Windows.Forms.Button();
            this.labelQueue = new System.Windows.Forms.Label();
            this.gridSerials = new System.Windows.Forms.DataGridView();
            this.colSerial = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.btnSaveLogs = new System.Windows.Forms.Button();
            ((System.ComponentModel.ISupportInitialize)(this.gridSerials)).BeginInit();
            this.SuspendLayout();
            // 
            // labelFio
            // 
            this.labelFio.AutoSize = true;
            this.labelFio.Location = new System.Drawing.Point(18, 15);
            this.labelFio.Name = "labelFio";
            this.labelFio.Size = new System.Drawing.Size(103, 15);
            this.labelFio.TabIndex = 0;
            this.labelFio.Text = "ФИО сотрудника:";
            // 
            // txtFio
            // 
            this.txtFio.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtFio.Location = new System.Drawing.Point(200, 12);
            this.txtFio.Name = "txtFio";
            this.txtFio.Size = new System.Drawing.Size(420, 23);
            this.txtFio.TabIndex = 1;
            // 
            // labelAct
            // 
            this.labelAct.AutoSize = true;
            this.labelAct.Location = new System.Drawing.Point(18, 48);
            this.labelAct.Name = "labelAct";
            this.labelAct.Size = new System.Drawing.Size(51, 15);
            this.labelAct.TabIndex = 2;
            this.labelAct.Text = "№ Акта:";
            // 
            // txtActNumber
            // 
            this.txtActNumber.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtActNumber.Location = new System.Drawing.Point(200, 45);
            this.txtActNumber.Name = "txtActNumber";
            this.txtActNumber.Size = new System.Drawing.Size(420, 23);
            this.txtActNumber.TabIndex = 3;
            // 
            // labelPath
            // 
            this.labelPath.AutoSize = true;
            this.labelPath.Location = new System.Drawing.Point(18, 81);
            this.labelPath.MaximumSize = new System.Drawing.Size(180, 0);
            this.labelPath.Name = "labelPath";
            this.labelPath.Size = new System.Drawing.Size(163, 30);
            this.labelPath.TabIndex = 4;
            this.labelPath.Text = "Корневая папка (внутри — папка акта):";
            // 
            // txtReportsPath
            // 
            this.txtReportsPath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtReportsPath.Location = new System.Drawing.Point(200, 78);
            this.txtReportsPath.Name = "txtReportsPath";
            this.txtReportsPath.Size = new System.Drawing.Size(310, 23);
            this.txtReportsPath.TabIndex = 5;
            // 
            // btnBrowseFolder
            // 
            this.btnBrowseFolder.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnBrowseFolder.Location = new System.Drawing.Point(516, 77);
            this.btnBrowseFolder.Name = "btnBrowseFolder";
            this.btnBrowseFolder.Size = new System.Drawing.Size(104, 25);
            this.btnBrowseFolder.TabIndex = 6;
            this.btnBrowseFolder.Text = "Обзор…";
            this.btnBrowseFolder.UseVisualStyleBackColor = true;
            this.btnBrowseFolder.Click += new System.EventHandler(this.btnBrowseFolder_Click);
            // 
            // labelQueue
            // 
            this.labelQueue.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
            | System.Windows.Forms.AnchorStyles.Right)));
            this.labelQueue.Location = new System.Drawing.Point(18, 114);
            this.labelQueue.Name = "labelQueue";
            this.labelQueue.Size = new System.Drawing.Size(602, 32);
            this.labelQueue.TabIndex = 7;
            this.labelQueue.Text = "Очередь серийных номеров: верхняя строка обрабатывается при «Сохранить логи».";
            // 
            // gridSerials
            // 
            this.gridSerials.AllowUserToResizeRows = false;
            this.gridSerials.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
            | System.Windows.Forms.AnchorStyles.Left)
            | System.Windows.Forms.AnchorStyles.Right)));
            this.gridSerials.BackgroundColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle1.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle1.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle1.Font = new System.Drawing.Font("Segoe UI", 9F);
            dataGridViewCellStyle1.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle1.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle1.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle1.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.gridSerials.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle1;
            this.gridSerials.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.gridSerials.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.colSerial});
            dataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle2.Font = new System.Drawing.Font("Segoe UI", 9F);
            dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.gridSerials.DefaultCellStyle = dataGridViewCellStyle2;
            this.gridSerials.Location = new System.Drawing.Point(18, 148);
            this.gridSerials.Name = "gridSerials";
            this.gridSerials.RowHeadersWidth = 28;
            this.gridSerials.RowTemplate.Height = 25;
            this.gridSerials.Size = new System.Drawing.Size(602, 288);
            this.gridSerials.TabIndex = 8;
            // 
            // colSerial
            // 
            this.colSerial.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
            this.colSerial.HeaderText = "Серийный номер";
            this.colSerial.MinimumWidth = 120;
            this.colSerial.Name = "colSerial";
            this.colSerial.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.NotSortable;
            // 
            // btnSaveLogs
            // 
            this.btnSaveLogs.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)
            | System.Windows.Forms.AnchorStyles.Right)));
            this.btnSaveLogs.Font = new System.Drawing.Font("Segoe UI", 10F, System.Drawing.FontStyle.Bold);
            this.btnSaveLogs.Location = new System.Drawing.Point(18, 448);
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
            this.ClientSize = new System.Drawing.Size(644, 506);
            this.Controls.Add(this.btnSaveLogs);
            this.Controls.Add(this.gridSerials);
            this.Controls.Add(this.labelQueue);
            this.Controls.Add(this.btnBrowseFolder);
            this.Controls.Add(this.txtReportsPath);
            this.Controls.Add(this.labelPath);
            this.Controls.Add(this.txtActNumber);
            this.Controls.Add(this.labelAct);
            this.Controls.Add(this.txtFio);
            this.Controls.Add(this.labelFio);
            this.Font = new System.Drawing.Font("Segoe UI", 9F);
            this.MinimumSize = new System.Drawing.Size(560, 420);
            this.Name = "Form1";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "BrigeLogCopy — отчёт Bridge";
            this.Load += new System.EventHandler(this.Form1_Load);
            ((System.ComponentModel.ISupportInitialize)(this.gridSerials)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label labelFio;
        private System.Windows.Forms.TextBox txtFio;
        private System.Windows.Forms.Label labelAct;
        private System.Windows.Forms.TextBox txtActNumber;
        private System.Windows.Forms.Label labelPath;
        private System.Windows.Forms.TextBox txtReportsPath;
        private System.Windows.Forms.Button btnBrowseFolder;
        private System.Windows.Forms.Label labelQueue;
        private System.Windows.Forms.DataGridView gridSerials;
        private System.Windows.Forms.DataGridViewTextBoxColumn colSerial;
        private System.Windows.Forms.Button btnSaveLogs;
    }
}
