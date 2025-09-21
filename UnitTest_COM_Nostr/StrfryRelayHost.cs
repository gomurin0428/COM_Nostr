using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Threading.Tasks;

namespace UnitTest_COM_Nostr;

internal sealed class StrfryRelayHost : IAsyncDisposable
{
    private string _containerName;
    private readonly int _hostPort;
    private readonly string _volumePath;
    private readonly string _policyMount;

    private StrfryRelayHost(string containerName, int hostPort, string volumePath, string policyMount)
    {
        _containerName = containerName;
        _hostPort = hostPort;
        _volumePath = volumePath;
        _policyMount = policyMount;
    }

    public async Task RestartAsync()
    {
        await RunDockerAsync(new[] { "stop", _containerName }, throwOnError: false).ConfigureAwait(false);

        var newContainer = $"strfry-test-{Guid.NewGuid():N}";

        try
        {
            await RunDockerAsync(new[]
            {
                "run",
                "--rm",
                "-d",
                "-p",
                $"{_hostPort}:7777",
                "-v",
                $"{_volumePath}:/app/strfry-db",
                "-v",
                _policyMount,
                "--name",
                newContainer,
                "dockurr/strfry"
            }).ConfigureAwait(false);
        }
        catch
        {
            _containerName = newContainer;
            throw;
        }

        _containerName = newContainer;
        await WaitUntilReadyAsync(_hostPort).ConfigureAwait(false);
    }

    public string RelayWebSocketUrl => $"ws://127.0.0.1:{_hostPort}";

    public static async Task<StrfryRelayHost> StartAsync()
    {
        var port = GetFreeTcpPort();
        var containerName = $"strfry-test-{Guid.NewGuid():N}";
        var volumePath = Path.Combine(Path.GetTempPath(), containerName);
        Directory.CreateDirectory(volumePath);

        var policyPath = Path.Combine(volumePath, "write-policy.py");
        var policyScript = "#!/usr/bin/env python3\nimport json, sys\nfor line in sys.stdin:\n    request = json.loads(line)\n    event = request.get('event', {})\n    response = {'id': event.get('id', ''), 'action': 'accept'}\n    json.dump(response, sys.stdout, separators=(',', ':'))\n    sys.stdout.write('\\n')\n    sys.stdout.flush()\n";
        await File.WriteAllTextAsync(policyPath, policyScript).ConfigureAwait(false);

        var policyMount = $"{policyPath.Replace("\\", "/")}:/app/write-policy.py";

        await RunDockerAsync(new[]
        {
            "run",
            "--rm",
            "-d",
            "-p",
            $"{port}:7777",
            "-v",
            $"{volumePath}:/app/strfry-db",
            "-v",
            policyMount,
            "--name",
            containerName,
            "dockurr/strfry"
        }).ConfigureAwait(false);

        await WaitUntilReadyAsync(port).ConfigureAwait(false);

        return new StrfryRelayHost(containerName, port, volumePath, policyMount);
    }

    public async ValueTask DisposeAsync()
    {
        await RunDockerAsync(new[] { "stop", _containerName }, throwOnError: false).ConfigureAwait(false);

        try
        {
            if (Directory.Exists(_volumePath))
            {
                Directory.Delete(_volumePath, recursive: true);
            }
        }
        catch
        {
            // Ignore cleanup errors.
        }
    }

    private static async Task RunDockerAsync(IReadOnlyList<string> arguments, bool throwOnError = true)
    {
        var psi = new ProcessStartInfo("docker")
        {
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        foreach (var argument in arguments)
        {
            psi.ArgumentList.Add(argument);
        }

        using var process = Process.Start(psi) ?? throw new InvalidOperationException("Failed to start docker process.");
        var stdout = await process.StandardOutput.ReadToEndAsync().ConfigureAwait(false);
        var stderr = await process.StandardError.ReadToEndAsync().ConfigureAwait(false);
        await process.WaitForExitAsync().ConfigureAwait(false);

        if (throwOnError && process.ExitCode != 0)
        {
            throw new InvalidOperationException($"docker {(arguments.Count > 0 ? arguments[0] : string.Empty)} failed with exit code {process.ExitCode}. StdOut: {stdout}. StdErr: {stderr}.");
        }
    }

    private static async Task WaitUntilReadyAsync(int port)
    {
        using var client = new HttpClient();
        client.DefaultRequestHeaders.Accept.ParseAdd("application/nostr+json");
        var endpoint = new Uri($"http://127.0.0.1:{port}");

        for (var attempt = 0; attempt < 20; attempt++)
        {
            try
            {
                var response = await client.GetAsync(endpoint).ConfigureAwait(false);
                if (response.IsSuccessStatusCode)
                {
                    return;
                }
            }
            catch
            {
                // ignore transient failures
            }

            await Task.Delay(TimeSpan.FromMilliseconds(500)).ConfigureAwait(false);
        }

        throw new TimeoutException($"Timed out waiting for strfry relay on port {port}.");
    }

    private static int GetFreeTcpPort()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        try
        {
            return ((IPEndPoint)listener.LocalEndpoint).Port;
        }
        finally
        {
            listener.Stop();
        }
    }
}

