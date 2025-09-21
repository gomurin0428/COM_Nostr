using System;

namespace COM_Nostr.Internal;

internal static class RelayUriUtilities
{
    public static Uri ParseWebSocketUri(string? raw)
    {
        if (string.IsNullOrWhiteSpace(raw))
        {
            throw new ArgumentException("Relay URL must not be null or whitespace.", nameof(raw));
        }

        if (!Uri.TryCreate(raw.Trim(), UriKind.Absolute, out var uri))
        {
            throw new ArgumentException("Relay URL must be an absolute URI.", nameof(raw));
        }

        if (!string.Equals(uri.Scheme, "ws", StringComparison.OrdinalIgnoreCase) &&
            !string.Equals(uri.Scheme, "wss", StringComparison.OrdinalIgnoreCase))
        {
            throw new ArgumentException("Relay URL must use ws:// or wss:// scheme.", nameof(raw));
        }

        return uri;
    }

    public static string ToCanonicalString(Uri uri)
    {
        if (uri is null)
        {
            throw new ArgumentNullException(nameof(uri));
        }

        var scheme = uri.Scheme.ToLowerInvariant();
        var host = uri.IdnHost.ToLowerInvariant();
        var portPart = uri.IsDefaultPort ? string.Empty : $":{uri.Port}";
        var pathAndQuery = uri.GetComponents(UriComponents.PathAndQuery, UriFormat.UriEscaped);
        if (string.IsNullOrEmpty(pathAndQuery) || pathAndQuery == "/")
        {
            pathAndQuery = string.Empty;
        }

        return $"{scheme}://{host}{portPart}{pathAndQuery}";
    }

    public static string GetSessionKey(Uri uri)
    {
        return ToCanonicalString(uri);
    }

    public static Uri BuildMetadataUri(Uri websocketUri)
    {
        if (websocketUri is null)
        {
            throw new ArgumentNullException(nameof(websocketUri));
        }

        var builder = new UriBuilder(websocketUri)
        {
            Scheme = string.Equals(websocketUri.Scheme, "wss", StringComparison.OrdinalIgnoreCase) ? "https" : "http",
            Fragment = string.Empty
        };

        if (builder.Path == "/")
        {
            builder.Path = string.Empty;
        }

        return builder.Uri;
    }
}
