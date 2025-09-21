using System;
using System.Buffers;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Encodings.Web;
using System.Text.Json;
using NBitcoin.Secp256k1;

namespace COM_Nostr.Contracts;

[ComVisible(true)]
[Guid("ae9df2b5-8650-4a51-8bb2-1df35a48a6ec")]
[ProgId("COM_Nostr.NostrSigner")]
[ClassInterface(ClassInterfaceType.None)]
public sealed class NostrSigner : INostrSigner
{
    private const string PrivateKeyEnvVar = "NOSTR_PRIVATE_KEY";

    private readonly ECPrivKey _privateKey;
    private readonly ECXOnlyPubKey _publicKey;
    private readonly string _publicKeyHex;

    public NostrSigner()
        : this(ReadPrivateKeyFromEnvironment())
    {
    }

    public NostrSigner(string privateKey)
    {
        if (string.IsNullOrWhiteSpace(privateKey))
        {
            throw new ArgumentException("Private key must not be empty.", nameof(privateKey));
        }

        var keyBytes = ParsePrivateKey(privateKey);
        _privateKey = ECPrivKey.Create(keyBytes);
        _publicKey = _privateKey.CreateXOnlyPubKey(out _);

        Span<byte> pubKeyBytes = stackalloc byte[32];
        _publicKey.WriteToSpan(pubKeyBytes);
        _publicKeyHex = Convert.ToHexString(pubKeyBytes).ToLowerInvariant();
    }

    public string Sign(NostrEventDraft draft)
    {
        if (draft is null)
        {
            throw new ArgumentNullException(nameof(draft));
        }

        ValidateDraft(draft);

        if (!string.IsNullOrEmpty(draft.PublicKey) &&
            !draft.PublicKey.Equals(_publicKeyHex, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Draft public key does not match signer public key.");
        }

        draft.PublicKey = _publicKeyHex;
        draft.Content ??= string.Empty;

        var normalizedTags = NormalizeTags(draft.Tags);
        var serialized = SerializeEvent(draft, _publicKeyHex, normalizedTags);
        var eventIdBytes = System.Security.Cryptography.SHA256.HashData(serialized);
        var signature = _privateKey.SignBIP340(eventIdBytes);
        var signatureBytes = signature.ToBytes();
        return Convert.ToHexString(signatureBytes).ToLowerInvariant();
    }

    public string GetPublicKey() => _publicKeyHex;

    internal static string ComputeEventId(NostrEventDraft draft, string publicKeyHex)
    {
        if (draft is null)
        {
            throw new ArgumentNullException(nameof(draft));
        }

        if (string.IsNullOrWhiteSpace(publicKeyHex))
        {
            throw new ArgumentException("Public key must not be empty.", nameof(publicKeyHex));
        }

        var normalizedTags = NormalizeTags(draft.Tags);
        var serialized = SerializeEvent(draft, publicKeyHex.ToLowerInvariant(), normalizedTags);
        var eventIdBytes = System.Security.Cryptography.SHA256.HashData(serialized);
        return Convert.ToHexString(eventIdBytes).ToLowerInvariant();
    }

    private static string ReadPrivateKeyFromEnvironment()
    {
        var value = Environment.GetEnvironmentVariable(PrivateKeyEnvVar);
        if (string.IsNullOrWhiteSpace(value))
        {
            throw new InvalidOperationException($"Environment variable '{PrivateKeyEnvVar}' is not set.");
        }

        return value;
    }

    private static byte[] ParsePrivateKey(string value)
    {
        var sanitized = value.Trim();
        if (sanitized.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
        {
            sanitized = sanitized[2..];
        }

        if (sanitized.Length != 64)
        {
            throw new ArgumentException("Private key must be a 32-byte hex string.", nameof(value));
        }

        if (!IsHex(sanitized))
        {
            throw new ArgumentException("Private key contains invalid characters.", nameof(value));
        }

        return Convert.FromHexString(sanitized);
    }

    private static bool IsHex(string value)
    {
        foreach (var c in value)
        {
            var isDigit = c is >= '0' and <= '9';
            var isLower = c is >= 'a' and <= 'f';
            var isUpper = c is >= 'A' and <= 'F';
            if (!isDigit && !isLower && !isUpper)
            {
                return false;
            }
        }

        return true;
    }

    private static void ValidateDraft(NostrEventDraft draft)
    {
        if (double.IsNaN(draft.CreatedAt) || double.IsInfinity(draft.CreatedAt) || draft.CreatedAt <= 0)
        {
            throw new ArgumentException("Draft.CreatedAt must be a valid UNIX timestamp.", nameof(draft));
        }
    }

    private static IReadOnlyList<string[]> NormalizeTags(object[]? tags)
    {
        if (tags is null || tags.Length == 0)
        {
            return Array.Empty<string[]>();
        }

        var result = new List<string[]>(tags.Length);
        foreach (var tag in tags)
        {
            if (tag is null)
            {
                throw new ArgumentException("Tags must not contain null entries.");
            }

            switch (tag)
            {
                case string[] stringArray:
                    result.Add(NormalizeStringArray(stringArray));
                    break;
                case object[] objectArray:
                    result.Add(NormalizeObjectArray(objectArray));
                    break;
                default:
                    throw new ArgumentException("Each tag must be an array of strings.");
            }
        }

        return result;
    }

    private static string[] NormalizeStringArray(string[] source)
    {
        var copy = new string[source.Length];
        for (var i = 0; i < source.Length; i++)
        {
            copy[i] = source[i] ?? string.Empty;
        }

        return copy;
    }

    private static string[] NormalizeObjectArray(object[] source)
    {
        var copy = new string[source.Length];
        for (var i = 0; i < source.Length; i++)
        {
            copy[i] = source[i]?.ToString() ?? string.Empty;
        }

        return copy;
    }

    private static byte[] SerializeEvent(NostrEventDraft draft, string publicKeyHex, IReadOnlyList<string[]> tags)
    {
        var buffer = new ArrayBufferWriter<byte>();
        using (var writer = new Utf8JsonWriter(buffer, new JsonWriterOptions
        {
            Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
            Indented = false
        }))
        {
            writer.WriteStartArray();
            writer.WriteNumberValue(0);
            writer.WriteStringValue(publicKeyHex);
            writer.WriteNumberValue(ToUnixSeconds(draft.CreatedAt));
            writer.WriteNumberValue(draft.Kind);
            writer.WriteStartArray();
            foreach (var tag in tags)
            {
                writer.WriteStartArray();
                foreach (var value in tag)
                {
                    writer.WriteStringValue(value);
                }
                writer.WriteEndArray();
            }
            writer.WriteEndArray();
            writer.WriteStringValue(draft.Content ?? string.Empty);
            writer.WriteEndArray();
            writer.Flush();
        }

        var written = buffer.WrittenSpan;
        var result = new byte[written.Length];
        written.CopyTo(result);
        return result;
    }

    private static long ToUnixSeconds(double value)
    {
        if (value > long.MaxValue)
        {
            throw new ArgumentOutOfRangeException(nameof(value), "CreatedAt exceeds UNIX timestamp range.");
        }

        return (long)Math.Floor(value);
    }
}
