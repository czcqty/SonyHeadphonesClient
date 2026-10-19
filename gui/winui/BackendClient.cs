using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace SoundConnect.WinUI;

// Transport only. Device behavior and validation belong to SoundConnectBackend.
internal sealed class BackendClient : IDisposable
{
    private sealed class Session
    {
        internal Process? Process;
        internal readonly CancellationTokenSource Stop = new();
        internal readonly Channel<string> Outgoing = Channel.CreateBounded<string>(64);
        internal string Diagnostic = "";
        internal int DiagnosticPending;
        internal Task? Cleanup;
        internal readonly Dictionary<string, TaskCompletionSource<JsonObject>> Requests = new();
        internal readonly Dictionary<string, JsonObject> Results = new();
    }
    private readonly Action<Action> dispatch;
    private Session? session;
    private string executable = "";
    private string[] arguments = Array.Empty<string>();
    private bool disposed;
    public JsonObject State { get; private set; } = new();
    public string Error { get; private set; } = "";
    public string Diagnostic { get; private set; } = "";
    public bool Ready { get; private set; }
    public int SentCommandCount { get; private set; }
    public long StateRevision { get; private set; }
    internal string ExecutablePath => executable;
    public TimeSpan StartupTimeout { get; set; } = TimeSpan.FromSeconds(10);
    internal TimeSpan CommandTimeout { get; set; } = TimeSpan.FromSeconds(10);
    public event Action? Changed;
    public bool Connected => Ready && Text("connection", "state") == "connected";
    public bool Flag(string group, string key) => State[group]?[key]?.GetValue<bool>() ?? false;
    public int Number(string group, string key) => State[group]?[key]?.GetValue<int>() ?? 0;
    public string Text(string group, string key) => State[group]?[key]?.GetValue<string>() ?? "";

    public BackendClient(Action<Action> dispatch) { this.dispatch = dispatch; }

    public void Start(string path, bool live, string profile, string locale, string theme)
    {
        var args = new System.Collections.Generic.List<string>();
        if (live) args.Add("--live"); else { args.Add("--profile"); args.Add(profile); }
        if (locale.Length > 0) { args.Add("--locale"); args.Add(locale); }
        if (theme.Length > 0) { args.Add("--theme"); args.Add(theme); }
        Start(path, args.ToArray());
    }
    internal void Start(string path, string[] args)
    {
        executable = path; arguments = args; Restart();
    }
    private void Post(Session source, Action action)
    {
        dispatch(() => {
            if (disposed || session != source || source.Stop.IsCancellationRequested) return;
            try { action(); }
            catch (Exception error) when (error is InvalidOperationException or JsonException or FormatException)
            { Fail(source, "backend-protocol-error", error.Message); }
        });
    }
    public void Restart()
    {
        if (disposed) return;
        var old = session;
        if (old is not null) CancelRequests(old);
        var source = session = new Session();
        State = new(); Ready = false; Error = "backend-starting"; Diagnostic = "";
        Changed?.Invoke();
        _ = CheckTimeoutAsync(source);
        _ = Task.Run(async () => {
            if (old is not null) await RetireAsync(old, true);
            if (disposed || session != source || source.Stop.IsCancellationRequested) return;
            try {
                if (!File.Exists(executable)) throw new FileNotFoundException("Backend executable not found", executable);
                var info = new ProcessStartInfo(executable) {
                    UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true,
                    WorkingDirectory = Path.GetDirectoryName(Path.GetFullPath(executable))!
                };
                foreach (var argument in arguments) info.ArgumentList.Add(argument);
                var process = new Process { StartInfo = info };
                lock (source) {
                    if (source.Stop.IsCancellationRequested) { process.Dispose(); return; }
                    process.Start(); source.Process = process;
                }
                _ = WriteAsync(source);
                var diagnostics = DiagnosticsAsync(source);
                await ReadAsync(source);
                await Task.WhenAny(diagnostics, Task.Delay(100));
                Post(source, () => Fail(source, "backend-stopped", source.Diagnostic));
            } catch (Exception error) when (error is IOException or InvalidOperationException or System.ComponentModel.Win32Exception) {
                Post(source, () => Fail(source, "backend-start-failed", error.Message));
            }
        });
    }
    private async Task CheckTimeoutAsync(Session source)
    {
        try {
            await Task.Delay(StartupTimeout, source.Stop.Token);
            Post(source, () => { if (!Ready) Fail(source, "backend-start-timeout"); });
        } catch (OperationCanceledException) { }
    }
    private async Task DiagnosticsAsync(Session source)
    {
        var buffer = new char[1024];
        try {
            int count;
            while ((count = await source.Process!.StandardError.ReadAsync(buffer.AsMemory(), source.Stop.Token)) > 0) {
                var text = source.Diagnostic + new string(buffer, 0, count);
                source.Diagnostic = text.Length <= 4096 ? text : text[^4096..];
                if (Interlocked.Exchange(ref source.DiagnosticPending, 1) == 0)
                    Post(source, () => { Interlocked.Exchange(ref source.DiagnosticPending, 0); Diagnostic = source.Diagnostic; });
            }
        } catch (Exception error) when (error is IOException or OperationCanceledException or ObjectDisposedException) { }
    }
    private async Task ReadAsync(Session source)
    {
        try {
            // A bounded line reader avoids unbounded memory from a faulty child.
            var pending = new System.Text.StringBuilder();
            var buffer = new char[4096];
            int count;
            while ((count = await source.Process!.StandardOutput.ReadAsync(buffer.AsMemory(), source.Stop.Token)) > 0) {
                for (int i = 0; i < count; i++) {
                    if (buffer[i] != '\n') {
                        pending.Append(buffer[i]);
                        if (pending.Length > 1048576) throw new JsonException("Oversized backend response");
                        continue;
                    }
                    var data = JsonNode.Parse(pending.ToString()) as JsonObject ?? throw new JsonException("Expected a backend object");
                    pending.Clear();
                    Post(source, () => Receive(source, data));
                }
            }
        } catch (Exception error) when (error is IOException or JsonException or OperationCanceledException or ObjectDisposedException) {
            Post(source, () => Fail(source, "backend-protocol-error", error.Message));
        }
    }
    private void Receive(Session source, JsonObject data)
    {
        if (data["type"]?.GetValue<string>() == "state") {
            if (data["version"]?.GetValue<int>() != 1) { Fail(source, "unsupported-version"); return; }
            if (new[] { "app", "connection", "playback", "noiseControl", "equalizer", "settings", "deviceManager" }.Any(key => data[key] is not JsonObject)) {
                Fail(source, "backend-protocol-error"); return;
            }
            if (!Ready) Error = "";
            State = data; Ready = true; StateRevision++;
            // v1 sends a result followed by its full state before reading the
            // next request. Finish edits only after that state has arrived.
            foreach (var pair in source.Results.ToArray()) {
                if (source.Requests.Remove(pair.Key, out var request)) request.TrySetResult(pair.Value);
            }
            source.Results.Clear();
        } else if (data["type"]?.GetValue<string>() == "result") {
            Error = data["ok"]?.GetValue<bool>() == true ? "" : data["errorCode"]?.GetValue<string>() ?? "Command rejected";
            string id = data["id"]?.ToString() ?? "";
            if (source.Requests.ContainsKey(id)) source.Results[id] = data;
        }
        Changed?.Invoke();
    }
    private void Fail(Session source, string code, string detail = "")
    {
        State = new(); Ready = false; Error = code;
        Diagnostic = detail.Length > 0 ? detail : source.Diagnostic;
        if (Diagnostic.Length > 4096) Diagnostic = Diagnostic[^4096..];
        CancelRequests(source);
        _ = RetireAsync(source, true);
        Changed?.Invoke();
    }
    public void Send(string target, string method, params object[] args)
        => Queue(target, method, args, null);

