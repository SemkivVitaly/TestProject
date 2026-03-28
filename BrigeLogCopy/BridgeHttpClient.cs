using System.Net.Http;
using System.Text;

namespace BrigeLogCopy;

/// <summary>
/// Загрузка единого лога и JSON лога с веб‑интерфейса ESP32 Bridge.
/// </summary>
internal static class BridgeHttpClient
{
    /// <summary>
    /// Базовый URL по умолчанию (режим AP прошивки). Можно переопределить переменной окружения BRIDGE_BASE_URL.
    /// </summary>
    internal static string DefaultBaseUrl =>
        Environment.GetEnvironmentVariable("BRIDGE_BASE_URL")?.Trim().TrimEnd('/') ?? "http://192.168.2.1";

    internal static async Task<(bool Ok, string? Text, string? Error)> GetTextAsync(string baseUrl, string relativePath, CancellationToken ct)
    {
        var url = $"{baseUrl.TrimEnd('/')}{relativePath}";
        try
        {
            using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(45) };
            var bytes = await client.GetByteArrayAsync(url, ct).ConfigureAwait(false);
            var text = Encoding.UTF8.GetString(bytes);
            return (true, text, null);
        }
        catch (Exception ex)
        {
            return (false, null, ex.Message);
        }
    }
}
