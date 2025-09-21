using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace COM_Nostr.Internal;

internal sealed class NostrHttpClient
{
    private readonly Func<HttpClient> _httpClientFactory;

    public NostrHttpClient(Func<HttpClient> httpClientFactory)
    {
        _httpClientFactory = httpClientFactory ?? throw new ArgumentNullException(nameof(httpClientFactory));
    }

    public async Task<NostrRelayInformation> FetchRelayInformationAsync(Uri metadataUri, CancellationToken cancellationToken)
    {
        if (metadataUri is null)
        {
            throw new ArgumentNullException(nameof(metadataUri));
        }

        using var client = _httpClientFactory();
        using var request = new HttpRequestMessage(HttpMethod.Get, metadataUri);
        request.Headers.Accept.Clear();
        request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/nostr+json"));

        using var response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, cancellationToken).ConfigureAwait(false);
        response.EnsureSuccessStatusCode();

        var content = await response.Content.ReadAsStringAsync(cancellationToken).ConfigureAwait(false);
        ValidateContentType(response.Content.Headers.ContentType?.MediaType, content);
        var supportedNips = ParseSupportedNips(content);

        return new NostrRelayInformation(content, supportedNips);
    }

    private static void ValidateContentType(string? mediaType, string body)
    {
        if (string.IsNullOrEmpty(mediaType))
        {
            return;
        }

        if (string.Equals(mediaType, "application/nostr+json", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(mediaType, "application/json", StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        throw new InvalidOperationException($"Relay returned unsupported content type '{mediaType}'. Body length={body?.Length ?? 0}.");
    }

    private static int[] ParseSupportedNips(string json)
    {
        using var document = JsonDocument.Parse(json);
        if (!document.RootElement.TryGetProperty("supported_nips", out var supportedElement) || supportedElement.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<int>();
        }

        var values = new List<int>(supportedElement.GetArrayLength());
        foreach (var element in supportedElement.EnumerateArray())
        {
            if (element.ValueKind == JsonValueKind.Number && element.TryGetInt32(out var value))
            {
                values.Add(value);
            }
        }

        return values.ToArray();
    }
}
