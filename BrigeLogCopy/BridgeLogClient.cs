using System;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace BrigeLogCopy
{
    /// <summary>
    /// Загрузка единого лога и JSON с веб-интерфейса ESP32-моста.
    /// </summary>
    public static class BridgeLogClient
    {
        private static readonly HttpClient Http = CreateClient();

        private static HttpClient CreateClient()
        {
            var c = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
            c.DefaultRequestHeaders.TryAddWithoutValidation("Accept", "*/*");
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

        public static async Task<string> DownloadUnifiedLogAsync(string baseUrl)
        {
            var url = NormalizeBaseUrl(baseUrl) + "/api/log/file";
            var bytes = await Http.GetByteArrayAsync(url).ConfigureAwait(false);
            return Encoding.UTF8.GetString(bytes);
        }

        public static async Task<string> DownloadStatusJsonAsync(string baseUrl)
        {
            var url = NormalizeBaseUrl(baseUrl) + "/api/status";
            return await Http.GetStringAsync(url).ConfigureAwait(false);
        }

        public static async Task<string> DownloadMavlinkLogJsonAsync(string baseUrl)
        {
            var url = NormalizeBaseUrl(baseUrl) + "/api/log";
            return await Http.GetStringAsync(url).ConfigureAwait(false);
        }
    }
}
