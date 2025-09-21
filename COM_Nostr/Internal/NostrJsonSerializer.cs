using System;
using System.Buffers;
using System.Collections.Generic;
using System.Linq;
using System.Text.Encodings.Web;
using System.Text.Json;

namespace COM_Nostr.Internal;

internal sealed class NostrJsonSerializer
{
    private static readonly JsonWriterOptions WriterOptions = new()
    {
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
        Indented = false
    };

    public byte[] SerializeEvent(NostrEventDto @event)
    {
        if (@event is null)
        {
            throw new ArgumentNullException(nameof(@event));
        }

        var buffer = new ArrayBufferWriter<byte>();
        using (var writer = new Utf8JsonWriter(buffer, WriterOptions))
        {
            writer.WriteStartArray();
            writer.WriteStringValue("EVENT");
            WriteEventObject(writer, @event);
            writer.WriteEndArray();
            writer.Flush();
        }

        return buffer.WrittenSpan.ToArray();
    }

    public byte[] SerializeRequest(NostrRequestMessage request)
    {
        if (request is null)
        {
            throw new ArgumentNullException(nameof(request));
        }

        var buffer = new ArrayBufferWriter<byte>();
        using (var writer = new Utf8JsonWriter(buffer, WriterOptions))
        {
            writer.WriteStartArray();
            writer.WriteStringValue("REQ");
            writer.WriteStringValue(request.SubscriptionId);
            foreach (var filter in request.Filters)
            {
                WriteFilterObject(writer, filter);
            }
            writer.WriteEndArray();
            writer.Flush();
        }

        return buffer.WrittenSpan.ToArray();
    }

    public byte[] SerializeClose(string subscriptionId)
    {
        if (string.IsNullOrWhiteSpace(subscriptionId))
        {
            throw new ArgumentException("Subscription identifier must not be null or whitespace.", nameof(subscriptionId));
        }

        var buffer = new ArrayBufferWriter<byte>();
        using (var writer = new Utf8JsonWriter(buffer, WriterOptions))
        {
            writer.WriteStartArray();
            writer.WriteStringValue("CLOSE");
            writer.WriteStringValue(subscriptionId);
            writer.WriteEndArray();
            writer.Flush();
        }

        return buffer.WrittenSpan.ToArray();
    }

    public NostrEventMessage DeserializeEvent(ReadOnlySpan<byte> json)
    {
        using var document = JsonDocument.Parse(json.ToArray());
        if (document.RootElement.ValueKind != JsonValueKind.Array)
        {
            throw new FormatException("EVENT message must be a JSON array.");
        }

        var root = document.RootElement;
        if (root.GetArrayLength() < 2)
        {
            throw new FormatException("EVENT message missing required elements.");
        }

        var messageType = root[0].GetString();
        if (!string.Equals(messageType, "EVENT", StringComparison.Ordinal))
        {
            throw new FormatException("JSON message is not an EVENT type.");
        }

        string? subscriptionId = null;
        JsonElement eventElement;
        if (root.GetArrayLength() == 2)
        {
            eventElement = root[1];
        }
        else if (root.GetArrayLength() >= 3)
        {
            subscriptionId = root[1].GetString();
            eventElement = root[2];
        }
        else
        {
            throw new FormatException("EVENT message contains unexpected element count.");
        }

        var eventDto = ReadEventObject(eventElement);
        return new NostrEventMessage(subscriptionId, eventDto);
    }

    public NostrOkMessage DeserializeOk(ReadOnlySpan<byte> json)
    {
        using var document = JsonDocument.Parse(json.ToArray());
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Array)
        {
            throw new FormatException("OK message must be a JSON array.");
        }

        if (root.GetArrayLength() < 3)
        {
            throw new FormatException("OK message missing required elements.");
        }

        var messageType = root[0].GetString();
        if (!string.Equals(messageType, "OK", StringComparison.Ordinal))
        {
            throw new FormatException("JSON message is not an OK type.");
        }

