using System;
using System.Collections.Generic;

namespace COM_Nostr.Internal;

internal sealed class NostrEventDto
{
    public NostrEventDto(
        string id,
        string publicKey,
        long createdAt,
        int kind,
        IReadOnlyList<IReadOnlyList<string>> tags,
        string content,
        string signature)
    {
        Id = id ?? throw new ArgumentNullException(nameof(id));
        PublicKey = publicKey ?? throw new ArgumentNullException(nameof(publicKey));
        CreatedAt = createdAt;
        Kind = kind;
        Tags = tags ?? throw new ArgumentNullException(nameof(tags));
        Content = content ?? throw new ArgumentNullException(nameof(content));
        Signature = signature ?? throw new ArgumentNullException(nameof(signature));
    }

    public string Id { get; }

    public string PublicKey { get; }

    public long CreatedAt { get; }

    public int Kind { get; }

    public IReadOnlyList<IReadOnlyList<string>> Tags { get; }

    public string Content { get; }

    public string Signature { get; }
}

internal sealed class NostrEventMessage
{
    public NostrEventMessage(string? subscriptionId, NostrEventDto @event)
    {
        SubscriptionId = subscriptionId;
        Event = @event ?? throw new ArgumentNullException(nameof(@event));
    }

    public string? SubscriptionId { get; }

    public NostrEventDto Event { get; }
}

internal sealed class NostrRequestMessage
{
    public NostrRequestMessage(string subscriptionId, IReadOnlyList<NostrFilterDto> filters)
    {
        if (string.IsNullOrWhiteSpace(subscriptionId))
        {
            throw new ArgumentException("Subscription identifier must not be null or whitespace.", nameof(subscriptionId));
        }

        SubscriptionId = subscriptionId;
        Filters = filters ?? throw new ArgumentNullException(nameof(filters));
    }

    public string SubscriptionId { get; }

    public IReadOnlyList<NostrFilterDto> Filters { get; }
}

internal sealed class NostrFilterDto
{
    public NostrFilterDto(
        IReadOnlyList<string>? ids,
        IReadOnlyList<string>? authors,
        IReadOnlyList<int>? kinds,
        IReadOnlyDictionary<string, IReadOnlyList<string>>? tags,
        long? since,
        long? until,
        int? limit)
    {
        Ids = ids ?? Array.Empty<string>();
        Authors = authors ?? Array.Empty<string>();
        Kinds = kinds ?? Array.Empty<int>();
        Tags = tags ?? new Dictionary<string, IReadOnlyList<string>>(StringComparer.OrdinalIgnoreCase);
        Since = since;
        Until = until;
        Limit = limit;
    }

    public IReadOnlyList<string> Ids { get; }

    public IReadOnlyList<string> Authors { get; }

    public IReadOnlyList<int> Kinds { get; }

    public IReadOnlyDictionary<string, IReadOnlyList<string>> Tags { get; }

    public long? Since { get; }

    public long? Until { get; }

    public int? Limit { get; }
}

internal sealed class NostrOkMessage
{
    public NostrOkMessage(string eventId, bool success, string message)
    {
        if (string.IsNullOrWhiteSpace(eventId))
        {
            throw new ArgumentException("Event identifier must not be null or whitespace.", nameof(eventId));
        }

        EventId = eventId;
        Success = success;
        Message = message ?? string.Empty;
    }

    public string EventId { get; }

    public bool Success { get; }

    public string Message { get; }
}


internal sealed class NostrAuthChallengeMessage
{
    public NostrAuthChallengeMessage(string challenge, double? expiresAt)
    {
        if (string.IsNullOrWhiteSpace(challenge))
        {
            throw new ArgumentException("Challenge must not be null or whitespace.", nameof(challenge));
        }

        Challenge = challenge;
        ExpiresAtUnixSeconds = expiresAt;
    }

    public string Challenge { get; }

    public double? ExpiresAtUnixSeconds { get; }
}

internal sealed class NostrNoticeMessage
{
    public NostrNoticeMessage(string message)
    {
        if (string.IsNullOrEmpty(message))
        {
            throw new ArgumentException("Notice message must not be null or empty.", nameof(message));
        }

        Message = message;
    }

    public string Message { get; }
}
internal sealed class NostrEndOfStoredEventsMessage
{
    public NostrEndOfStoredEventsMessage(string subscriptionId)
    {
        if (string.IsNullOrWhiteSpace(subscriptionId))
        {
            throw new ArgumentException("Subscription identifier must not be null or whitespace.", nameof(subscriptionId));
        }

        SubscriptionId = subscriptionId;
    }

    public string SubscriptionId { get; }
}

internal sealed class NostrClosedMessage
{
    public NostrClosedMessage(string subscriptionId, string reason)
    {
        if (string.IsNullOrWhiteSpace(subscriptionId))
        {
            throw new ArgumentException("Subscription identifier must not be null or whitespace.", nameof(subscriptionId));
        }

        SubscriptionId = subscriptionId;
        Reason = reason ?? string.Empty;
    }

    public string SubscriptionId { get; }

    public string Reason { get; }
}
