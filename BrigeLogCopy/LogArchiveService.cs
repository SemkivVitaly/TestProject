using System;
using System.Collections.Generic;
using System.Configuration;
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

        /// <summary>Папка для «быстрого» экспорта (Единый лог.txt, Esp32Log.txt, Json.txt).</summary>
        public static string ResolveLogExportDirectory()
        {
            string raw = ConfigurationManager.AppSettings["LogExportDirectory"];
            if (string.IsNullOrWhiteSpace(raw))
            {
                return Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                    "Downloads", "Telegram Desktop");
            }
            return Environment.ExpandEnvironmentVariables(raw.Trim());
        }

        public static bool IsLogExportEnabled()
        {
            string v = ConfigurationManager.AppSettings["LogExportEnabled"];
            if (string.IsNullOrWhiteSpace(v))
                return true;
            v = v.Trim();
            return !string.Equals(v, "false", StringComparison.OrdinalIgnoreCase) && v != "0";
        }

        /// <summary>«Единый лог.txt» или «Единый лог 2.txt», если первый уже занят.</summary>
        public static string AllocateUnifiedLogExportPath(string directory)
        {
            Directory.CreateDirectory(directory);
            string first = Path.Combine(directory, "Единый лог.txt");
            if (!File.Exists(first))
                return first;
            for (int i = 2; i < 1000; i++)
            {
                string p = Path.Combine(directory, "Единый лог " + i + ".txt");
                if (!File.Exists(p))
                    return p;
            }
            return Path.Combine(directory, "Единый лог " + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".txt");
        }

        /// <summary>Результат дополнительного сохранения в LogExportDirectory.</summary>
        public class QuickExportResult
        {
            public string UnifiedLogPath;
            public string Esp32LogPath;
            public string MavlinkJsonPath;
            public readonly List<string> Errors = new List<string>();
            public bool AnyOk =>
                !string.IsNullOrEmpty(UnifiedLogPath)
                || !string.IsNullOrEmpty(Esp32LogPath)
                || !string.IsNullOrEmpty(MavlinkJsonPath);
        }

        /// <summary>
        /// Сохраняет три файла в стиле «Telegram Desktop»:
        /// Единый лог(.txt / … 2.txt), Esp32Log.txt, Json.txt (лог пакетов MAVLink с /api/log).
        /// </summary>
        public static async Task<QuickExportResult> ExportQuickLogsToFolderAsync(string bridgeBaseUrl, string exportDirectory)
        {
            var result = new QuickExportResult();
            var utf8 = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
            Directory.CreateDirectory(exportDirectory);

            try
            {
                string uPath = AllocateUnifiedLogExportPath(exportDirectory);
                string content = await BridgeLogClient.DownloadUnifiedLogAsync(bridgeBaseUrl).ConfigureAwait(false);
                File.WriteAllText(uPath, content ?? "", utf8);
                result.UnifiedLogPath = uPath;
            }
            catch (Exception ex)
            {
                result.Errors.Add("Единый лог: " + ex.Message);
            }

            try
            {
                string p = Path.Combine(exportDirectory, "Esp32Log.txt");
                string content = await BridgeLogClient.DownloadEspLogAsync(bridgeBaseUrl).ConfigureAwait(false);
                File.WriteAllText(p, content ?? "", utf8);
                result.Esp32LogPath = p;
            }
            catch (Exception ex)
            {
                result.Errors.Add("Esp32Log.txt: " + ex.Message);
            }

            try
            {
                string p = Path.Combine(exportDirectory, "Json.txt");
                string content = await BridgeLogClient.DownloadMavlinkLogJsonAsync(bridgeBaseUrl).ConfigureAwait(false);
                File.WriteAllText(p, content ?? "", utf8);
                result.MavlinkJsonPath = p;
            }
            catch (Exception ex)
            {
                result.Errors.Add("Json.txt: " + ex.Message);
            }

            return result;
        }
    }
}
