using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using COM_Nostr.Contracts;

namespace COM_Nostr.Internal;

internal static class NostrFilterConverter
{
    public static IReadOnlyList<NostrFilterDto> ToDtos(NostrFilter[] filters)
    {
        if (filters is null || filters.Length == 0)
        {
            return Array.Empty<NostrFilterDto>();
        }

        var result = new List<NostrFilterDto>(filters.Length);
        foreach (var filter in filters)
        {
            result.Add(ConvertFilter(filter ?? throw new ArgumentNullException(nameof(filters), "Filter entry must not be null.")));
        }

        return result;
    }

    private static NostrFilterDto ConvertFilter(NostrFilter filter)
    {
        var ids = NormalizeStringArray(filter.Ids);
        var authors = NormalizeStringArray(filter.Authors);
        var kinds = NormalizeKinds(filter.Kinds);
        var tags = NormalizeTags(filter.Tags);
        var since = NormalizeTimestamp(filter.Since, nameof(NostrFilter.Since));
        var until = NormalizeTimestamp(filter.Until, nameof(NostrFilter.Until));
        var limit = NormalizeLimit(filter.Limit);

        return new NostrFilterDto(ids, authors, kinds, tags, since, until, limit);
    }

    private static IReadOnlyList<string> NormalizeStringArray(string[]? values)
    {
        if (values is null || values.Length == 0)
        {
            return Array.Empty<string>();
        }

        var result = new List<string>(values.Length);
        foreach (var value in values)
        {
            if (string.IsNullOrWhiteSpace(value))
            {
                continue;
            }

            result.Add(value.Trim());
        }

        return result;
    }

    private static IReadOnlyList<int> NormalizeKinds(int[]? kinds)
    {
        if (kinds is null || kinds.Length == 0)
        {
            return Array.Empty<int>();
        }

        var result = new List<int>(kinds.Length);
        foreach (var kind in kinds)
        {
            result.Add(kind);
        }

        return result;
    }

    private static IReadOnlyDictionary<string, IReadOnlyList<string>> NormalizeTags(NostrTagQuery[]? tags)
    {
        if (tags is null || tags.Length == 0)
        {
            return new Dictionary<string, IReadOnlyList<string>>(StringComparer.OrdinalIgnoreCase);
        }

        var result = new Dictionary<string, IReadOnlyList<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var tag in tags)
        {
            if (tag is null)
            {
                continue;
            }

            var label = NormalizeTagLabel(tag.Label);
            if (label is null)
            {
                continue;
            }

            var values = NormalizeStringArray(tag.Values);
            if (values.Count == 0)
            {
                continue;
            }

            result[label] = values;
        }

        return result;
    }

    private static string? NormalizeTagLabel(string? label)
    {
        if (string.IsNullOrWhiteSpace(label))
        {
            return null;
        }

        var trimmed = label.Trim();
        if (!trimmed.StartsWith("#", StringComparison.Ordinal))
        {
            trimmed = "#" + trimmed;
        }

        return trimmed.ToLowerInvariant();
    }

    private static long? NormalizeTimestamp(object? value, string propertyName)
    {
        if (value is null)
        {
            return null;
        }

        switch (value)
        {
            case int intValue:
                return intValue;
            case long longValue:
                return longValue;
            case double doubleValue:
                if (double.IsNaN(doubleValue) || double.IsInfinity(doubleValue))
                {
                    throw new ArgumentException($"{propertyName} must be a finite number.", propertyName);
                }

                return checked((long)Math.Floor(doubleValue));
            case float floatValue:
                if (float.IsNaN(floatValue) || float.IsInfinity(floatValue))
                {
                    throw new ArgumentException($"{propertyName} must be a finite number.", propertyName);
                }

                return checked((long)Math.Floor(floatValue));
            case string text:
                if (string.IsNullOrWhiteSpace(text))
                {
                    return null;
                }

                if (double.TryParse(text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
                {
                    if (double.IsNaN(parsed) || double.IsInfinity(parsed))
                    {
                        throw new ArgumentException($"{propertyName} must be a finite number.", propertyName);
                    }

                    return checked((long)Math.Floor(parsed));
                }

                throw new ArgumentException($"{propertyName} must be parsable as a number.", propertyName);
            default:
                throw new ArgumentException($"Unsupported value type for {propertyName}.", propertyName);
        }
    }

    private static int? NormalizeLimit(object? value)
    {
        if (value is null)
        {
            return null;
        }

        switch (value)
        {
            case int intValue:
                return ValidateLimit(intValue);
            case long longValue:
                return ValidateLimit(checked((int)longValue));
            case double doubleValue:
                if (double.IsNaN(doubleValue) || double.IsInfinity(doubleValue))
                {
                    throw new ArgumentException($"{nameof(NostrFilter.Limit)} must be a finite number.", nameof(NostrFilter.Limit));
                }

                return ValidateLimit((int)Math.Floor(doubleValue));
            case float floatValue:
                if (float.IsNaN(floatValue) || float.IsInfinity(floatValue))
                {
                    throw new ArgumentException($"{nameof(NostrFilter.Limit)} must be a finite number.", nameof(NostrFilter.Limit));
                }

                return ValidateLimit((int)Math.Floor(floatValue));
            case string text:
                if (string.IsNullOrWhiteSpace(text))
                {
                    return null;
                }

                if (double.TryParse(text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
                {
                    if (double.IsNaN(parsed) || double.IsInfinity(parsed))
                    {
                        throw new ArgumentException($"{nameof(NostrFilter.Limit)} must be a finite number.", nameof(NostrFilter.Limit));
                    }

                    return ValidateLimit((int)Math.Floor(parsed));
                }

                throw new ArgumentException($"{nameof(NostrFilter.Limit)} must be parsable as a number.", nameof(NostrFilter.Limit));
            default:
                throw new ArgumentException($"Unsupported value type for {nameof(NostrFilter.Limit)}.", nameof(NostrFilter.Limit));
        }
    }

    private static int? ValidateLimit(int value)
    {
        if (value <= 0)
        {
            return null;
        }

        return value;
    }
}
