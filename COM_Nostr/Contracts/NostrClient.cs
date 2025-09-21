using System;
using System.Globalization;
using System.Net;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Threading;
using COM_Nostr.Internal;

namespace COM_Nostr.Contracts;

[ComVisible(true)]
[Guid("7d3091fe-ca18-49ba-835c-012991076660")]
[ClassInterface(ClassInterfaceType.None)]
[ProgId("COM_Nostr.NostrClient")]
public sealed class NostrClient : INostrClient
{
    private const string DefaultUserAgent = "COM_Nostr/1.0";
    private const int EInvalidarg = unchecked((int)0x80070057);

    private readonly object _syncRoot = new();

    private NostrClientResources? _resources;
    private SynchronizationContext? _callbackContext;
    private bool _initialized;

    internal bool IsInitialized
    {
        get
        {
            lock (_syncRoot)
            {
                return _initialized;
            }
        }
    }

    internal NostrClientResources? CurrentResources
    {
        get
        {
            lock (_syncRoot)
            {
                return _resources;
            }
        }
    }

    internal SynchronizationContext? CallbackContext
    {
        get
        {
            lock (_syncRoot)
            {
                return _callbackContext;
            }
        }
    }

    public void Initialize(ClientOptions options)
    {
        var capturedContext = SynchronizationContext.Current;

        try
        {
            var runtimeOptions = BuildRuntimeOptions(options);
            var httpClientFactory = CreateHttpClientFactory(runtimeOptions);
            var webSocketFactory = WebSocketFactoryResolver.Create(runtimeOptions);
            var serializer = new NostrJsonSerializer();
            var resources = new NostrClientResources(runtimeOptions, httpClientFactory, webSocketFactory, serializer);

            lock (_syncRoot)
            {
                _resources = resources;
                _callbackContext = capturedContext;
                _initialized = true;
            }
        }
        catch (COMException)
        {
            throw;
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }
        catch (InvalidOperationException ex)
        {
            throw new COMException(ex.Message, ex);
        }
        catch (Exception ex)
        {
            throw new COMException("Failed to initialize NostrClient.", ex);
        }
    }

    public void SetSigner(INostrSigner signer)
    {
        throw new NotImplementedException();
    }

    public INostrRelaySession ConnectRelay(RelayDescriptor descriptor, INostrAuthCallback authCallback)
    {
        throw new NotImplementedException();
    }

    public void DisconnectRelay(string relayUrl)
    {
        throw new NotImplementedException();
    }

    public bool HasRelay(string relayUrl)
    {
        throw new NotImplementedException();
    }

    public INostrSubscription OpenSubscription(string relayUrl, NostrFilter[] filters, INostrEventCallback callback, SubscriptionOptions options)
    {
        throw new NotImplementedException();
    }

    public void PublishEvent(string relayUrl, NostrEvent eventPayload)
    {
        throw new NotImplementedException();
    }

    public void RespondAuth(string relayUrl, NostrEvent authEvent)
    {
        throw new NotImplementedException();
    }

    public void RefreshRelayInfo(string relayUrl)
    {
        throw new NotImplementedException();
    }

    public string[] ListRelays()
    {
        throw new NotImplementedException();
    }

    private static ClientRuntimeOptions BuildRuntimeOptions(ClientOptions? options)
    {
        var connectTimeout = ConvertToPositiveTimeSpan(options?.ConnectTimeoutSeconds, nameof(ClientOptions.ConnectTimeoutSeconds));
        var sendTimeout = ConvertToPositiveTimeSpan(options?.SendTimeoutSeconds, nameof(ClientOptions.SendTimeoutSeconds));
        var receiveTimeout = ConvertToPositiveTimeSpan(options?.ReceiveTimeoutSeconds, nameof(ClientOptions.ReceiveTimeoutSeconds));
        var userAgent = DetermineUserAgent(options?.UserAgent);
        var progId = NormalizeText(options?.WebSocketFactoryProgId);

        return new ClientRuntimeOptions
        {
            ConnectTimeout = connectTimeout,
            SendTimeout = sendTimeout,
            ReceiveTimeout = receiveTimeout,
            UserAgent = userAgent,
            WebSocketFactoryProgId = progId
        };
    }

    private static TimeSpan? ConvertToPositiveTimeSpan(object? value, string propertyName)
    {
        if (value is null)
        {
            return null;
        }

        var seconds = ExtractSeconds(value, propertyName);
        if (!seconds.HasValue)
        {
            return null;
        }

        var numeric = seconds.Value;
        if (double.IsNaN(numeric) || double.IsInfinity(numeric))
        {
            throw new ArgumentOutOfRangeException(propertyName, "Timeout seconds must be a finite value.");
        }

        if (numeric <= 0)
        {
            throw new ArgumentOutOfRangeException(propertyName, "Timeout seconds must be greater than zero.");
        }

        if (numeric > TimeSpan.MaxValue.TotalSeconds)
        {
            throw new ArgumentOutOfRangeException(propertyName, "Timeout seconds exceed supported range.");
        }

        return TimeSpan.FromSeconds(numeric);
    }

