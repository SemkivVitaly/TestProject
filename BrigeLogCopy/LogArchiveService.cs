using System;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace BrigeLogCopy
{
    public static class LogArchiveService
    {
        public static string SanitizeFolderName(string serial)
        {
            if (string.IsNullOrWhiteSpace(serial))
                return "Без_серийного_номера";
            var inv = Path.GetInvalidFileNameChars();
            var chars = serial.Trim().ToCharArray();
            for (int i = 0; i < chars.Length; i++)
            {
                if (Array.IndexOf(inv, chars[i]) >= 0)
                    chars[i] = '_';
            }
            return new string(chars);
        }

        public static async Task SaveLogsFromBridgeAsync(string bridgeBaseUrl, string reportsRootPath, string serialRaw)
        {
            string folderName = SanitizeFolderName(serialRaw);
            string targetDir = Path.Combine(reportsRootPath.Trim(), folderName);
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
