using System;
using System.Buffers;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.WebSockets;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
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
    private static readonly TimeSpan DefaultPublishAckTimeout = TimeSpan.FromSeconds(10);
    private const int PublishRetryAttempts = 3;

    private readonly object _syncRoot = new();
    private readonly Dictionary<string, NostrRelaySession> _relaySessions = new(StringComparer.OrdinalIgnoreCase);

    private NostrClientResources? _resources;
    private SynchronizationContext? _callbackContext;
    private bool _initialized;
    private INostrSigner? _signer;
    private bool _disposed;

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

    private void EnsureNotDisposed()
    {
        if (_disposed)
        {
            throw new COMException("NostrClient has been disposed.", ComErrorCodes.E_NOSTR_OBJECT_DISPOSED);
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
            session.CloseAllSubscriptions("Relay disconnected by client.");
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
        EnsureResources();

        if (callback is null)
        {
            throw new COMException("Event callback must not be null.", EPointer);
        }

        var normalizedFilters = NormalizeFilters(filters);
        IReadOnlyList<NostrFilterDto> filterDtos;

        try
        {
            filterDtos = NostrFilterConverter.ToDtos(normalizedFilters);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }

        var configuration = NormalizeSubscriptionOptions(options);
        var session = GetSessionOrThrow(relayUrl);

        if (!session.ReadEnabled)
        {
            throw new COMException($"Relay '{relayUrl}' is not configured for reading.", EInvalidarg);
        }

        var subscriptionId = GenerateSubscriptionId();

        try
        {
            return session.RegisterSubscription(
                subscriptionId,
                normalizedFilters,
                filterDtos,
                callback,
                configuration,
                CallbackContext);
        }
        catch (OperationCanceledException)
        {
            throw new COMException($"Opening subscription on '{relayUrl}' timed out.", ETimeout);
        }
        catch (WebSocketException ex)
        {
            throw new COMException($"Failed to open subscription on '{relayUrl}'. {ex.Message}", ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
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
            throw new COMException($"Failed to open subscription on '{relayUrl}'.", ex);
        }
    }

    public void PublishEvent(string relayUrl, NostrEvent eventPayload)
    {
        var resources = EnsureResources();

        if (eventPayload is null)
        {
            throw new COMException("Event payload must not be null.", EPointer);
        }

        var session = GetSessionOrThrow(relayUrl);

        if (!session.WriteEnabled)
        {
            throw new COMException($"Relay '{relayUrl}' is not configured for writing.", EInvalidarg);
        }

        var signer = EnsureSigner();

        NostrEvent normalizedEvent;
        NostrEventDraft draft;

        try
        {
            normalizedEvent = NormalizeEvent(eventPayload);
            draft = CreateDraftForSigning(normalizedEvent);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }
        catch (Exception ex)
        {
            throw new COMException("Failed to prepare Nostr event payload.", ex);
        }

        try
        {
            if (string.IsNullOrWhiteSpace(normalizedEvent.Signature))
            {
                var signature = signer.Sign(draft);
                normalizedEvent.PublicKey = NormalizeHex(draft.PublicKey);
                normalizedEvent.Signature = NormalizeHex(signature);
            }
            else
            {
                normalizedEvent.PublicKey = NormalizeHex(normalizedEvent.PublicKey);
                normalizedEvent.Signature = NormalizeHex(normalizedEvent.Signature);
                draft.PublicKey = normalizedEvent.PublicKey;
            }

            if (string.IsNullOrEmpty(normalizedEvent.PublicKey))
            {
                throw new ArgumentException("NostrEvent.PublicKey must be specified when publishing.", nameof(NostrEvent.PublicKey));
            }

            draft.PublicKey = normalizedEvent.PublicKey;
            normalizedEvent.Id = NormalizeHex(NostrSigner.ComputeEventId(draft, normalizedEvent.PublicKey));
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
            throw new COMException("Failed to sign Nostr event payload.", ex);
        }

        if (string.IsNullOrEmpty(normalizedEvent.Signature))
        {
            throw new COMException("Event signature must not be empty after signing.", EInvalidarg);
        }

        CopyEvent(normalizedEvent, eventPayload);

        IReadOnlyList<IReadOnlyList<string>> tagsForDto;
        long createdAtSeconds;

        try
        {
            tagsForDto = ConvertTagsToDto(normalizedEvent.Tags);
            createdAtSeconds = ToUnixSeconds(normalizedEvent.CreatedAt);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }
        catch (Exception ex)
        {
            throw new COMException("Failed to serialize Nostr event payload.", ex);
        }

        var eventDto = new NostrEventDto(
            normalizedEvent.Id,
            normalizedEvent.PublicKey,
            createdAtSeconds,
            normalizedEvent.Kind,
            tagsForDto,
            normalizedEvent.Content,
            normalizedEvent.Signature);

        try
        {
            var ok = session.PublishEvent(
                eventDto,
                DeterminePublishAckTimeout(resources.Options),
                PublishRetryAttempts);

            if (!ok.Success)
            {
                var message = string.IsNullOrEmpty(ok.Message)
                    ? $"Relay '{relayUrl}' rejected event '{ok.EventId}'."
                    : $"Relay '{relayUrl}' rejected event '{ok.EventId}': {ok.Message}";

                throw new COMException(message, ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
            }
        }
        catch (TimeoutException)
        {
            throw new COMException($"Publishing event on '{relayUrl}' timed out.", ETimeout);
        }
        catch (OperationCanceledException)
        {
            throw new COMException($"Publishing event on '{relayUrl}' was canceled.", ETimeout);
        }
        catch (WebSocketException ex)
        {
            throw new COMException($"Failed to publish event on '{relayUrl}'. {ex.Message}", ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }
        catch (InvalidOperationException ex)
        {
            throw new COMException(ex.Message, ex);
        }
        catch (COMException)
        {
            throw;
        }
        catch (Exception ex)
        {
            throw new COMException($"Failed to publish event on '{relayUrl}'.", ex);
        }
    }

    public void RespondAuth(string relayUrl, NostrEvent authEvent)
    {
        var resources = EnsureResources();

        if (authEvent is null)
        {
            throw new COMException("Auth event must not be null.", EPointer);
        }

        var session = GetSessionOrThrow(relayUrl);
        var signer = EnsureSigner();

        NostrEvent normalizedEvent;
        NostrEventDraft draft;

        try
        {
            normalizedEvent = NormalizeEvent(authEvent);
            draft = CreateDraftForSigning(normalizedEvent);

            if (normalizedEvent.Kind != 22242)
            {
                throw new ArgumentException("Authentication events must use kind 22242.", nameof(NostrEvent.Kind));
            }

            string? storedChallenge = null;
            if (session.TryGetAuthChallenge(out var challenge, out var _))
            {
                storedChallenge = challenge;
            }

            EnsureAuthTags(normalizedEvent, session.Url, storedChallenge);
            draft.Tags = CloneEventTags(normalizedEvent.Tags);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }
        catch (Exception ex)
        {
            throw new COMException("Failed to prepare authentication event.", ex);
        }

        try
        {
            if (string.IsNullOrWhiteSpace(normalizedEvent.PublicKey))
            {
                normalizedEvent.PublicKey = NormalizeHex(signer.GetPublicKey());
            }
            else
            {
                normalizedEvent.PublicKey = NormalizeHex(normalizedEvent.PublicKey);
            }

            draft.PublicKey = normalizedEvent.PublicKey;

            if (string.IsNullOrWhiteSpace(normalizedEvent.Signature))
            {
                normalizedEvent.Signature = NormalizeHex(signer.Sign(draft));
            }
            else
            {
                normalizedEvent.Signature = NormalizeHex(normalizedEvent.Signature);
            }

            normalizedEvent.Id = NormalizeHex(NostrSigner.ComputeEventId(draft, normalizedEvent.PublicKey));
        }
        catch (COMException)
        {
            throw;
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }
        catch (Exception ex)
        {
            throw new COMException("Failed to sign authentication event.", ex);
        }

        CopyEvent(normalizedEvent, authEvent);

        IReadOnlyList<IReadOnlyList<string>> tagsForDto;
        long createdAtSeconds;

        try
        {
            tagsForDto = ConvertTagsToDto(normalizedEvent.Tags);
            createdAtSeconds = ToUnixSeconds(normalizedEvent.CreatedAt);
        }
        catch (Exception ex)
        {
            throw new COMException("Failed to serialize authentication event.", ex);
        }

        var eventDto = new NostrEventDto(
            normalizedEvent.Id,
            normalizedEvent.PublicKey,
            createdAtSeconds,
            normalizedEvent.Kind,
            tagsForDto,
            normalizedEvent.Content,
            normalizedEvent.Signature);

        try
        {
            var ok = session.Authenticate(eventDto, DeterminePublishAckTimeout(resources.Options), PublishRetryAttempts);

            if (!ok.Success)
            {
                var message = string.IsNullOrEmpty(ok.Message)
                    ? $"Relay '{relayUrl}' rejected authentication event."
                    : $"Relay '{relayUrl}' rejected authentication event: {ok.Message}";

                throw new COMException(message, ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
            }
        }
        catch (TimeoutException)
        {
            throw new COMException($"Authentication on '{relayUrl}' timed out.", ETimeout);
        }
        catch (OperationCanceledException)
        {
            throw new COMException($"Authentication on '{relayUrl}' was canceled.", ETimeout);
        }
        catch (WebSocketException ex)
        {
            throw new COMException($"Failed to authenticate on '{relayUrl}'. {ex.Message}", ComErrorCodes.E_NOSTR_WEBSOCKET_ERROR);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }
        catch (InvalidOperationException ex)
        {
            throw new COMException(ex.Message, ex);
        }
        catch (COMException)
        {
            throw;
        }
        catch (Exception ex)
        {
            throw new COMException($"Failed to authenticate on '{relayUrl}'.", ex);
        }
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

    private INostrSigner EnsureSigner()
    {
        lock (_syncRoot)
        {
            if (_signer is null)
            {
                throw new COMException("Signer must be configured via SetSigner before publishing events.", ComErrorCodes.E_NOSTR_SIGNER_MISSING);
            }

            return _signer;
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

    private static string GenerateSubscriptionId()
    {
        Span<byte> buffer = stackalloc byte[32];
        RandomNumberGenerator.Fill(buffer);
        return Convert.ToHexString(buffer).ToLowerInvariant();
    }

    private static SubscriptionConfiguration NormalizeSubscriptionOptions(SubscriptionOptions? options)
    {
        var keepAlive = options?.KeepAlive ?? true;
        var autoRequery = ConvertToPositiveDouble(options?.AutoRequeryWindowSeconds, nameof(SubscriptionOptions.AutoRequeryWindowSeconds));
        var maxQueueLength = ConvertToPositiveInt(options?.MaxQueueLength, nameof(SubscriptionOptions.MaxQueueLength));
        var overflowStrategy = ConvertToOverflowStrategy(options?.QueueOverflowStrategy, nameof(SubscriptionOptions.QueueOverflowStrategy));
        return new SubscriptionConfiguration(keepAlive, autoRequery, maxQueueLength, overflowStrategy);
    }

    private static double? ConvertToPositiveDouble(object? value, string propertyName)
    {
        if (value is null)
        {
            return null;
        }

        double result = value switch
        {
            int intValue => intValue,
            long longValue => longValue,
            double doubleValue => doubleValue,
            float floatValue => floatValue,
            string text => ParseDouble(text, propertyName),
            _ => throw new ArgumentException($"{propertyName} must be specified as a number.", propertyName)
        };

        if (double.IsNaN(result) || double.IsInfinity(result))
        {
            throw new ArgumentException($"{propertyName} must be a finite number.", propertyName);
        }

        return result <= 0 ? null : result;
    }

    private static double ParseDouble(string text, string propertyName)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return 0;
        }

        if (double.TryParse(text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
        {
            return parsed;
        }

        throw new ArgumentException($"{propertyName} must be parsable as a number.", propertyName);
    }

    private static int? ConvertToPositiveInt(object? value, string propertyName)
    {
        if (value is null)
        {
            return null;
        }

        int result = value switch
        {
            int intValue => intValue,
            long longValue => checked((int)longValue),
            double doubleValue => (int)Math.Floor(doubleValue),
            float floatValue => (int)Math.Floor(floatValue),
            string text => ParseInt(text, propertyName),
            _ => throw new ArgumentException($"{propertyName} must be specified as a number.", propertyName)
        };

        return result <= 0 ? null : result;
    }

    private static QueueOverflowStrategy ConvertToOverflowStrategy(object? value, string propertyName)
    {
        if (value is null)
        {
            return QueueOverflowStrategy.DropOldest;
        }

        if (value is QueueOverflowStrategy strategy)
        {
            return strategy;
        }

        if (value is int intValue)
        {
            return intValue == 0 ? QueueOverflowStrategy.DropOldest : QueueOverflowStrategy.Throw;
        }

        if (value is long longValue)
        {
            return longValue == 0 ? QueueOverflowStrategy.DropOldest : QueueOverflowStrategy.Throw;
        }

        if (value is double doubleValue)
        {
            return Math.Abs(doubleValue) < double.Epsilon ? QueueOverflowStrategy.DropOldest : QueueOverflowStrategy.Throw;
        }

        if (value is float floatValue)
        {
            return Math.Abs(floatValue) < float.Epsilon ? QueueOverflowStrategy.DropOldest : QueueOverflowStrategy.Throw;
        }

        if (value is string textValue)
        {
            if (string.IsNullOrWhiteSpace(textValue))
            {
                return QueueOverflowStrategy.DropOldest;
            }

            var normalized = textValue.Trim();
            if (normalized.Equals("drop", StringComparison.OrdinalIgnoreCase) ||
                normalized.Equals("dropoldest", StringComparison.OrdinalIgnoreCase) ||
                normalized.Equals("keep", StringComparison.OrdinalIgnoreCase))
            {
                return QueueOverflowStrategy.DropOldest;
            }

            if (normalized.Equals("throw", StringComparison.OrdinalIgnoreCase) ||
                normalized.Equals("error", StringComparison.OrdinalIgnoreCase) ||
                normalized.Equals("fail", StringComparison.OrdinalIgnoreCase))
            {
                return QueueOverflowStrategy.Throw;
            }

            if (int.TryParse(normalized, NumberStyles.Integer, CultureInfo.InvariantCulture, out var parsedInt))
            {
                return parsedInt == 0 ? QueueOverflowStrategy.DropOldest : QueueOverflowStrategy.Throw;
            }

            throw new ArgumentException($"{propertyName} must be either 'DropOldest' or 'Throw'.", propertyName);
        }

        throw new ArgumentException($"{propertyName} must be specified as a string or integer.", propertyName);
    }

    private static int ParseInt(string text, string propertyName)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return 0;
        }

        if (double.TryParse(text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
        {
            if (double.IsNaN(parsed) || double.IsInfinity(parsed))
            {
                throw new ArgumentException($"{propertyName} must be a finite number.", propertyName);
            }

            return (int)Math.Floor(parsed);
        }

        throw new ArgumentException($"{propertyName} must be parsable as a number.", propertyName);
    }

    private static TimeSpan DeterminePublishAckTimeout(ClientRuntimeOptions options)
    {
        if (options is not null && options.ReceiveTimeout.HasValue && options.ReceiveTimeout.Value > TimeSpan.Zero)
        {
            return options.ReceiveTimeout.Value;
        }

        return DefaultPublishAckTimeout;
    }

    private static NostrEvent NormalizeEvent(NostrEvent source)
    {
        if (source is null)
        {
            throw new ArgumentNullException(nameof(source));
        }

        return new NostrEvent
        {
            Id = source.Id ?? string.Empty,
            PublicKey = source.PublicKey ?? string.Empty,
            CreatedAt = NormalizeCreatedAt(source.CreatedAt),
            Kind = source.Kind,
            Tags = CloneEventTags(source.Tags),
            Content = source.Content ?? string.Empty,
            Signature = source.Signature ?? string.Empty
        };
    }

    private static NostrEventDraft CreateDraftForSigning(NostrEvent source)
    {
        return new NostrEventDraft
        {
            PublicKey = source.PublicKey,
            CreatedAt = source.CreatedAt,
            Kind = source.Kind,
            Tags = CloneEventTags(source.Tags),
            Content = source.Content
        };
    }

    private static void EnsureAuthTags(NostrEvent normalizedEvent, string relayUrl, string? expectedChallenge)
    {
        if (normalizedEvent is null)
        {
            throw new ArgumentNullException(nameof(normalizedEvent));
        }

        var tags = normalizedEvent.Tags ?? Array.Empty<object>();
        var list = new List<object>(tags);
        var hasRelay = false;
        var hasChallenge = false;

        for (var i = 0; i < list.Count; i++)
        {
            if (!TryGetTagLabel(list[i], out var label, out var value))
            {
                continue;
            }

            if (string.Equals(label, "relay", StringComparison.OrdinalIgnoreCase))
            {
                var normalizedRelay = NormalizeRelayTagValue(value);
                if (!string.Equals(normalizedRelay, relayUrl, StringComparison.OrdinalIgnoreCase))
                {
                    throw new ArgumentException($"AUTH relay tag must match '{relayUrl}'.", nameof(NostrEvent.Tags));
                }

                hasRelay = true;
            }
            else if (string.Equals(label, "challenge", StringComparison.OrdinalIgnoreCase))
            {
                hasChallenge = true;
                if (!string.IsNullOrEmpty(expectedChallenge) && !string.Equals(value, expectedChallenge, StringComparison.Ordinal))
                {
                    throw new ArgumentException("AUTH challenge tag does not match the relay challenge.", nameof(NostrEvent.Tags));
                }
            }
        }

        if (!hasRelay)
        {
            list.Add(new object[] { "relay", relayUrl });
        }

        if (!hasChallenge)
        {
            if (string.IsNullOrEmpty(expectedChallenge))
            {
                throw new ArgumentException("Challenge tag is required for authentication.", nameof(NostrEvent.Tags));
            }

            list.Add(new object[] { "challenge", expectedChallenge });
        }

        normalizedEvent.Tags = list.ToArray();
    }

    private static bool TryGetTagLabel(object tag, out string label, out string? value)
    {
        label = string.Empty;
        value = null;

        if (tag is string[] stringArray && stringArray.Length > 0)
        {
            label = stringArray[0] ?? string.Empty;
            value = stringArray.Length > 1 ? stringArray[1] : null;
            return true;
        }

        if (tag is object[] objectArray && objectArray.Length > 0)
        {
            label = objectArray[0]?.ToString() ?? string.Empty;
            value = objectArray.Length > 1 ? objectArray[1]?.ToString() : null;
            return true;
        }

        return false;
    }

    private static string NormalizeRelayTagValue(string? relayTag)
    {
        if (string.IsNullOrWhiteSpace(relayTag))
        {
            throw new ArgumentException("Relay tag value must not be empty.", nameof(NostrEvent.Tags));
        }

        try
        {
            var uri = RelayUriUtilities.ParseWebSocketUri(relayTag);
            return RelayUriUtilities.ToCanonicalString(uri);
        }
        catch (ArgumentException ex)
        {
            throw new ArgumentException("Relay tag value is not a valid Nostr relay URL.", nameof(NostrEvent.Tags), ex);
        }
    }
    private static double NormalizeCreatedAt(double value)
    {
        if (double.IsNaN(value) || double.IsInfinity(value) || value <= 0)
        {
            return DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        }

        if (value > long.MaxValue)
        {
            throw new ArgumentOutOfRangeException(nameof(NostrEvent.CreatedAt), "CreatedAt exceeds UNIX timestamp range.");
        }

        return value;
    }

    private static long ToUnixSeconds(double value)
    {
        if (double.IsNaN(value) || double.IsInfinity(value))
        {
            throw new ArgumentException("CreatedAt must be a finite number.", nameof(NostrEvent.CreatedAt));
        }

        var floored = Math.Floor(value);
        if (floored < 0)
        {
            throw new ArgumentException("CreatedAt must not be negative.", nameof(NostrEvent.CreatedAt));
        }

        if (floored > long.MaxValue)
        {
            throw new ArgumentOutOfRangeException(nameof(NostrEvent.CreatedAt), "CreatedAt exceeds UNIX timestamp range.");
        }

        return (long)floored;
    }

    private static IReadOnlyList<IReadOnlyList<string>> ConvertTagsToDto(object[]? tags)
    {
        if (tags is null || tags.Length == 0)
        {
            return Array.Empty<IReadOnlyList<string>>();
        }

        var result = new List<IReadOnlyList<string>>(tags.Length);

        foreach (var tag in tags)
        {
            if (tag is string[] stringArray)
            {
                var copy = new string[stringArray.Length];
                for (var i = 0; i < stringArray.Length; i++)
                {
                    copy[i] = stringArray[i] ?? string.Empty;
                }

                result.Add(copy);
            }
            else if (tag is object[] objectArray)
            {
                var copy = new string[objectArray.Length];
                for (var i = 0; i < objectArray.Length; i++)
                {
                    copy[i] = objectArray[i]?.ToString() ?? string.Empty;
                }

                result.Add(copy);
            }
            else
            {
                throw new ArgumentException("Each tag must be specified as an array of strings.", nameof(NostrEvent.Tags));
            }
        }

        return result;
    }

    private static string NormalizeHex(string value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return string.Empty;
        }

        return value.Trim().ToLowerInvariant();
    }

    private static object[] CloneEventTags(object[]? tags)
    {
        if (tags is null || tags.Length == 0)
        {
            return Array.Empty<object>();
        }

        var result = new object[tags.Length];
        for (var i = 0; i < tags.Length; i++)
        {
            var tag = tags[i];
            if (tag is string[] stringArray)
            {
                result[i] = stringArray.ToArray();
            }
            else if (tag is object[] objectArray)
            {
                result[i] = objectArray.ToArray();
            }
            else
            {
                result[i] = tag ?? string.Empty;
            }
        }

        return result;
    }

    private static void CopyEvent(NostrEvent source, NostrEvent target)
    {
        target.Id = source.Id;
        target.PublicKey = source.PublicKey;
        target.CreatedAt = source.CreatedAt;
        target.Kind = source.Kind;
        target.Tags = CloneEventTags(source.Tags);
        target.Content = source.Content;
        target.Signature = source.Signature;
    }

    private static NostrFilter[] NormalizeFilters(NostrFilter[]? filters)
    {
        if (filters is null || filters.Length == 0)
        {
            return new[] { new NostrFilter() };
        }

        var result = new NostrFilter[filters.Length];
        for (var i = 0; i < filters.Length; i++)
        {
            result[i] = CloneFilter(filters[i]);
        }

        return result;
    }

    private static NostrFilter CloneFilter(NostrFilter? source)
    {
        if (source is null)
        {
            return new NostrFilter();
        }

        return new NostrFilter
        {
            Ids = source.Ids?.ToArray() ?? Array.Empty<string>(),
            Authors = source.Authors?.ToArray() ?? Array.Empty<string>(),
            Kinds = source.Kinds?.ToArray() ?? Array.Empty<int>(),
            Tags = CloneTags(source.Tags),
            Since = source.Since,
            Until = source.Until,
            Limit = source.Limit
        };
    }

    private static NostrTagQuery[] CloneTags(NostrTagQuery[]? tags)
    {
        if (tags is null || tags.Length == 0)
        {
            return Array.Empty<NostrTagQuery>();
        }

        var result = new NostrTagQuery[tags.Length];
        for (var i = 0; i < tags.Length; i++)
        {
            var tag = tags[i];
            if (tag is null)
            {
                result[i] = new NostrTagQuery();
                continue;
            }

            result[i] = new NostrTagQuery
            {
                Label = tag.Label,
                Values = tag.Values?.ToArray() ?? Array.Empty<string>()
            };
        }

        return result;
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

    private const string QueueOverflowClosedReason = "Subscription queue overflow.";

    private readonly object _stateLock = new();
    private readonly Uri _webSocketUri;
    private readonly Uri _metadataUri;
    private readonly NostrClientResources _resources;
    private readonly NostrHttpClient _httpClient;
    private readonly SynchronizationContext? _callbackContext;
    private readonly BackoffPolicy _backoffPolicy;
    private readonly NostrJsonSerializer _serializer;
    private readonly object _reconnectLock = new();
    private readonly TimeSpan _receiveTimeout;

    private readonly object _subscriptionLock = new();
    private readonly Dictionary<string, NostrSubscription> _subscriptions = new(StringComparer.Ordinal);
    private readonly object _pendingPublishLock = new();
    private readonly Dictionary<string, TaskCompletionSource<NostrOkMessage>> _pendingPublishes = new(StringComparer.OrdinalIgnoreCase);
    private readonly object _authLock = new();

    private Task? _reconnectWorker;
    private bool _shutdownRequested;
    private bool _disposed;

    private string? _lastAuthChallenge;
    private double? _lastAuthChallengeExpiresAt;
    private bool _authSucceeded;

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
        _receiveTimeout = resources.Options.ReceiveTimeout ?? TimeSpan.FromSeconds(60);
    }

    private void ThrowIfDisposed()
    {
        if (_disposed)
        {
            throw new ObjectDisposedException(nameof(NostrRelaySession));
        }
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
        ThrowIfDisposed();

        if (callback is null)
        {
            throw new ArgumentNullException(nameof(callback));
        }

        lock (_stateLock)
        {
            _authCallback = callback;
        }
    }

    internal bool TryGetAuthChallenge(out string? challenge, out double? expiresAt)
    {
        ThrowIfDisposed();

        lock (_authLock)
        {
            challenge = _lastAuthChallenge;
            expiresAt = _lastAuthChallengeExpiresAt;
            return !string.IsNullOrEmpty(challenge);
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
            if (isReconnect)
            {
                ResubscribeAll();
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
        Exception? failure = null;

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

            failure = ex;
        }
        finally
        {
            ArrayPool<byte>.Shared.Return(buffer);
            HandleConnectionTermination(failure);
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
            TaskCompletionSource<NostrOkMessage>? pending = null;

            lock (_pendingPublishLock)
            {
                if (_pendingPublishes.TryGetValue(ok.EventId, out pending))
                {
                    _pendingPublishes.Remove(ok.EventId);
                }
            }

            lock (_stateLock)
            {
                _lastOkResult = new NostrOkResult
                {
                    Success = ok.Success,
                    EventId = ok.EventId,
                    Message = ok.Message
                };
            }

            pending?.TrySetResult(ok);

            if (!ok.Success && !string.IsNullOrEmpty(ok.Message))
            {
                BroadcastNotice(ok.Message);
            }
            HandleAuthSignalFromOk(ok);

        }
        else if (string.Equals(messageType, "AUTH", StringComparison.Ordinal))
        {
            var challenge = _serializer.DeserializeAuthChallenge(payload);
            HandleAuthChallenge(challenge);
        }
        else if (string.Equals(messageType, "EVENT", StringComparison.Ordinal))
        {
            var message = _serializer.DeserializeEvent(payload);
            if (string.IsNullOrEmpty(message.SubscriptionId))
            {
                return;
            }

            var subscriptionId = message.SubscriptionId;
            if (TryGetSubscription(subscriptionId, out var subscription) && subscription is not null)
            {
                try
                {
                    var contractEvent = ConvertEvent(message.Event);
                    subscription.HandleEvent(contractEvent);
                }
                catch (SubscriptionQueueOverflowException)
                {
                    HandleSubscriptionOverflow(subscriptionId, subscription);
                }
            }
        }
        else if (string.Equals(messageType, "EOSE", StringComparison.Ordinal))
        {
            var eose = _serializer.DeserializeEndOfStoredEvents(payload);
            if (TryGetSubscription(eose.SubscriptionId, out var subscription) && subscription is not null)
            {
                subscription.HandleEndOfStoredEvents();
            }
        }
        else if (string.Equals(messageType, "CLOSED", StringComparison.Ordinal))
        {
            var closed = _serializer.DeserializeClosed(payload);
            if (RemoveSubscription(closed.SubscriptionId, out var subscription) && subscription is not null)
            {
                subscription.HandleClosed(closed.Reason);
            }

            HandleAuthSignalFromReason(closed.Reason);
        }
        else if (string.Equals(messageType, "NOTICE", StringComparison.Ordinal))
        {
            var notice = _serializer.DeserializeNotice(payload);
            BroadcastNotice(notice.Message);
            HandleAuthSignalFromReason(notice.Message);
        }
    }

    private void HandleSubscriptionOverflow(string subscriptionId, NostrSubscription subscription)
    {
        try
        {
            SendClose(subscriptionId);
        }
        catch
        {
            // Ignore errors while attempting to stop the subscription on the relay.
        }

        TryRemoveSubscription(subscriptionId, subscription);
        subscription.HandleClosed(QueueOverflowClosedReason);
    }

    internal NostrSubscription RegisterSubscription(
        string subscriptionId,
        NostrFilter[] originalFilters,
        IReadOnlyList<NostrFilterDto> filterDtos,
        INostrEventCallback callback,
        SubscriptionConfiguration configuration,
        SynchronizationContext? callbackContext)
    {
        ThrowIfDisposed();

        if (string.IsNullOrWhiteSpace(subscriptionId))
        {
            throw new ArgumentException("Subscription identifier must not be null or whitespace.", nameof(subscriptionId));
        }

        if (callback is null)
        {
            throw new ArgumentNullException(nameof(callback));
        }

        var subscription = new NostrSubscription(this, _descriptor.Url, subscriptionId, originalFilters, filterDtos, callback, callbackContext, configuration);
        IReadOnlyList<NostrFilterDto> requestFilters;

        lock (_subscriptionLock)
        {
            if (_subscriptions.ContainsKey(subscriptionId))
            {
                throw new InvalidOperationException($"Subscription '{subscriptionId}' is already registered.");
            }

            _subscriptions[subscriptionId] = subscription;
            requestFilters = subscription.BuildRequestFilters();
        }

        try
        {
            SendRequest(subscriptionId, requestFilters);
        }
        catch
        {
            lock (_subscriptionLock)
            {
                _subscriptions.Remove(subscriptionId);
            }

            throw;
        }

        return subscription;
    }

    internal bool TryRemoveSubscription(string subscriptionId, NostrSubscription subscription)
    {
        lock (_subscriptionLock)
        {
            if (_subscriptions.TryGetValue(subscriptionId, out var existing) && ReferenceEquals(existing, subscription))
            {
                _subscriptions.Remove(subscriptionId);
                return true;
            }
        }

        return false;
    }

    private bool RemoveSubscription(string subscriptionId, out NostrSubscription? subscription)
    {
        lock (_subscriptionLock)
        {
            if (_subscriptions.TryGetValue(subscriptionId, out subscription))
            {
                _subscriptions.Remove(subscriptionId);
                return true;
            }
        }

        subscription = null;
        return false;
    }

    private bool TryGetSubscription(string subscriptionId, out NostrSubscription? subscription)
    {
        lock (_subscriptionLock)
        {
            return _subscriptions.TryGetValue(subscriptionId, out subscription);
        }
    }

    internal void CloseAllSubscriptions(string reason)
    {
        NostrSubscription[] snapshot;
        lock (_subscriptionLock)
        {
            if (_subscriptions.Count == 0)
            {
                return;
            }

            snapshot = _subscriptions.Values.ToArray();
            _subscriptions.Clear();
        }

        foreach (var subscription in snapshot)
        {
            subscription.HandleSessionClosed(reason);
        }
    }

    private void BroadcastNotice(string message)
    {
        NostrSubscription[] snapshot;
        lock (_subscriptionLock)
        {
            if (_subscriptions.Count == 0)
            {
                return;
            }

            snapshot = _subscriptions.Values.ToArray();
        }

        foreach (var subscription in snapshot)
        {
            subscription.HandleNotice(message);
        }
    }

    private void ResubscribeAll()
    {
        NostrSubscription[] snapshot;

        lock (_subscriptionLock)
        {
            if (_subscriptions.Count == 0)
            {
                return;
            }

            snapshot = _subscriptions.Values.ToArray();
        }

        foreach (var subscription in snapshot)
        {
            try
            {
                var filters = subscription.BuildRequestFilters();
                SendRequest(subscription.Id, filters);
            }
            catch (Exception ex)
            {
                if (RemoveSubscription(subscription.Id, out _))
                {
                    subscription.HandleClosed($"Resubscribe failed: {ex.Message}");
                }
            }
        }
    }

    private void PrepareSubscriptionsForReconnect()
    {
        NostrSubscription[] snapshot;

        lock (_subscriptionLock)
        {
            if (_subscriptions.Count == 0)
            {
                return;
            }

            snapshot = _subscriptions.Values.ToArray();
        }

        foreach (var subscription in snapshot)
        {
            subscription.PrepareForReconnect();
        }
    }

    private void HandleConnectionTermination(Exception? failure)
    {
        var exception = failure ?? new OperationCanceledException("Relay connection ended.");
        FailPendingPublishes(exception);

        lock (_stateLock)
        {
            _connection = null;
            _receiveLoop = null;
            _sessionCancellation = null;

            if (_shutdownRequested)
            {
                _state = RelaySessionState.Disconnected;
            }
            else if (failure is not null)
            {
                _lastFault = failure;
                _state = RelaySessionState.Faulted;
            }
            else
            {
                _state = RelaySessionState.Disconnected;
            }
        }

        if (_shutdownRequested || _disposed)
        {
            CloseAllSubscriptions("Relay connection ended.");
            return;
        }

        PrepareSubscriptionsForReconnect();
        ScheduleReconnect();
    }

    private void ScheduleReconnect()
    {
        lock (_reconnectLock)
        {
            if (_shutdownRequested || _disposed)
            {
                return;
            }

            if (_reconnectWorker is not null && !_reconnectWorker.IsCompleted)
            {
                return;
            }

            _reconnectWorker = Task.Run(async () =>
            {
                try
                {
                    await ReconnectLoopAsync().ConfigureAwait(false);
                }
                finally
                {
                    lock (_reconnectLock)
                    {
                        _reconnectWorker = null;
                    }
                }
            });
        }
    }

    private async Task ReconnectLoopAsync()
    {
        while (true)
        {
            if (_shutdownRequested || _disposed)
            {
                return;
            }

            try
            {
                await ConnectInternalAsync(isReconnect: true, CancellationToken.None).ConfigureAwait(false);
                return;
            }
            catch (Exception ex)
            {
                lock (_stateLock)
                {
                    _lastFault = ex;
                    _state = RelaySessionState.Faulted;
                }

                await Task.Delay(_backoffPolicy.GetNextDelay()).ConfigureAwait(false);
            }
        }
    }

    private void FailPendingPublishes(Exception reason)
    {
        KeyValuePair<string, TaskCompletionSource<NostrOkMessage>>[] snapshot;

        lock (_pendingPublishLock)
        {
            if (_pendingPublishes.Count == 0)
            {
                return;
            }

            snapshot = _pendingPublishes.ToArray();
            _pendingPublishes.Clear();
        }

        foreach (var pair in snapshot)
        {
            pair.Value.TrySetException(reason);
        }
    }

    internal void SendRequest(string subscriptionId, IReadOnlyList<NostrFilterDto> filters)
    {
        var payload = _serializer.SerializeRequest(new NostrRequestMessage(subscriptionId, filters));
        SendPayload(payload);
    }

    internal void SendClose(string subscriptionId)
    {
        var payload = _serializer.SerializeClose(subscriptionId);
        SendPayload(payload);
    }

    internal NostrOkMessage PublishEvent(NostrEventDto eventDto, TimeSpan ackTimeout, int maxAttempts)
    {
        ThrowIfDisposed();

        if (eventDto is null)
        {
            throw new ArgumentNullException(nameof(eventDto));
        }

        if (maxAttempts <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(maxAttempts));
        }

        var payload = _serializer.SerializeEvent(eventDto);
        return SendWithAck(payload, eventDto.Id, ackTimeout, maxAttempts);
    }

    internal NostrOkMessage Authenticate(NostrEventDto eventDto, TimeSpan ackTimeout, int maxAttempts)
    {
        ThrowIfDisposed();

        if (eventDto is null)
        {
            throw new ArgumentNullException(nameof(eventDto));
        }

        if (maxAttempts <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(maxAttempts));
        }

        var payload = _serializer.SerializeAuth(eventDto);
        return SendWithAck(payload, eventDto.Id, ackTimeout, maxAttempts);
    }

    private NostrOkMessage SendWithAck(ReadOnlyMemory<byte> payload, string eventId, TimeSpan ackTimeout, int maxAttempts)
    {
        if (string.IsNullOrWhiteSpace(eventId))
        {
            throw new ArgumentException("Event identifier must not be null or whitespace.", nameof(eventId));
        }

        var completion = RegisterPublishAck(eventId);
        var backoff = new BackoffPolicy(TimeSpan.FromMilliseconds(250), TimeSpan.FromSeconds(2));
        var attempts = 0;

        try
        {
            while (true)
            {
                attempts++;

                try
                {
                    SendPayload(payload);
                }
                catch (OperationCanceledException) when (attempts < maxAttempts)
                {
                    Task.Delay(backoff.GetNextDelay()).GetAwaiter().GetResult();
                    continue;
                }
                catch (WebSocketException) when (attempts < maxAttempts)
                {
                    Task.Delay(backoff.GetNextDelay()).GetAwaiter().GetResult();
                    continue;
                }

                try
                {
                    if (ackTimeout > TimeSpan.Zero)
                    {
                        return completion.Task.WaitAsync(ackTimeout).GetAwaiter().GetResult();
                    }

                    return completion.Task.GetAwaiter().GetResult();
                }
                catch (TimeoutException) when (attempts < maxAttempts)
                {
                    Task.Delay(backoff.GetNextDelay()).GetAwaiter().GetResult();
                    continue;
                }
                catch (OperationCanceledException) when (attempts < maxAttempts)
                {
                    Task.Delay(backoff.GetNextDelay()).GetAwaiter().GetResult();
                    continue;
                }
            }
        }
        finally
        {
            RemovePublishAck(eventId, completion);
        }
    }
    private void HandleAuthChallenge(NostrAuthChallengeMessage challenge)
    {
        if (challenge is null)
        {
            throw new ArgumentNullException(nameof(challenge));
        }

        lock (_authLock)
        {
            _lastAuthChallenge = challenge.Challenge;
            _lastAuthChallengeExpiresAt = challenge.ExpiresAtUnixSeconds;
            _authSucceeded = false;
        }

        NotifyAuthRequired(challenge.Challenge, challenge.ExpiresAtUnixSeconds);
    }
    internal void ProcessAuthAck(NostrOkMessage ok)
    {
        HandleAuthSignalFromOk(ok);
    }

    private void HandleAuthSignalFromOk(NostrOkMessage ok)
    {
        if (ok is null)
        {
            return;
        }

        var signal = ParseAuthSignal(ok.Message);

        if (ok.Success)
        {
            if (signal == AuthSignal.Required)
            {
                NotifyAuthRequiredFromStoredChallenge();
            }

            NotifyAuthSucceeded();
            return;
        }

        switch (signal)
        {
            case AuthSignal.Required:
                NotifyAuthRequiredFromStoredChallenge();
                NotifyAuthFailed(ExtractAuthReason(ok.Message));
                break;
            case AuthSignal.Failed:
            case AuthSignal.Restricted:
                NotifyAuthFailed(ExtractAuthReason(ok.Message));
                break;
            default:
                if (!string.IsNullOrWhiteSpace(ok.Message))
                {
                    NotifyAuthFailed(ExtractAuthReason(ok.Message));
                }
                break;
        }
    }

    private void HandleAuthSignalFromReason(string reason)
    {
        var signal = ParseAuthSignal(reason);
        switch (signal)
        {
            case AuthSignal.Required:
                NotifyAuthRequiredFromStoredChallenge();
                break;
            case AuthSignal.Failed:
            case AuthSignal.Restricted:
                NotifyAuthFailed(ExtractAuthReason(reason));
                break;
            case AuthSignal.Succeeded:
                NotifyAuthSucceeded();
                break;
        }
    }

    private void NotifyAuthRequired(string challenge, double? expiresAt)
    {
        string relayUrl;
        INostrAuthCallback callback;
        lock (_stateLock)
        {
            callback = _authCallback;
            relayUrl = _descriptor.Url;
        }

        lock (_authLock)
        {
            _authSucceeded = false;
        }

        var contract = new AuthChallenge
        {
            RelayUrl = relayUrl,
            Challenge = challenge ?? string.Empty,
            ExpiresAt = expiresAt.HasValue ? expiresAt.Value : null
        };

        if (_callbackContext is not null)
        {
            var state = Tuple.Create(callback, contract);
            _callbackContext.Post(static s =>
            {
                var tuple = (Tuple<INostrAuthCallback, AuthChallenge>)s!;
                tuple.Item1.OnAuthRequired(tuple.Item2);
            }, state);
        }
        else
        {
            callback.OnAuthRequired(contract);
        }
    }

    private void NotifyAuthRequiredFromStoredChallenge()
    {
        string challenge;
        double? expiresAt;
        lock (_authLock)
        {
            challenge = _lastAuthChallenge ?? string.Empty;
            expiresAt = _lastAuthChallengeExpiresAt;
            _authSucceeded = false;
        }

        NotifyAuthRequired(challenge, expiresAt);
    }

    private void NotifyAuthFailed(string reason)
    {
        var message = ExtractAuthReason(reason);

        string relayUrl;
        INostrAuthCallback callback;
        lock (_stateLock)
        {
            callback = _authCallback;
            relayUrl = _descriptor.Url;
        }

        lock (_authLock)
        {
            _authSucceeded = false;
        }

        if (_callbackContext is not null)
        {
            var state = Tuple.Create(callback, relayUrl, message);
            _callbackContext.Post(static s =>
            {
                var tuple = (Tuple<INostrAuthCallback, string, string>)s!;
                tuple.Item1.OnAuthFailed(tuple.Item2, tuple.Item3);
            }, state);
        }
        else
        {
            callback.OnAuthFailed(relayUrl, message);
        }
    }

    private void NotifyAuthSucceeded()
    {
        INostrAuthCallback callback;
        string relayUrl;
        lock (_stateLock)
        {
            callback = _authCallback;
            relayUrl = _descriptor.Url;
        }

        bool shouldNotify;
        lock (_authLock)
        {
            if (_authSucceeded)
            {
                shouldNotify = false;
            }
            else
            {
                _authSucceeded = true;
                _lastAuthChallenge = null;
                _lastAuthChallengeExpiresAt = null;
                shouldNotify = true;
            }
        }

        if (!shouldNotify)
        {
            return;
        }

        if (_callbackContext is not null)
        {
            var state = Tuple.Create(callback, relayUrl);
            _callbackContext.Post(static s =>
            {
                var tuple = (Tuple<INostrAuthCallback, string>)s!;
                tuple.Item1.OnAuthSucceeded(tuple.Item2);
            }, state);
        }
        else
        {
            callback.OnAuthSucceeded(relayUrl);
        }
    }

    private static AuthSignal ParseAuthSignal(string? reason)
    {
        if (string.IsNullOrWhiteSpace(reason))
        {
            return AuthSignal.None;
        }

        var trimmed = reason.Trim();

        if (trimmed.StartsWith("auth-required", StringComparison.OrdinalIgnoreCase))
        {
            return AuthSignal.Required;
        }

        if (trimmed.StartsWith("auth-failed", StringComparison.OrdinalIgnoreCase))
        {
            return AuthSignal.Failed;
        }

        if (trimmed.StartsWith("restricted", StringComparison.OrdinalIgnoreCase))
        {
            return AuthSignal.Restricted;
        }

        if (trimmed.StartsWith("auth-success", StringComparison.OrdinalIgnoreCase) ||
            trimmed.StartsWith("auth-ok", StringComparison.OrdinalIgnoreCase))
        {
            return AuthSignal.Succeeded;
        }

        return AuthSignal.None;
    }

    private static string ExtractAuthReason(string? reason)
    {
        if (string.IsNullOrWhiteSpace(reason))
        {
            return "Authentication failed.";
        }

        var trimmed = reason.Trim();
        var colonIndex = trimmed.IndexOf(':');
        if (colonIndex >= 0 && colonIndex + 1 < trimmed.Length)
        {
            return trimmed.Substring(colonIndex + 1).Trim();
        }

        if (trimmed.Equals("auth-required", StringComparison.OrdinalIgnoreCase))
        {
            return "Authentication required.";
        }

        if (trimmed.Equals("auth-failed", StringComparison.OrdinalIgnoreCase))
        {
            return "Authentication failed.";
        }

        if (trimmed.Equals("restricted", StringComparison.OrdinalIgnoreCase))
        {
            return "Access restricted.";
        }

        if (trimmed.Equals("auth-success", StringComparison.OrdinalIgnoreCase) ||
            trimmed.Equals("auth-ok", StringComparison.OrdinalIgnoreCase))
        {
            return "Authentication succeeded.";
        }

        return trimmed;
    }

    private enum AuthSignal
    {
        None,
        Required,
        Failed,
        Restricted,
        Succeeded
    }
    private TaskCompletionSource<NostrOkMessage> RegisterPublishAck(string eventId)
    {
        if (string.IsNullOrWhiteSpace(eventId))
        {
            throw new ArgumentException("Event identifier must not be null or whitespace.", nameof(eventId));
        }

        var tcs = new TaskCompletionSource<NostrOkMessage>(TaskCreationOptions.RunContinuationsAsynchronously);

        lock (_pendingPublishLock)
        {
            _pendingPublishes[eventId] = tcs;
        }

        return tcs;
    }

    private void RemovePublishAck(string eventId, TaskCompletionSource<NostrOkMessage> completion)
    {
        lock (_pendingPublishLock)
        {
            if (_pendingPublishes.TryGetValue(eventId, out var existing) && ReferenceEquals(existing, completion))
            {
                _pendingPublishes.Remove(eventId);
            }
        }
    }

    private void SendPayload(ReadOnlyMemory<byte> payload)
    {
        IWebSocketConnection? connection;
        CancellationToken token;
        CancellationTokenSource? timeoutCts = null;

        lock (_stateLock)
        {
            connection = _connection;
            token = _sessionCancellation?.Token ?? CancellationToken.None;
        }

        if (connection is null)
        {
            throw new InvalidOperationException("Relay session is not connected.");
        }

        if (_resources.Options.SendTimeout.HasValue)
        {
            timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(token);
            timeoutCts.CancelAfter(_resources.Options.SendTimeout.Value);
            token = timeoutCts.Token;
        }

        try
        {
            connection.SendAsync(payload, WebSocketMessageType.Text, true, token).GetAwaiter().GetResult();
        }
        finally
        {
            timeoutCts?.Dispose();
        }
    }

    private static NostrEvent ConvertEvent(NostrEventDto dto)
    {
        var tags = dto.Tags.Count == 0
            ? Array.Empty<object>()
            : dto.Tags.Select(tag => (object)tag.ToArray()).ToArray();

        return new NostrEvent
        {
            Id = dto.Id,
            PublicKey = dto.PublicKey,
            CreatedAt = dto.CreatedAt,
            Kind = dto.Kind,
            Tags = tags,
            Content = dto.Content,
            Signature = dto.Signature
        };
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

internal readonly struct SubscriptionConfiguration
{
    public SubscriptionConfiguration(bool keepAlive, double? autoRequeryWindowSeconds, int? maxQueueLength, QueueOverflowStrategy overflowStrategy)
    {
        KeepAlive = keepAlive;
        AutoRequeryWindowSeconds = autoRequeryWindowSeconds;
        MaxQueueLength = maxQueueLength;
        OverflowStrategy = overflowStrategy;
    }

    public bool KeepAlive { get; }

    public double? AutoRequeryWindowSeconds { get; }

    public int? MaxQueueLength { get; }

    public QueueOverflowStrategy OverflowStrategy { get; }
}

[ComVisible(true)]
[Guid("175bd625-18d9-42bd-b75a-0642abf029b4")]
[ClassInterface(ClassInterfaceType.None)]
public sealed class NostrSubscription : INostrSubscription
{
    private const int EInvalidarg = unchecked((int)0x80070057);

    private readonly NostrRelaySession _session;
    private readonly string _relayUrl;
    private readonly SynchronizationContext? _callbackContext;
    private readonly INostrEventCallback _callback;
    private readonly bool _keepAlive;
    private readonly double? _autoRequeryWindowSeconds;
    private readonly int _maxQueueLength;
    private readonly QueueOverflowStrategy _overflowStrategy;

    private readonly object _gate = new();
    private readonly Queue<CallbackWork> _pendingCallbacks = new();

    private NostrFilter[] _filters;
    private IReadOnlyList<NostrFilterDto> _filterDtos;
    private SubscriptionStatus _status = SubscriptionStatus.Pending;
    private bool _dispatchScheduled;
    private int _pendingEventCallbacks;
    private bool _closeRequested;
    private double? _lastSeenTimestamp;

    internal NostrSubscription(
        NostrRelaySession session,
        string relayUrl,
        string id,
        NostrFilter[] filters,
        IReadOnlyList<NostrFilterDto> filterDtos,
        INostrEventCallback callback,
        SynchronizationContext? callbackContext,
        SubscriptionConfiguration configuration)
    {
        _session = session ?? throw new ArgumentNullException(nameof(session));
        _relayUrl = relayUrl ?? throw new ArgumentNullException(nameof(relayUrl));
        Id = id ?? throw new ArgumentNullException(nameof(id));
        _callback = callback ?? throw new ArgumentNullException(nameof(callback));
        _callbackContext = callbackContext;
        _keepAlive = configuration.KeepAlive;
        _autoRequeryWindowSeconds = configuration.AutoRequeryWindowSeconds;
        _maxQueueLength = configuration.MaxQueueLength ?? 0;
        _overflowStrategy = configuration.OverflowStrategy;
        _filters = CloneFilters(filters);
        _filterDtos = filterDtos ?? throw new ArgumentNullException(nameof(filterDtos));
    }

    public string Id { get; }

    public SubscriptionStatus Status
    {
        get
        {
            lock (_gate)
            {
                return _status;
            }
        }
    }

    public NostrFilter[] Filters
    {
        get
        {
            lock (_gate)
            {
                return CloneFilters(_filters);
            }
        }
    }

    public void UpdateFilters(NostrFilter[] filters)
    {
        if (filters is null)
        {
            throw new COMException("Filters must not be null.", EInvalidarg);
        }

        var normalized = CloneFilters(filters);
        IReadOnlyList<NostrFilterDto> filterDtos;

        try
        {
            filterDtos = NostrFilterConverter.ToDtos(normalized);
        }
        catch (ArgumentException ex)
        {
            throw new COMException(ex.Message, EInvalidarg);
        }

        IReadOnlyList<NostrFilterDto> requestFilters;
        lock (_gate)
        {
            if (_status == SubscriptionStatus.Closed)
            {
                throw new InvalidOperationException("Subscription has already been closed.");
            }

            _filters = normalized;
            _filterDtos = filterDtos;
            _status = SubscriptionStatus.Pending;
            _closeRequested = false;
            requestFilters = BuildRequestFiltersInternal();
        }

        _session.SendRequest(Id, requestFilters);
    }

    public void Close()
    {
        bool shouldSendClose;

        lock (_gate)
        {
            if (_status == SubscriptionStatus.Closed)
            {
                return;
            }

            if (_status != SubscriptionStatus.Draining)
            {
                _status = SubscriptionStatus.Draining;
            }

            if (_closeRequested)
            {
                shouldSendClose = false;
            }
            else
            {
                _closeRequested = true;
                shouldSendClose = true;
            }
        }

        if (shouldSendClose)
        {
            try
            {
                _session.SendClose(Id);
            }
            catch
            {
                // Ignored: closing due to client request.
            }
        }

        if (_session.TryRemoveSubscription(Id, this))
        {
            HandleClosed("Subscription closed by client.", suppressRemoval: true);
        }
    }

    internal IReadOnlyList<NostrFilterDto> BuildRequestFilters()
    {
        lock (_gate)
        {
            return BuildRequestFiltersInternal();
        }
    }

    internal void PrepareForReconnect()
    {
        lock (_gate)
        {
            if (_status == SubscriptionStatus.Closed)
            {
                return;
            }

            _status = SubscriptionStatus.Pending;
            _closeRequested = false;
        }
    }

    internal void HandleEvent(NostrEvent eventData)
    {
        if (eventData is null)
        {
            return;
        }

        lock (_gate)
        {
            if (_status == SubscriptionStatus.Closed)
            {
                return;
            }

            var createdAt = eventData.CreatedAt;
            if (!double.IsNaN(createdAt) && !double.IsInfinity(createdAt))
            {
                _lastSeenTimestamp = _lastSeenTimestamp.HasValue
                    ? Math.Max(_lastSeenTimestamp.Value, createdAt)
                    : createdAt;
            }
        }

        EnqueueWork(CallbackWork.ForEvent(CloneEvent(eventData)));
    }

    internal void HandleEndOfStoredEvents()
    {
        bool shouldSendClose = false;

        lock (_gate)
        {
            if (_status == SubscriptionStatus.Closed)
            {
                return;
            }

            if (_status == SubscriptionStatus.Pending)
            {
                _status = _keepAlive ? SubscriptionStatus.Active : SubscriptionStatus.Draining;
            }

            if (!_keepAlive && !_closeRequested)
            {
                _closeRequested = true;
                shouldSendClose = true;
            }
        }

        EnqueueWork(CallbackWork.ForEndOfStoredEvents());

        if (shouldSendClose)
        {
            try
            {
                _session.SendClose(Id);
            }
            catch
            {
                // Suppress errors during automatic close.
            }

            if (_session.TryRemoveSubscription(Id, this))
            {
                HandleClosed("KeepAlive=false auto close.", suppressRemoval: true);
            }
        }
    }

    internal void HandleClosed(string reason, bool suppressRemoval = false)
    {
        lock (_gate)
        {
            if (_status == SubscriptionStatus.Closed)
            {
                return;
            }

            _status = SubscriptionStatus.Closed;
            _closeRequested = true;
        }

        EnqueueWork(CallbackWork.ForClosed(reason ?? string.Empty));
    }

    internal void HandleSessionClosed(string reason)
    {
        lock (_gate)
        {
            if (_status == SubscriptionStatus.Closed)
            {
                return;
            }

            _status = SubscriptionStatus.Closed;
        }

        EnqueueWork(CallbackWork.ForClosed(reason));
    }

    internal void HandleNotice(string message)
    {
        EnqueueWork(CallbackWork.ForNotice(message ?? string.Empty));
    }

    private IReadOnlyList<NostrFilterDto> BuildRequestFiltersInternal()
    {
        if (_filterDtos.Count == 0)
        {
            return Array.Empty<NostrFilterDto>();
        }

        if (!_autoRequeryWindowSeconds.HasValue || !_lastSeenTimestamp.HasValue)
        {
            return new List<NostrFilterDto>(_filterDtos);
        }

        var windowStart = Math.Max(0, _lastSeenTimestamp.Value - _autoRequeryWindowSeconds.Value);
        var sinceBoundary = (long)Math.Floor(windowStart);
        var result = new List<NostrFilterDto>(_filterDtos.Count);

        foreach (var filter in _filterDtos)
        {
            var effectiveSince = filter.Since.HasValue && filter.Since.Value > sinceBoundary
                ? filter.Since.Value
                : sinceBoundary;

            result.Add(new NostrFilterDto(filter.Ids, filter.Authors, filter.Kinds, filter.Tags, effectiveSince, filter.Until, filter.Limit));
        }

        return result;
    }

    private void EnqueueWork(CallbackWork work)
    {
        bool schedule = false;
        bool overflow = false;

        lock (_gate)
        {
            if (work.Kind == CallbackKind.Event)
            {
                if (_maxQueueLength > 0 && _pendingEventCallbacks >= _maxQueueLength)
                {
                    if (_overflowStrategy == QueueOverflowStrategy.DropOldest)
                    {
                        DropOldestEventLocked();
                    }
                    else
                    {
                        overflow = true;
                    }
                }

                if (!overflow)
                {
                    _pendingEventCallbacks++;
                }
            }

            if (!overflow)
            {
                _pendingCallbacks.Enqueue(work);

                if (!_dispatchScheduled)
                {
                    _dispatchScheduled = true;
                    schedule = true;
                }
            }
        }

        if (overflow)
        {
            throw new SubscriptionQueueOverflowException(_relayUrl, Id);
        }

        if (schedule)
        {
            if (_callbackContext is not null)
            {
                _callbackContext.Post(static state => ((NostrSubscription)state!).DrainCallbacks(), this);
            }
            else
            {
                Task.Run(DrainCallbacks);
            }
        }
    }

    private void DrainCallbacks()
    {
        while (true)
        {
            CallbackWork work;

            lock (_gate)
            {
                if (_pendingCallbacks.Count == 0)
                {
                    _dispatchScheduled = false;
                    return;
                }

                work = _pendingCallbacks.Dequeue();
                if (work.Kind == CallbackKind.Event && _pendingEventCallbacks > 0)
                {
                    _pendingEventCallbacks--;
                }
            }

            try
            {
                switch (work.Kind)
                {
                    case CallbackKind.Event:
                        _callback.OnEvent(_relayUrl, work.Event!);
                        break;
                    case CallbackKind.EndOfStoredEvents:
                        _callback.OnEndOfStoredEvents(_relayUrl, Id);
                        break;
                    case CallbackKind.Notice:
                        _callback.OnNotice(_relayUrl, work.Message ?? string.Empty);
                        break;
                    case CallbackKind.Closed:
                        _callback.OnClosed(_relayUrl, Id, work.Message ?? string.Empty);
                        break;
                }
            }
            catch
            {
                // Swallow callback exceptions to avoid terminating the dispatcher.
            }
        }
    }

    private void DropOldestEventLocked()
    {
        if (_pendingEventCallbacks == 0 || _pendingCallbacks.Count == 0)
        {
            return;
        }

        var temp = new Queue<CallbackWork>(_pendingCallbacks.Count);
        var dropped = false;

        while (_pendingCallbacks.Count > 0)
        {
            var item = _pendingCallbacks.Dequeue();
            if (!dropped && item.Kind == CallbackKind.Event)
            {
                dropped = true;
                if (_pendingEventCallbacks > 0)
                {
                    _pendingEventCallbacks--;
                }

                continue;
            }

            temp.Enqueue(item);
        }

        while (temp.Count > 0)
        {
            _pendingCallbacks.Enqueue(temp.Dequeue());
        }
    }

    private static NostrEvent CloneEvent(NostrEvent source)
    {
        return new NostrEvent
        {
            Id = source.Id,
            PublicKey = source.PublicKey,
            CreatedAt = source.CreatedAt,
            Kind = source.Kind,
            Tags = CloneEventTags(source.Tags),
            Content = source.Content,
            Signature = source.Signature
        };
    }

    private static object[] CloneEventTags(object[]? tags)
    {
        if (tags is null || tags.Length == 0)
        {
            return Array.Empty<object>();
        }

        var result = new object[tags.Length];
        for (var i = 0; i < tags.Length; i++)
        {
            if (tags[i] is string[] stringArray)
            {
                result[i] = stringArray.ToArray();
            }
            else if (tags[i] is object[] nested)
            {
                result[i] = nested.ToArray();
            }
            else
            {
                result[i] = tags[i] ?? string.Empty;
            }
        }

        return result;
    }

    private static NostrFilter[] CloneFilters(NostrFilter[]? filters)
    {
        if (filters is null || filters.Length == 0)
        {
            return new[] { new NostrFilter() };
        }

        var result = new NostrFilter[filters.Length];
        for (var i = 0; i < filters.Length; i++)
        {
            var source = filters[i];
            if (source is null)
            {
                result[i] = new NostrFilter();
                continue;
            }

            result[i] = new NostrFilter
            {
                Ids = source.Ids?.ToArray() ?? Array.Empty<string>(),
                Authors = source.Authors?.ToArray() ?? Array.Empty<string>(),
                Kinds = source.Kinds?.ToArray() ?? Array.Empty<int>(),
                Tags = CloneTagQueries(source.Tags),
                Since = source.Since,
                Until = source.Until,
                Limit = source.Limit
            };
        }

        return result;
    }

    private static NostrTagQuery[] CloneTagQueries(NostrTagQuery[]? tags)
    {
        if (tags is null || tags.Length == 0)
        {
            return Array.Empty<NostrTagQuery>();
        }

        var result = new NostrTagQuery[tags.Length];
        for (var i = 0; i < tags.Length; i++)
        {
            var tag = tags[i];
            if (tag is null)
            {
                result[i] = new NostrTagQuery();
                continue;
            }

            result[i] = new NostrTagQuery
            {
                Label = tag.Label,
                Values = tag.Values?.ToArray() ?? Array.Empty<string>()
            };
        }

        return result;
    }

    private readonly struct CallbackWork
    {
        private CallbackWork(CallbackKind kind, NostrEvent? @event, string? message)
        {
            Kind = kind;
            Event = @event;
            Message = message;
        }

        public CallbackKind Kind { get; }

        public NostrEvent? Event { get; }

        public string? Message { get; }

        public static CallbackWork ForEvent(NostrEvent @event) => new(CallbackKind.Event, @event, null);

        public static CallbackWork ForNotice(string message) => new(CallbackKind.Notice, null, message);

        public static CallbackWork ForEndOfStoredEvents() => new(CallbackKind.EndOfStoredEvents, null, null);

        public static CallbackWork ForClosed(string reason) => new(CallbackKind.Closed, null, reason);
    }

    private enum CallbackKind
    {
        Event,
        EndOfStoredEvents,
        Notice,
        Closed
    }
}




















internal sealed class SubscriptionQueueOverflowException : Exception
{
    public SubscriptionQueueOverflowException(string relayUrl, string subscriptionId)
        : base($"Subscription '{subscriptionId}' on relay '{relayUrl}' exceeded the configured queue length.")
    {
        RelayUrl = relayUrl;
        SubscriptionId = subscriptionId;
    }

    public string RelayUrl { get; }

    public string SubscriptionId { get; }
}