        var eventId = root[1].GetString() ?? throw new FormatException("OK message missing event id.");
        var success = root[2].GetBoolean();
        var message = root.GetArrayLength() >= 4 ? root[3].GetString() ?? string.Empty : string.Empty;
        return new NostrOkMessage(eventId, success, message);
    }

    public NostrNoticeMessage DeserializeNotice(ReadOnlySpan<byte> json)
    {
        using var document = JsonDocument.Parse(json.ToArray());
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Array)
        {
            throw new FormatException("NOTICE message must be a JSON array.");
        }

        if (root.GetArrayLength() < 2)
        {
            throw new FormatException("NOTICE message missing text element.");
        }

        var messageType = root[0].GetString();
        if (!string.Equals(messageType, "NOTICE", StringComparison.Ordinal))
        {
            throw new FormatException("JSON message is not a NOTICE type.");
        }

        var message = root[1].GetString();
        if (string.IsNullOrEmpty(message))
        {
            throw new FormatException("NOTICE message text must not be empty.");
        }

        return new NostrNoticeMessage(message);
    }

    public NostrEndOfStoredEventsMessage DeserializeEndOfStoredEvents(ReadOnlySpan<byte> json)
    {
        using var document = JsonDocument.Parse(json.ToArray());
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Array)
        {
            throw new FormatException("EOSE message must be a JSON array.");
        }

        if (root.GetArrayLength() < 2)
        {
            throw new FormatException("EOSE message missing subscription id.");
        }

        var messageType = root[0].GetString();
        if (!string.Equals(messageType, "EOSE", StringComparison.Ordinal))
        {
            throw new FormatException("JSON message is not an EOSE type.");
        }

        var subscriptionId = root[1].GetString();
        if (string.IsNullOrWhiteSpace(subscriptionId))
        {
            throw new FormatException("EOSE message must include a subscription id.");
        }

        return new NostrEndOfStoredEventsMessage(subscriptionId);
    }

    public NostrClosedMessage DeserializeClosed(ReadOnlySpan<byte> json)
    {
        using var document = JsonDocument.Parse(json.ToArray());
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Array)
        {
            throw new FormatException("CLOSED message must be a JSON array.");
        }

        if (root.GetArrayLength() < 2)
        {
            throw new FormatException("CLOSED message missing subscription id.");
        }

        var messageType = root[0].GetString();
        if (!string.Equals(messageType, "CLOSED", StringComparison.Ordinal))
        {
            throw new FormatException("JSON message is not a CLOSED type.");
        }

        var subscriptionId = root[1].GetString();
        if (string.IsNullOrWhiteSpace(subscriptionId))
        {
            throw new FormatException("CLOSED message must include a subscription id.");
        }

        var reason = root.GetArrayLength() >= 3 ? root[2].GetString() ?? string.Empty : string.Empty;
        return new NostrClosedMessage(subscriptionId, reason);
    }

    private static void WriteEventObject(Utf8JsonWriter writer, NostrEventDto @event)
    {
        writer.WriteStartObject();
        writer.WriteString("id", @event.Id);
        writer.WriteString("pubkey", @event.PublicKey);
        writer.WriteNumber("created_at", @event.CreatedAt);
        writer.WriteNumber("kind", @event.Kind);
        writer.WritePropertyName("tags");
        writer.WriteStartArray();
        foreach (var tag in @event.Tags)
        {
            writer.WriteStartArray();
            foreach (var value in tag)
            {
                writer.WriteStringValue(value);
            }
            writer.WriteEndArray();
        }
        writer.WriteEndArray();
        writer.WriteString("content", @event.Content);
        writer.WriteString("sig", @event.Signature);
        writer.WriteEndObject();
    }

    private static void WriteFilterObject(Utf8JsonWriter writer, NostrFilterDto filter)
    {
        writer.WriteStartObject();
        if (filter.Ids.Count > 0)
        {
            writer.WritePropertyName("ids");
            WriteStringArray(writer, filter.Ids);
        }

        if (filter.Authors.Count > 0)
        {
            writer.WritePropertyName("authors");
            WriteStringArray(writer, filter.Authors);
        }

        if (filter.Kinds.Count > 0)
        {
            writer.WritePropertyName("kinds");
            writer.WriteStartArray();
            foreach (var kind in filter.Kinds)
            {
                writer.WriteNumberValue(kind);
            }
            writer.WriteEndArray();
        }

        if (filter.Since.HasValue)
        {
            writer.WriteNumber("since", filter.Since.Value);
        }

        if (filter.Until.HasValue)
        {
            writer.WriteNumber("until", filter.Until.Value);
        }

        if (filter.Limit.HasValue)
        {
            writer.WriteNumber("limit", filter.Limit.Value);
        }

        if (filter.Tags.Count > 0)
        {
            foreach (var pair in filter.Tags.OrderBy(static entry => entry.Key, StringComparer.Ordinal))
            {
                writer.WritePropertyName(pair.Key);
                WriteStringArray(writer, pair.Value);
            }
        }

        writer.WriteEndObject();
    }

    private static void WriteStringArray(Utf8JsonWriter writer, IReadOnlyList<string> values)
    {
        writer.WriteStartArray();
        foreach (var value in values)
        {
            writer.WriteStringValue(value ?? string.Empty);
        }
        writer.WriteEndArray();
    }

    private static NostrEventDto ReadEventObject(JsonElement element)
    {
        if (element.ValueKind != JsonValueKind.Object)
        {
            throw new FormatException("EVENT payload must be a JSON object.");
        }

        var id = GetRequiredString(element, "id");
        var pubkey = GetRequiredString(element, "pubkey");
        var createdAt = GetRequiredInt64(element, "created_at");
        var kind = GetRequiredInt32(element, "kind");
        var tags = ReadTags(element);
        var content = element.TryGetProperty("content", out var contentElement) ? contentElement.GetString() ?? string.Empty : string.Empty;
        var signature = GetRequiredString(element, "sig");
        return new NostrEventDto(id, pubkey, createdAt, kind, tags, content, signature);
    }

    private static IReadOnlyList<IReadOnlyList<string>> ReadTags(JsonElement element)
    {
        if (!element.TryGetProperty("tags", out var tagsElement))
        {
            return Array.Empty<IReadOnlyList<string>>();
        }

        if (tagsElement.ValueKind != JsonValueKind.Array)
        {
            throw new FormatException("EVENT tags must be a JSON array.");
        }

        var result = new List<IReadOnlyList<string>>(tagsElement.GetArrayLength());
        foreach (var tagArray in tagsElement.EnumerateArray())
        {
            if (tagArray.ValueKind != JsonValueKind.Array)
            {
                throw new FormatException("EVENT tag entries must be arrays of strings.");
            }

            var values = new List<string>(tagArray.GetArrayLength());
            foreach (var value in tagArray.EnumerateArray())
            {
                values.Add(value.GetString() ?? string.Empty);
            }

            result.Add(values);
        }

        return result;
    }

    private static string GetRequiredString(JsonElement element, string propertyName)
    {
        if (!element.TryGetProperty(propertyName, out var property) || property.ValueKind is JsonValueKind.Null or JsonValueKind.Undefined)
        {
            throw new FormatException($"EVENT payload missing '{propertyName}' property.");
        }

        var value = property.GetString();
        if (string.IsNullOrEmpty(value))
        {
            throw new FormatException($"EVENT payload property '{propertyName}' must not be empty.");
        }

        return value;
    }

    private static int GetRequiredInt32(JsonElement element, string propertyName)
    {
        if (!element.TryGetProperty(propertyName, out var property))
        {
            throw new FormatException($"EVENT payload missing '{propertyName}' property.");
        }

        if (property.ValueKind == JsonValueKind.Number)
        {
            if (property.TryGetInt32(out var intValue))
            {
                return intValue;
            }

            if (property.TryGetInt64(out var longValue))
            {
                if (longValue is < int.MinValue or > int.MaxValue)
                {
                    throw new FormatException($"EVENT payload property '{propertyName}' exceeds 32-bit integer range.");
                }

                return (int)longValue;
            }
        }

        throw new FormatException($"EVENT payload property '{propertyName}' is not a valid integer.");
    }

    private static long GetRequiredInt64(JsonElement element, string propertyName)
    {
        if (!element.TryGetProperty(propertyName, out var property))
        {
            throw new FormatException($"EVENT payload missing '{propertyName}' property.");
        }

        if (property.ValueKind == JsonValueKind.Number)
        {
            if (property.TryGetInt64(out var longValue))
            {
                return longValue;
            }

            if (property.TryGetDouble(out var doubleValue))
            {
                if (double.IsNaN(doubleValue) || double.IsInfinity(doubleValue))
                {
                    throw new FormatException($"EVENT payload property '{propertyName}' is not a finite number.");
                }

                if (doubleValue > long.MaxValue || doubleValue < long.MinValue)
                {
                    throw new FormatException($"EVENT payload property '{propertyName}' is outside of Int64 bounds.");
                }

                return (long)Math.Floor(doubleValue);
            }
        }

        throw new FormatException($"EVENT payload property '{propertyName}' is not a valid integer.");
    }
}
