using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BrigeLogCopy
{
    /// <summary>
    /// Сохранение снапшота логов ESP32-моста (AsyncWebServer, v2.1+) в папку серийного номера.
    ///
    /// Файлы, которые создаются (все с суффиксом _yyyyMMdd_HHmmss):
    ///   Единый_лог_*.txt       — /api/log/file       (MAVLink + ESP + статистика).
    ///   esp32_log_*.txt        — /api/log/esp32      (полный ESP-лог).
    ///   status_*.json          — /api/status         (полный снапшот метрик).
    ///   link_*.json            — /api/link           (MAVLink-канал: rx/tx/drops/hb).
    ///   clients_*.json         — /api/clients        (per-slot TCP, UDP клиент).
    ///   system_stats_*.json    — /api/system/stats   (UART bytes, RSSI, chip_temp).
    ///   mavlink_log_*.json     — /api/log            (кольцевой журнал MAVLink строк).
    ///
    /// Принцип отказоустойчивости: каждый эндпоинт качается независимо; если один упал —
    /// остальные всё равно сохраняются. Список ошибок возвращается вызывающему.
    /// </summary>
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

        /// <summary>Результат попытки снять снапшот: сколько эндпоинтов удалось сохранить и ошибки.</summary>
        public class SnapshotResult
        {
            public int SavedCount;
            public int TotalCount;
            public List<string> Errors = new List<string>();
            public bool AnySaved => SavedCount > 0;
            public bool AllSaved => SavedCount == TotalCount;
        }

        /// <param name="actFolderPath">Папка конкретного акта (уже создана); внутри неё создаётся подпапка по серийному номеру.</param>
        public static async Task<SnapshotResult> SaveLogsFromBridgeAsync(string bridgeBaseUrl, string actFolderPath, string serialRaw)
        {
            string folderName = SanitizeFolderName(serialRaw);
            string targetDir = Path.Combine(actFolderPath.Trim(), folderName);
            Directory.CreateDirectory(targetDir);

            string stamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            var utf8 = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
            var result = new SnapshotResult();

            /* (Имя файла, лейбл для ошибки, функция-загрузчик) — порядок соответствует приоритету. */
            var endpoints = new (string FileName, string Label, Func<Task<string>> Load)[]
            {
                ("Единый_лог_"     + stamp + ".txt",  "/api/log/file",    () => BridgeLogClient.DownloadUnifiedLogAsync(bridgeBaseUrl)),
                ("esp32_log_"      + stamp + ".txt",  "/api/log/esp32",   () => BridgeLogClient.DownloadEspLogAsync(bridgeBaseUrl)),
                ("status_"         + stamp + ".json", "/api/status",      () => BridgeLogClient.DownloadStatusJsonAsync(bridgeBaseUrl)),
                ("link_"           + stamp + ".json", "/api/link",        () => BridgeLogClient.DownloadLinkJsonAsync(bridgeBaseUrl)),
                ("clients_"        + stamp + ".json", "/api/clients",     () => BridgeLogClient.DownloadClientsJsonAsync(bridgeBaseUrl)),
                ("system_stats_"   + stamp + ".json", "/api/system/stats",() => BridgeLogClient.DownloadSystemStatsJsonAsync(bridgeBaseUrl)),
                ("mavlink_log_"    + stamp + ".json", "/api/log",         () => BridgeLogClient.DownloadMavlinkLogJsonAsync(bridgeBaseUrl)),
            };

            result.TotalCount = endpoints.Length;

            foreach (var (fileName, label, load) in endpoints)
            {
                try
                {
                    string content = await load().ConfigureAwait(false);
                    File.WriteAllText(Path.Combine(targetDir, fileName), content ?? "", utf8);
                    result.SavedCount++;
                }
                catch (Exception ex)
                {
                    result.Errors.Add(label + ": " + ex.Message);
                }
            }

            return result;
        }

        /// <summary>
        /// Папка серийного номера внутри папки акта существует и в ней уже есть файлы (раньше сохраняли логи).
        /// </summary>
        public static bool SerialFolderHasSavedContent(string actFolderPath, string serialRaw)
        {
            string path = Path.Combine(actFolderPath.Trim(), SanitizeFolderName(serialRaw));
            if (!Directory.Exists(path))
                return false;
            return Directory.EnumerateFileSystemEntries(path).Any();
        }
    }
}
