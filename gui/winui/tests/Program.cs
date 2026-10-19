using System.Collections.Concurrent;
using System.Diagnostics;
using System.Reflection;
using System.Text.Json.Nodes;
using SoundConnect.WinUI;

static JsonObject Snapshot() {
    var state = new JsonObject { ["type"] = "state", ["version"] = 1 };
    foreach (var key in new[] { "app", "connection", "playback", "noiseControl", "equalizer", "settings", "deviceManager", "preferences" }) state[key] = new JsonObject();
    state["app"]!["pid"] = Environment.ProcessId;
    return state;
}
if (args.Length > 0 && args[0] == "--fixture") {
    var mode = args[1];
    if (mode == "timeout") { await Task.Delay(30000); return; }
    if (mode == "busy") { Console.Error.WriteLine("Another live backend owns the device session"); Environment.ExitCode = 3; return; }
    if (mode == "invalid") { Console.WriteLine("[]"); await Task.Delay(30000); return; }
    if (mode == "noisy") { Console.Error.Write(new string('x', 200000) + "TAIL"); Console.Error.Flush(); }
    var state = Snapshot();
    if (mode == "version") state["version"] = 99;
    void Publish() { Console.WriteLine(state.ToJsonString()); Console.Out.Flush(); }
    Publish();
    if (mode == "crash") { await Task.Delay(300); Environment.ExitCode = 7; return; }
    if (mode == "hang") { await Task.Delay(30000); return; }
    int writes = 0;
    while (Console.ReadLine() is string line) {
        var request = JsonNode.Parse(line)!;
        state["app"]!["writes"] = ++writes;
        state["app"]!["lastId"] = request["id"]!.ToString();
        Publish();
    }
    return;
}
var events = new ConcurrentQueue<Action>();
void Check(bool value, string message) { if (!value) throw new Exception(message); }
async Task Pump(Func<bool> predicate) {
    var until = DateTime.UtcNow.AddSeconds(8);
    while (DateTime.UtcNow < until) {
        while (events.TryDequeue(out var action)) action();
        if (predicate()) return;
        await Task.Delay(10);
    }
    throw new Exception("Timed out waiting for transport state");
}
BackendClient Make(string mode, double timeout = 10) {
    var client = new BackendClient(events.Enqueue) { StartupTimeout = TimeSpan.FromSeconds(timeout) };
    var path = Environment.ProcessPath!;
    var arguments = Path.GetFileNameWithoutExtension(path) == "dotnet"
        ? new[] { Assembly.GetExecutingAssembly().Location, "--fixture", mode } : new[] { "--fixture", mode };
    client.Start(path, arguments);
    return client;
}
foreach (var pair in new[] { ("timeout", "backend-start-timeout"), ("version", "unsupported-version"), ("invalid", "backend-protocol-error"), ("busy", "backend-stopped") }) {
    using var client = Make(pair.Item1, pair.Item1 == "timeout" ? .2 : 10);
    await Pump(() => client.Error == pair.Item2);
    Check(!client.Ready && client.State.Count == 0, "Failure retained usable state");
    client.Send("app", "test"); Check(client.SentCommandCount == 0, "Sent while offline");
    if (pair.Item1 == "busy") Check(client.Diagnostic.Contains("owns the device session"), "Lost diagnostics");
}
using (var client = new BackendClient(events.Enqueue)) {
    client.Start(Path.Combine(Path.GetTempPath(), "soundconnect-missing", "backend.exe"), Array.Empty<string>());
    await Pump(() => client.Error == "backend-start-failed");
}
using (var client = Make("noisy")) {
    await Pump(() => client.Ready && client.Diagnostic.EndsWith("TAIL"));
    Check(client.Diagnostic.Length <= 4096, "Unbounded diagnostics");
    for (int i = 0; i < 20; i++) client.Send("app", "test", i);
    await Pump(() => client.Text("app", "lastId") == "20");
    Check(client.Number("app", "writes") == 20, "Lost or reordered writes");
    client.Restart();
    await Pump(() => client.Ready);
    Check(client.Number("app", "writes") == 0 && client.SentCommandCount == 20, "Replayed commands after restart");
}
using (var client = Make("crash")) {
    await Pump(() => client.Ready);
    await Pump(() => client.Error == "backend-stopped");
    Check(client.State.Count == 0, "Crash retained state");
}
using (var client = Make("hang")) {
    await Pump(() => client.Ready);
    var watch = Stopwatch.StartNew();
    await client.CloseAsync();
    Check(watch.Elapsed < TimeSpan.FromSeconds(5), "Shutdown did not terminate child");
}
Console.WriteLine("PASS: native C# transport startup, timeout, malformed/version failures, diagnostics, ordered writes, restart, crash and shutdown");
