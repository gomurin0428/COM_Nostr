using System;
using System.Buffers;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.WebSockets;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
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
    private const int EPointer = unchecked((int)0x80004003);
    private const int ETimeout = unchecked((int)0x800705B4);

    private readonly object _syncRoot = new();
    private readonly Dictionary<string, NostrRelaySession> _relaySessions = new(StringComparer.OrdinalIgnoreCase);

    private NostrClientResources? _resources;
    private SynchronizationContext? _callbackContext;
    private bool _initialized;
    private INostrSigner? _signer;

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
        if (signer is null)
        {
            throw new COMException("Signer must not be null.", EPointer);
        }

        lock (_syncRoot)
        {
            EnsureInitializedUnsafe();
            _signer = signer;
        }
    }

    public INostrRelaySession ConnectRelay(RelayDescriptor descriptor, INostrAuthCallback authCallback)
    {
        if (descriptor is null)
        {
            throw new COMException("Relay descriptor must not be null.", EPointer);
        }

        if (authCallback is null)
        {
            throw new COMException("Auth callback must not be null.", EPointer);
        }

        var resources = EnsureResources();
        var callbackContext = CallbackContext;

        Uri relayUri;
        try
        {
            relayUri = RelayUriUtilities.ParseWebSocketUri(descriptor.Url);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }

        var key = RelayUriUtilities.GetSessionKey(relayUri);
        NostrRelaySession? session = null;

        lock (_syncRoot)
        {
            EnsureInitializedUnsafe();

            if (!_relaySessions.TryGetValue(key, out var existing))
            {
                var normalizedDescriptor = CloneDescriptor(descriptor);
                normalizedDescriptor.Url = RelayUriUtilities.ToCanonicalString(relayUri);
                var httpClient = new NostrHttpClient(resources.HttpClientFactory);
                session = new NostrRelaySession(relayUri, normalizedDescriptor, resources, httpClient, authCallback, callbackContext);
                _relaySessions[key] = session;
            }
            else
            {
                existing.SetAuthCallback(authCallback);
                existing.UpdatePolicy(descriptor);
                session = existing;
            }
        }

        var activeSession = session ?? throw new InvalidOperationException("Relay session was not created.");

        try
        {
            activeSession.Connect();
            return activeSession;
        }
        catch (OperationCanceledException)
        {
            throw new COMException($"Relay '{descriptor.Url}' connection timed out.", ETimeout);
        }
        catch (HttpRequestException ex)
        {
            throw new COMException($"Failed to fetch NIP-11 metadata for '{descriptor.Url}'. {ex.Message}", ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
        }
        catch (WebSocketException ex)
        {
            throw new COMException($"Failed to connect to relay '{descriptor.Url}'. {ex.Message}", ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
        }
        catch (COMException)
        {
            throw;
        }
        catch (Exception ex)
        {
            throw new COMException($"Failed to connect to relay '{descriptor.Url}'.", ex);
        }
    }

    public void DisconnectRelay(string relayUrl)
    {
        EnsureResources();

        var session = GetSessionOrThrow(relayUrl);
        var uri = RelayUriUtilities.ParseWebSocketUri(relayUrl);
        var key = RelayUriUtilities.GetSessionKey(uri);

        lock (_syncRoot)
        {
            _relaySessions.Remove(key);
        }

        try
        {
            session.Close();
        }
        catch (Exception ex)
        {
            throw new COMException($"Failed to close relay '{relayUrl}'.", ex);
        }
    }

    public bool HasRelay(string relayUrl)
    {
        EnsureResources();

        if (string.IsNullOrWhiteSpace(relayUrl))
        {
            throw new COMException("Relay URL must not be null or whitespace.", EInvalidarg);
        }

        Uri relayUri;
        try
        {
            relayUri = RelayUriUtilities.ParseWebSocketUri(relayUrl);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }

        var key = RelayUriUtilities.GetSessionKey(relayUri);
        lock (_syncRoot)
        {
            return _relaySessions.ContainsKey(key);
        }
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
        EnsureResources();
        var session = GetSessionOrThrow(relayUrl);

        try
        {
            session.RefreshMetadata();
        }
        catch (HttpRequestException ex)
        {
            throw new COMException($"Failed to refresh NIP-11 metadata for '{relayUrl}'. {ex.Message}", ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
        }
        catch (Exception ex)
        {
            throw new COMException($"Failed to refresh NIP-11 metadata for '{relayUrl}'.", ex);
        }
    }

    public string[] ListRelays()
    {
        EnsureResources();

        lock (_syncRoot)
        {
            return _relaySessions.Values.Select(session => session.Url).ToArray();
        }
    }

    private NostrClientResources EnsureResources()
    {
        lock (_syncRoot)
        {
            EnsureInitializedUnsafe();
            return _resources!;
        }
    }

    private void EnsureInitializedUnsafe()
    {
        if (!_initialized || _resources is null)
        {
            throw new COMException("NostrClient.Initialize must be called before use.", ComErrorCodes.E_NOSTR_NOT_INITIALIZED);
        }
    }

    private NostrRelaySession GetSessionOrThrow(string relayUrl)
    {
        if (string.IsNullOrWhiteSpace(relayUrl))
        {
            throw new COMException("Relay URL must not be null or whitespace.", EInvalidarg);
        }

        Uri relayUri;
        try
        {
            relayUri = RelayUriUtilities.ParseWebSocketUri(relayUrl);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }

        var key = RelayUriUtilities.GetSessionKey(relayUri);
        lock (_syncRoot)
        {
            if (_relaySessions.TryGetValue(key, out var session))
            {
                return session;
            }
        }

        throw new COMException($"Relay '{relayUrl}' is not registered.", ComErrorCodes.E_NOSTR_RELAY_NOT_CONNECTED);
    }

    private static RelayDescriptor CloneDescriptor(RelayDescriptor descriptor)
    {
        return new RelayDescriptor
        {
            Url = descriptor.Url,
            ReadEnabled = descriptor.ReadEnabled,
            WriteEnabled = descriptor.WriteEnabled,
            Preferred = descriptor.Preferred,
            Metadata = descriptor.Metadata
        };
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

        if (seconds <= 0)
        {
            throw new ArgumentException($"{propertyName} must be greater than zero seconds when specified.", propertyName);
        }

        return TimeSpan.FromSeconds(seconds.Value);
    }

    private static double? ExtractSeconds(object value, string propertyName)
    {
        switch (value)
        {
            case null:
                return null;
            case double doubleValue:
                return doubleValue;
            case float floatValue:
                return floatValue;
            case int intValue:
                return intValue;
            case long longValue:
                return longValue;
            case string text:
                if (string.IsNullOrWhiteSpace(text))
                {
                    return null;
                }

                if (double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
                {
                    return parsed;
                }

                throw new ArgumentException($"{propertyName} must be parsable as a number of seconds.", propertyName);
            default:
                throw new ArgumentException($"{propertyName} must be specified as a number of seconds.", propertyName);
        }
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
    private const int EInvalidarg = unchecked((int)0x80070057);
    private const int EPointer = unchecked((int)0x80004003);

    private readonly object _stateLock = new();
    private readonly Uri _webSocketUri;
    private readonly Uri _metadataUri;
    private readonly NostrClientResources _resources;
    private readonly NostrHttpClient _httpClient;
    private readonly SynchronizationContext? _callbackContext;
    private readonly BackoffPolicy _backoffPolicy;
    private readonly NostrJsonSerializer _serializer;

    private RelayDescriptor _descriptor;
    private RelaySessionState _state = RelaySessionState.Disconnected;
    private NostrOkResult _lastOkResult = new();
    private int[] _supportedNips = Array.Empty<int>();
    private IWebSocketConnection? _connection;
    private Task? _receiveLoop;
    private CancellationTokenSource? _sessionCancellation;
    private INostrAuthCallback _authCallback;
    private Exception? _lastFault;

    internal NostrRelaySession(
        Uri relayUri,
        RelayDescriptor descriptor,
        NostrClientResources resources,
        NostrHttpClient httpClient,
        INostrAuthCallback authCallback,
        SynchronizationContext? callbackContext)
    {
        _webSocketUri = relayUri ?? throw new ArgumentNullException(nameof(relayUri));
        _metadataUri = RelayUriUtilities.BuildMetadataUri(relayUri);
        _resources = resources ?? throw new ArgumentNullException(nameof(resources));
        _httpClient = httpClient ?? throw new ArgumentNullException(nameof(httpClient));
        _authCallback = authCallback ?? throw new ArgumentNullException(nameof(authCallback));
        _callbackContext = callbackContext;
        _descriptor = CloneDescriptor(descriptor ?? throw new ArgumentNullException(nameof(descriptor)));
        _descriptor.Url = RelayUriUtilities.ToCanonicalString(relayUri);
        _serializer = resources.Serializer;
        _backoffPolicy = new BackoffPolicy(TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(32));
    }

    public string Url
    {
        get
        {
            lock (_stateLock)
            {
                return _descriptor.Url;
            }
        }
    }

    public RelaySessionState State
    {
        get
        {
            lock (_stateLock)
            {
                return _state;
            }
        }
    }

    public NostrOkResult LastOkResult
    {
        get
        {
            lock (_stateLock)
            {
                return _lastOkResult;
            }
        }
    }

    public int[] SupportedNips
    {
        get
        {
            lock (_stateLock)
            {
                return _supportedNips.Length == 0 ? Array.Empty<int>() : (int[])_supportedNips.Clone();
            }
        }
    }

    public bool WriteEnabled
    {
        get
        {
            lock (_stateLock)
            {
                return _descriptor.WriteEnabled;
            }
        }
    }

    public bool ReadEnabled
    {
        get
        {
            lock (_stateLock)
            {
                return _descriptor.ReadEnabled;
            }
        }
    }

    public void Reconnect()
    {
        var delay = _backoffPolicy.GetNextDelay();
        if (delay > TimeSpan.Zero)
        {
            Task.Delay(delay).GetAwaiter().GetResult();
        }

        ConnectInternalAsync(isReconnect: true, CancellationToken.None).GetAwaiter().GetResult();
    }

    public void Close()
    {
        CloseAsync().GetAwaiter().GetResult();
    }

    public RelayDescriptor GetDescriptor()
    {
        lock (_stateLock)
        {
            return CloneDescriptor(_descriptor);
        }
    }

    public void UpdatePolicy(RelayDescriptor descriptor)
    {
        if (descriptor is null)
        {
            throw new COMException("Descriptor must not be null.", EPointer);
        }

        Uri incomingUri;
        try
        {
            incomingUri = RelayUriUtilities.ParseWebSocketUri(descriptor.Url);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }

        var canonical = RelayUriUtilities.ToCanonicalString(incomingUri);
        lock (_stateLock)
        {
            if (!string.Equals(canonical, _descriptor.Url, StringComparison.OrdinalIgnoreCase))
            {
                throw new COMException("Relay URL mismatch when updating policy.", EInvalidarg);
            }

            _descriptor.ReadEnabled = descriptor.ReadEnabled;
            _descriptor.WriteEnabled = descriptor.WriteEnabled;
            _descriptor.Preferred = descriptor.Preferred;
        }
    }

    internal void SetAuthCallback(INostrAuthCallback callback)
    {
        if (callback is null)
        {
            throw new ArgumentNullException(nameof(callback));
        }

        lock (_stateLock)
        {
            _authCallback = callback;
        }
    }

    internal void Connect()
    {
        ConnectInternalAsync(isReconnect: false, CancellationToken.None).GetAwaiter().GetResult();
    }

    internal void RefreshMetadata()
    {
        RefreshMetadataAsync(CancellationToken.None).GetAwaiter().GetResult();
    }

    private async Task ConnectInternalAsync(bool isReconnect, CancellationToken cancellationToken)
    {
        IWebSocketConnection? previousConnection;
        Task? previousLoop;
        CancellationTokenSource? previousCancellation;
        CancellationTokenSource sessionCancellation;

        lock (_stateLock)
        {
            previousConnection = _connection;
            previousLoop = _receiveLoop;
            previousCancellation = _sessionCancellation;
            _connection = null;
            _receiveLoop = null;

            sessionCancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            _sessionCancellation = sessionCancellation;
            _state = RelaySessionState.Connecting;
        }

        await CleanupPreviousAsync(previousConnection, previousLoop, previousCancellation).ConfigureAwait(false);

        try
        {
            await RefreshMetadataAsync(sessionCancellation.Token).ConfigureAwait(false);

            var connection = _resources.WebSocketFactory();
            if (connection is null)
            {
                throw new InvalidOperationException("WebSocket factory returned null connection.");
            }

            CancellationToken connectToken;
            CancellationTokenSource? connectTimeoutCts = null;
            if (_resources.Options.ConnectTimeout.HasValue)
            {
                connectTimeoutCts = CancellationTokenSource.CreateLinkedTokenSource(sessionCancellation.Token);
                connectTimeoutCts.CancelAfter(_resources.Options.ConnectTimeout.Value);
                connectToken = connectTimeoutCts.Token;
            }
            else
            {
                connectToken = sessionCancellation.Token;
            }

            try
            {
                await connection.ConnectAsync(_webSocketUri, connectToken).ConfigureAwait(false);
            }
            finally
            {
                connectTimeoutCts?.Dispose();
            }

            lock (_stateLock)
            {
                _connection = connection;
                _receiveLoop = Task.Run(() => ReceiveLoopAsync(connection, sessionCancellation.Token));
                _state = RelaySessionState.Connected;
                _backoffPolicy.Reset();
            }
        }
        catch
        {
            lock (_stateLock)
            {
                _state = RelaySessionState.Faulted;
            }

            throw;
        }
    }

    private async Task RefreshMetadataAsync(CancellationToken cancellationToken)
    {
        var info = await _httpClient.FetchRelayInformationAsync(_metadataUri, cancellationToken).ConfigureAwait(false);

        lock (_stateLock)
        {
            _supportedNips = info.SupportedNips;
            _descriptor.Metadata = info.MetadataJson;
        }
    }

    private async Task CloseAsync()
    {
        IWebSocketConnection? connection;
        Task? receiveLoop;
        CancellationTokenSource? cancellation;

        lock (_stateLock)
        {
            connection = _connection;
            receiveLoop = _receiveLoop;
            cancellation = _sessionCancellation;
            _connection = null;
            _receiveLoop = null;
            _sessionCancellation = null;
            _state = RelaySessionState.Disconnected;
        }

        await CleanupPreviousAsync(connection, receiveLoop, cancellation).ConfigureAwait(false);
    }

    private async Task ReceiveLoopAsync(IWebSocketConnection connection, CancellationToken cancellationToken)
    {
        var buffer = ArrayPool<byte>.Shared.Rent(64 * 1024);
        var messageBuffer = new ArrayBufferWriter<byte>(buffer.Length);

        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                ValueWebSocketReceiveResult result;
                try
                {
                    result = await connection.ReceiveAsync(buffer.AsMemory(), cancellationToken).ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    break;
                }

                if (result.MessageType == WebSocketMessageType.Close)
                {
                    lock (_stateLock)
                    {
                        _state = RelaySessionState.Disconnected;
                    }

                    break;
                }

                if (result.Count > 0)
                {
                    messageBuffer.Write(buffer.AsSpan(0, result.Count));
                }

                if (result.EndOfMessage)
                {
                    if (messageBuffer.WrittenCount > 0)
                    {
                        ProcessIncomingMessage(messageBuffer.WrittenSpan);
                        messageBuffer = new ArrayBufferWriter<byte>(buffer.Length);
                    }
                }
            }
        }
        catch (OperationCanceledException)
        {
            // Expected during shutdown.
        }
        catch (Exception ex)
        {
            lock (_stateLock)
            {
                _lastFault = ex;
                _state = RelaySessionState.Faulted;
            }
        }
        finally
        {
            ArrayPool<byte>.Shared.Return(buffer);
        }
    }

    private void ProcessIncomingMessage(ReadOnlySpan<byte> payload)
    {
        if (payload.IsEmpty)
        {
            return;
        }

        using var document = JsonDocument.Parse(payload.ToArray());
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Array || root.GetArrayLength() == 0)
        {
            return;
        }

        var messageType = root[0].GetString();
        if (string.Equals(messageType, "OK", StringComparison.Ordinal))
        {
            var ok = _serializer.DeserializeOk(payload);
            lock (_stateLock)
            {
                _lastOkResult = new NostrOkResult
                {
                    Success = ok.Success,
                    EventId = ok.EventId,
                    Message = ok.Message
                };
            }
        }
        else if (string.Equals(messageType, "NOTICE", StringComparison.Ordinal))
        {
            // NOTICE processing will be wired to callbacks in later phases.
        }
    }

    private async Task CleanupPreviousAsync(IWebSocketConnection? connection, Task? receiveLoop, CancellationTokenSource? cancellation)
    {
        if (cancellation is not null)
        {
            try
            {
                cancellation.Cancel();
            }
            catch
            {
                // ignored
            }
        }

        if (receiveLoop is not null)
        {
            try
            {
                await receiveLoop.ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                // expected
            }
            catch
            {
                // ignored
            }
        }

        if (connection is not null)
        {
            try
            {
                await connection.CloseOutputAsync(WebSocketCloseStatus.NormalClosure, "Client closing", CancellationToken.None).ConfigureAwait(false);
            }
            catch
            {
                // ignored
            }

            await connection.DisposeAsync().ConfigureAwait(false);
        }

        cancellation?.Dispose();
    }

    private static RelayDescriptor CloneDescriptor(RelayDescriptor descriptor)
    {
        return new RelayDescriptor
        {
            Url = descriptor.Url,
            ReadEnabled = descriptor.ReadEnabled,
            WriteEnabled = descriptor.WriteEnabled,
            Preferred = descriptor.Preferred,
            Metadata = descriptor.Metadata
        };
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