    private static double? ExtractSeconds(object value, string propertyName)
    {
        switch (value)
        {
            case double d:
                return d;
            case float f:
                return f;
            case decimal m:
                return (double)m;
            case int i:
                return i;
            case long l:
                return l;
            case short s:
                return s;
            case byte b:
                return b;
            case uint ui:
                return ui;
            case ulong ul:
                if (ul > long.MaxValue)
                {
                    throw new ArgumentOutOfRangeException(propertyName, "Timeout seconds exceed supported range.");
                }
                return ul;
            case string text:
                var trimmed = text.Trim();
                if (trimmed.Length == 0)
                {
                    return null;
                }

                if (!double.TryParse(trimmed, NumberStyles.Float | NumberStyles.AllowThousands, CultureInfo.InvariantCulture, out var parsed))
                {
                    throw new ArgumentException($"ClientOptions.{propertyName} must be numeric seconds.", propertyName);
                }

                return parsed;
        }

        if (value is IConvertible convertible)
        {
            try
            {
                return convertible.ToDouble(CultureInfo.InvariantCulture);
            }
            catch (Exception ex) when (ex is FormatException or InvalidCastException or OverflowException)
            {
                throw new ArgumentException($"ClientOptions.{propertyName} must be numeric seconds.", propertyName);
            }
        }

        throw new ArgumentException($"ClientOptions.{propertyName} must be numeric seconds.", propertyName);
    }

    private static string DetermineUserAgent(string? userAgent)
    {
        var normalized = NormalizeText(userAgent);
        var result = string.IsNullOrEmpty(normalized) ? DefaultUserAgent : normalized;
        ValidateUserAgent(result);
        return result;
    }

    private static string? NormalizeText(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return null;
        }

        return value.Trim();
    }

    private static void ValidateUserAgent(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            throw new ArgumentException("ClientOptions.UserAgent must not be empty after normalization.", nameof(ClientOptions.UserAgent));
        }

        for (var i = 0; i < value.Length; i++)
        {
            if (char.IsControl(value[i]))
            {
                throw new ArgumentException("ClientOptions.UserAgent must not contain control characters.", nameof(ClientOptions.UserAgent));
            }
        }
    }

    private static Func<HttpClient> CreateHttpClientFactory(ClientRuntimeOptions options)
    {
        return () =>
        {
            var handler = new HttpClientHandler
            {
                AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate | DecompressionMethods.Brotli,
                UseCookies = false
            };

            var client = new HttpClient(handler, disposeHandler: true);
            if (!string.IsNullOrEmpty(options.UserAgent))
            {
                try
                {
                    client.DefaultRequestHeaders.UserAgent.ParseAdd(options.UserAgent);
                }
                catch (FormatException ex)
                {
                    client.Dispose();
                    throw new ArgumentException("ClientOptions.UserAgent is not a valid HTTP User-Agent string.", nameof(ClientOptions.UserAgent), ex);
                }
            }

            if (options.ReceiveTimeout.HasValue)
            {
                client.Timeout = options.ReceiveTimeout.Value;
            }

            return client;
        };
    }
}

[ComVisible(true)]
[Guid("e53e9b56-da8d-4064-8df6-5563708f65a5")]
[ClassInterface(ClassInterfaceType.None)]
public sealed class NostrRelaySession : INostrRelaySession
{
    public string Url => throw new NotImplementedException();

    public RelaySessionState State => throw new NotImplementedException();

    public NostrOkResult LastOkResult => throw new NotImplementedException();

    public int[] SupportedNips => throw new NotImplementedException();

    public bool WriteEnabled => throw new NotImplementedException();

    public bool ReadEnabled => throw new NotImplementedException();

    public void Reconnect()
    {
        throw new NotImplementedException();
    }

    public void Close()
    {
        throw new NotImplementedException();
    }

    public RelayDescriptor GetDescriptor()
    {
        throw new NotImplementedException();
    }

    public void UpdatePolicy(RelayDescriptor descriptor)
    {
        throw new NotImplementedException();
    }
}

[ComVisible(true)]
[Guid("175bd625-18d9-42bd-b75a-0642abf029b4")]
[ClassInterface(ClassInterfaceType.None)]
public sealed class NostrSubscription : INostrSubscription
{
    public string Id => throw new NotImplementedException();

    public SubscriptionStatus Status => throw new NotImplementedException();

    public NostrFilter[] Filters => throw new NotImplementedException();

    public void UpdateFilters(NostrFilter[] filters)
    {
        throw new NotImplementedException();
    }

    public void Close()
    {
        throw new NotImplementedException();
    }
}