    public async Task<JsonObject> RequestAsync(string target, string method, params object[] args)
    {
        var source = session;
        var completion = new TaskCompletionSource<JsonObject>(TaskCreationOptions.RunContinuationsAsynchronously);
        if (!Queue(target, method, args, completion)) throw new OperationCanceledException("Backend is not ready");
        try { return await completion.Task.WaitAsync(CommandTimeout); }
        catch (TimeoutException) {
            if (session == source && source is not null) Fail(source, "backend-command-timeout", "Backend did not finish the command");
            throw;
        }
    }
    private bool Queue(string target, string method, object[] args, TaskCompletionSource<JsonObject>? completion)
    {
        if (disposed || !Ready || session is null) return false;
        string id = (SentCommandCount + 1).ToString();
        var frame = JsonSerializer.Serialize(new { version = 1, id, target, method, args });
        if (completion is not null) session.Requests.Add(id, completion);
        if (session.Outgoing.Writer.TryWrite(frame)) { SentCommandCount++; return true; }
        Fail(session, "backend-write-failed");
        return false;
    }
    private static void CancelRequests(Session source)
    {
        foreach (var request in source.Requests.Values) request.TrySetCanceled();
        source.Requests.Clear(); source.Results.Clear();
    }
    private async Task WriteAsync(Session source)
    {
        try {
            await foreach (var frame in source.Outgoing.Reader.ReadAllAsync(source.Stop.Token)) {
                source.Stop.Token.ThrowIfCancellationRequested();
                await source.Process!.StandardInput.WriteLineAsync(frame.AsMemory(), source.Stop.Token);
                await source.Process.StandardInput.FlushAsync(source.Stop.Token);
            }
        } catch (Exception error) when (error is IOException or InvalidOperationException or OperationCanceledException) {
            Post(source, () => Fail(source, "backend-write-failed", error.Message));
        }
    }
    private static Task RetireAsync(Session source, bool abort)
    {
        lock (source) {
            if (source.Cleanup is not null) return source.Cleanup;
            source.Stop.Cancel(); source.Outgoing.Writer.TryComplete();
            return source.Cleanup = Task.Run(async () => {
                var process = source.Process;
                if (process is null) return;
                try {
                    if (abort && !process.HasExited) process.Kill(entireProcessTree: true);
                    else process.StandardInput.Close();
                    using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(2));
                    try { await process.WaitForExitAsync(timeout.Token); }
                    catch (OperationCanceledException) {
                        if (!process.HasExited) process.Kill(entireProcessTree: true);
                        await process.WaitForExitAsync();
                    }
                } catch (Exception error) when (error is IOException or InvalidOperationException or System.ComponentModel.Win32Exception) { }
                finally { process.Dispose(); }
            });
        }
    }
    public Task CloseAsync()
    {
        disposed = true; Ready = false; State = new();
        if (session is not null) CancelRequests(session);
        return session is null ? Task.CompletedTask : RetireAsync(session, false);
    }
    public void Dispose() => CloseAsync().GetAwaiter().GetResult();
}
