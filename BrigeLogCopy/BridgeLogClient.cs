using System;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace BrigeLogCopy
{
    /// <summary>
    /// Загрузка логов и JSON с веб-интерфейса ESP32-моста (AsyncWebServer, v2.1+).
    ///
    /// Эндпоинты прошивки:
    ///   GET /api/log/file      — сводный лог (text/plain utf-8): MAVLink + ESP + статистика.
    ///   GET /api/log           — кольцевой журнал MAVLink (JSON-массив строк).
    ///   GET /api/log/esp32     — лог ESP32 (text/plain utf-8).
    ///   GET /api/status        — полный снапшот метрик: uptime, heap, chip_temp, MAVLink-счётчики
    ///                            (rx_pkts / rx_lost / parse_err / loss_pct), uart_bytes_*, net_bytes_*,
    ///                            rssi_now/min/max/avg, счётчики подключений, SERVO-параметры и MAVLink log.
    ///   GET /api/link          — компактный JSON канала MAVLink: packets_received/sent, packet_drops,
    ///                            packet_loss_pct, heartbeat_age_ms/interval_ms, net bytes.
    ///   GET /api/clients       — per-slot статистика TCP и текущий UDP-клиент (bytes_in/out, queue_used).
    ///   GET /api/system/stats  — UART bytes, RSSI, tcp/udp клиенты, chip_temp (совместимо со старым UI).
    /// </summary>
    public static class BridgeLogClient
    {
        /* Таймаут загрузки одного эндпоинта. Основной ограничитель — /api/log/file (~3 КБ) и /api/log (~2 КБ);
         * оба с большим запасом укладываются в 15 с даже по слабому Wi-Fi. */
        private static readonly HttpClient Http = CreateClient();

        private static HttpClient CreateClient()
        {
            var c = new HttpClient { Timeout = TimeSpan.FromSeconds(15) };
            c.DefaultRequestHeaders.TryAddWithoutValidation("Accept", "*/*");
            c.DefaultRequestHeaders.TryAddWithoutValidation("Cache-Control", "no-cache");
            return c;
        }

        public static string NormalizeBaseUrl(string baseUrl)
        {
            if (string.IsNullOrWhiteSpace(baseUrl))
                return "http://192.168.2.1";
            var u = baseUrl.Trim().TrimEnd('/');
            if (!u.StartsWith("http://", StringComparison.OrdinalIgnoreCase) &&
                !u.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
                u = "http://" + u;
            return u;
        }

        /* ===== Текстовые (utf-8) ===== */

        public static Task<string> DownloadUnifiedLogAsync(string baseUrl) =>
            DownloadTextAsync(baseUrl, "/api/log/file");

        public static Task<string> DownloadEspLogAsync(string baseUrl) =>
            DownloadTextAsync(baseUrl, "/api/log/esp32");

        /* ===== JSON ===== */

        public static Task<string> DownloadStatusJsonAsync(string baseUrl) =>
            DownloadTextAsync(baseUrl, "/api/status");

        public static Task<string> DownloadMavlinkLogJsonAsync(string baseUrl) =>
            DownloadTextAsync(baseUrl, "/api/log");

        public static Task<string> DownloadLinkJsonAsync(string baseUrl) =>
            DownloadTextAsync(baseUrl, "/api/link");

        public static Task<string> DownloadClientsJsonAsync(string baseUrl) =>
            DownloadTextAsync(baseUrl, "/api/clients");

        public static Task<string> DownloadSystemStatsJsonAsync(string baseUrl) =>
            DownloadTextAsync(baseUrl, "/api/system/stats");

        /// <summary>
        /// Общий метод: читает тело как UTF-8 (независимо от Content-Type) и возвращает строку.
        /// Для text/plain это сам лог, для application/json — JSON-текст.
        /// </summary>
        private static async Task<string> DownloadTextAsync(string baseUrl, string path)
        {
            var url = NormalizeBaseUrl(baseUrl) + path;
            var bytes = await Http.GetByteArrayAsync(url).ConfigureAwait(false);
            return Encoding.UTF8.GetString(bytes);
        }
    }
}
