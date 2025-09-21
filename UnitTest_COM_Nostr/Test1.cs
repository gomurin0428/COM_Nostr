using System;
using COM_Nostr.Contracts;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NBitcoin.Secp256k1;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrSignerTests
{
    private const string SecretKeyHex = "0000000000000000000000000000000000000000000000000000000000000001";
    private const string ExpectedPublicKey = "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798";
    private const string ExpectedEventId = "b259531af662b8c583fd2e68b58ccace3fc1179b0f8efe175fb810adbca2c152";

    [TestMethod]
    public void Sign_ComputesValidSignatureAndUpdatesDraft()
    {
        var signer = new NostrSigner(SecretKeyHex);
        var draft = new NostrEventDraft
        {
            CreatedAt = 1700000000,
            Kind = 1,
            Tags = Array.Empty<object>(),
            Content = "Hello from Codex"
        };

        var signatureHex = signer.Sign(draft);

        Assert.AreEqual(ExpectedPublicKey, signer.GetPublicKey());
        Assert.AreEqual(ExpectedPublicKey, draft.PublicKey);
        Assert.AreEqual(128, signatureHex.Length);
        Assert.IsTrue(IsHex(signatureHex));

        var eventId = NostrSigner.ComputeEventId(draft, signer.GetPublicKey());
        Assert.AreEqual(ExpectedEventId, eventId);

        Assert.IsTrue(ECXOnlyPubKey.TryCreate(Convert.FromHexString(signer.GetPublicKey()), out var pubKey));
        Assert.IsTrue(SecpSchnorrSignature.TryCreate(Convert.FromHexString(signatureHex), out var schnorr));
        Assert.IsTrue(pubKey.SigVerifyBIP340(schnorr, Convert.FromHexString(eventId)));
    }

    [TestMethod]
    public void Sign_WithMismatchedDraftPublicKey_Throws()
    {
        var signer = new NostrSigner(SecretKeyHex);
        var draft = new NostrEventDraft
        {
            CreatedAt = 1700000000,
            Kind = 1,
            Content = "Hello from Codex",
            Tags = Array.Empty<object>(),
            PublicKey = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        };

        Assert.ThrowsException<InvalidOperationException>(() => signer.Sign(draft));
    }

    [TestMethod]
    public void ComputeEventId_HandlesNestedTags()
    {
        var signer = new NostrSigner(SecretKeyHex);
        var draft = new NostrEventDraft
        {
            CreatedAt = 1700000100,
            Kind = 7,
            Tags = new object[]
            {
                new[] { "e", "abc123" },
                new object[] { "p", ExpectedPublicKey, "wss://relay.example.com" }
            },
            Content = "Tagged content"
        };

        var eventId = NostrSigner.ComputeEventId(draft, signer.GetPublicKey());
        Assert.AreEqual(64, eventId.Length);
        Assert.IsTrue(IsHex(eventId));

        var signatureHex = signer.Sign(draft);
        Assert.IsTrue(SecpSchnorrSignature.TryCreate(Convert.FromHexString(signatureHex), out var schnorr));
        Assert.IsTrue(ECXOnlyPubKey.TryCreate(Convert.FromHexString(signer.GetPublicKey()), out var pubKey));
        Assert.IsTrue(pubKey.SigVerifyBIP340(schnorr, Convert.FromHexString(eventId)));
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
}
