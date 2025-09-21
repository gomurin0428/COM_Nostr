using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using COM_Nostr.Contracts;
using COM_Nostr.Internal;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrClientInitializationTests
{
    [TestMethod]
    public async Task Initialize_WithNullOptions_UsesDefaults()
    {
        var client = new NostrClient();

        client.Initialize(null!);

        Assert.IsTrue(client.IsInitialized);
        var resources = client.CurrentResources;
        Assert.IsNotNull(resources);
        Assert.AreEqual("COM_Nostr/1.0", resources.Options.UserAgent);
        Assert.IsNull(resources.Options.ConnectTimeout);
        Assert.IsNull(resources.Options.SendTimeout);
        Assert.IsNull(resources.Options.ReceiveTimeout);

        using (var httpClient = resources.HttpClientFactory())
        {
            var userAgent = string.Join(" ", httpClient.DefaultRequestHeaders.UserAgent);
            Assert.AreEqual("COM_Nostr/1.0", userAgent);
        }

        await using (var connection = resources.WebSocketFactory())
        {
            Assert.IsNotNull(connection);
            Assert.IsInstanceOfType(connection, typeof(ClientWebSocketConnection));
        }
    }

    [TestMethod]
    public void Initialize_WithStringTimeouts_ParsesSuccessfully()
    {
        var options = new ClientOptions
        {
            ConnectTimeoutSeconds = "15.25",
            SendTimeoutSeconds = 20,
            ReceiveTimeoutSeconds = "45",
            UserAgent = "ExampleClient/2.0"
        };

        var client = new NostrClient();
        client.Initialize(options);

        var resources = client.CurrentResources;
        Assert.IsNotNull(resources);
        Assert.AreEqual(TimeSpan.FromSeconds(15.25), resources.Options.ConnectTimeout);
        Assert.AreEqual(TimeSpan.FromSeconds(20), resources.Options.SendTimeout);
        Assert.AreEqual(TimeSpan.FromSeconds(45), resources.Options.ReceiveTimeout);
        Assert.AreEqual("ExampleClient/2.0", resources.Options.UserAgent);

        using (var httpClient = resources.HttpClientFactory())
        {
            var userAgent = string.Join(" ", httpClient.DefaultRequestHeaders.UserAgent);
            Assert.AreEqual("ExampleClient/2.0", userAgent);
            Assert.AreEqual(TimeSpan.FromSeconds(45), httpClient.Timeout);
        }
    }

    [TestMethod]
    public void Initialize_WithInvalidProgId_ThrowsComException()
    {
        var options = new ClientOptions
        {
            WebSocketFactoryProgId = "Nonexistent.ProgId"
        };

        var client = new NostrClient();
        var exception = Assert.ThrowsException<COMException>(() => client.Initialize(options));
        Assert.AreEqual(unchecked((int)0x80040154), exception.ErrorCode);
    }

    [TestMethod]
    public void Initialize_WithNegativeTimeout_ThrowsInvalidArg()
    {
        var options = new ClientOptions
        {
            ReceiveTimeoutSeconds = -1
        };

        var client = new NostrClient();
        var exception = Assert.ThrowsException<COMException>(() => client.Initialize(options));
        Assert.AreEqual(unchecked((int)0x80070057), exception.ErrorCode);
    }

    [TestMethod]
    public void Initialize_WithInvalidUserAgent_ThrowsInvalidArg()
    {
        var options = new ClientOptions
        {
            UserAgent = "Invalid\r\nAgent"
        };

        var client = new NostrClient();
        var exception = Assert.ThrowsException<COMException>(() => client.Initialize(options));
        Assert.AreEqual(unchecked((int)0x80070057), exception.ErrorCode);
    }
}
