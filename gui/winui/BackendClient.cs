using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Microsoft.UI.Dispatching;

namespace SoundConnect.WinUI;

// Private redirected pipes keep the existing Qt event loop in its own process.
// Device state and command validation remain in the shared C++ ViewModels.
internal sealed class BackendClient : IDisposable
{
    private readonly DispatcherQueue dispatcher;
    private Process? process;
    private bool disposed;
    public JsonObject State { get; private set; } = new();
    public string Error { get; private set; } = "";
    public int SentCommandCount { get; private set; }
    public event Action? Changed;
    public bool Connected => Text("connection", "state") == "connected";
    public bool Flag(string group, string key) => State[group]?[key]?.GetValue<bool>() ?? false;
    public int Number(string group, string key) => State[group]?[key]?.GetValue<int>() ?? 0;
    public string Text(string group, string key) => State[group]?[key]?.GetValue<string>() ?? "";

    public BackendClient(DispatcherQueue dispatcher) { this.dispatcher = dispatcher; }

    public void Start(string path, bool live, string profile)
    {
        try
        {
            if (!File.Exists(path)) throw new FileNotFoundException("Backend executable not found", path);
            var info = new ProcessStartInfo(path) {
                UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true,
                WorkingDirectory = Path.GetDirectoryName(Path.GetFullPath(path))!
            };
            if (live) info.ArgumentList.Add("--live");
            else { info.ArgumentList.Add("--profile"); info.ArgumentList.Add(profile); }
            process = new Process { StartInfo = info };
            process.Start();
            _ = ReadOutputAsync(process);
            // Drain diagnostics so a verbose backend cannot block on stderr.
            _ = process.StandardError.ReadToEndAsync();
        }
        catch (Exception e) { Error = e.Message; Changed?.Invoke(); }
    }

    private async Task ReadOutputAsync(Process source)
    {
        try
        {
            while (await source.StandardOutput.ReadLineAsync() is string line)
            {
                JsonObject? data;
                try { data = JsonNode.Parse(line) as JsonObject; }
                catch (JsonException) { continue; }
                if (data is null) continue;
                dispatcher.TryEnqueue(() => {
                    if (disposed) return;
                    if (data["type"]?.GetValue<string>() == "state") State = data;
                    else if (data["type"]?.GetValue<string>() == "result")
                        Error = data["ok"]?.GetValue<bool>() == true ? "" : data["error"]?.GetValue<string>() ?? "Command rejected";
                    Changed?.Invoke();
                });
            }
            dispatcher.TryEnqueue(() => {
                if (!disposed) { Error = "Backend stopped"; State = new(); Changed?.Invoke(); }
            });
        }
        catch (Exception e) when (e is IOException or ObjectDisposedException or InvalidOperationException)
        {
            dispatcher.TryEnqueue(() => { if (!disposed) { Error = e.Message; Changed?.Invoke(); } });
        }
    }

    public void Send(string target, string method, params object[] args)
    {
        if (disposed || process is null) return;
        try
        {
            SentCommandCount++;
            process.StandardInput.WriteLine(JsonSerializer.Serialize(new { version = 1, id = SentCommandCount.ToString(), target, method, args }));
            process.StandardInput.Flush();
        }
        catch (Exception e) when (e is IOException or InvalidOperationException)
        { Error = e.Message; Changed?.Invoke(); }
    }

    public void Dispose()
    {
        disposed = true;
        if (process is null) return;
        try
        {
            process.StandardInput.Close(); // EOF requests orderly Qt/backend shutdown.
            if (!process.WaitForExit(2000)) process.Kill(entireProcessTree: true);
        }
        catch (InvalidOperationException) { }
        process.Dispose();
    }
}
