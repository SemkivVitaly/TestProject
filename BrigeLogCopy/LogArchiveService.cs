using System;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace BrigeLogCopy
{
    public static class LogArchiveService
    {
        /// <summary>Безопасное имя папки для № акта или серийного номера.</summary>
        public static string SanitizeFolderName(string name)
        {
            if (string.IsNullOrWhiteSpace(name))
                return "Без_имени";
            var inv = Path.GetInvalidFileNameChars();
            var chars = name.Trim().ToCharArray();
            for (int i = 0; i < chars.Length; i++)
            {
                if (Array.IndexOf(inv, chars[i]) >= 0)
                    chars[i] = '_';
            }
            return new string(chars);
        }

        /// <param name="actFolderPath">Папка конкретного акта (уже создана); внутри неё создаётся подпапка по серийному номеру.</param>
        public static async Task SaveLogsFromBridgeAsync(string bridgeBaseUrl, string actFolderPath, string serialRaw)
        {
            string folderName = SanitizeFolderName(serialRaw);
            string targetDir = Path.Combine(actFolderPath.Trim(), folderName);
            Directory.CreateDirectory(targetDir);

            string stamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");

            string unified = await BridgeLogClient.DownloadUnifiedLogAsync(bridgeBaseUrl).ConfigureAwait(false);
            File.WriteAllText(Path.Combine(targetDir, "Единый_лог_" + stamp + ".txt"), unified, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

            string statusJson = await BridgeLogClient.DownloadStatusJsonAsync(bridgeBaseUrl).ConfigureAwait(false);
            File.WriteAllText(Path.Combine(targetDir, "status_" + stamp + ".json"), statusJson, new UTF8Encoding(false));

            string mavlinkJson = await BridgeLogClient.DownloadMavlinkLogJsonAsync(bridgeBaseUrl).ConfigureAwait(false);
            File.WriteAllText(Path.Combine(targetDir, "mavlink_log_" + stamp + ".json"), mavlinkJson, new UTF8Encoding(false));
        }
    }
}
