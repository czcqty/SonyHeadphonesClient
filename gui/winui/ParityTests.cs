using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation.Peers;
using Microsoft.UI.Xaml.Automation.Provider;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace SoundConnect.WinUI;

internal sealed partial class MainWindow
{
    internal JsonArray ParityResults { get; } = new();
    private readonly HashSet<string> exercisedCommands = new();
    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
    private static async Task UntilAsync(Func<bool> predicate, string message)
    {
        for (int i = 0; i < 500; i++) {
            if (predicate()) return;
            await Task.Delay(20);
        }
        throw new InvalidOperationException(message);
    }
    private static void Invoke(Button button)
    {
        Require(button.IsEnabled, "Invoked disabled button: " + button.Content);
        var peer = new ButtonAutomationPeer(button);
        ((IInvokeProvider)peer.GetPattern(PatternInterface.Invoke)).Invoke();
    }
    private static Button? FindButton(DependencyObject root, string name)
    {
        if (root is Button button && button.Name == name) return button;
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(root); i++) {
            var result = FindButton(VisualTreeHelper.GetChild(root, i), name);
            if (result is not null) return result;
        }
        return null;
    }
    private async Task SetFieldAsync(FieldBinding binding, object target)
    {
        Require(binding.Control.IsEnabled && binding.Row.Visibility == Visibility.Visible,
            "Supported control cannot be adjusted: " + binding.Field.ToJsonString());
        int before = backend.SentCommandCount;
        var field = binding.Field;
        string method = field["method"]!.ToString();
        if (binding.Control is ToggleSwitch toggle) toggle.IsOn = (bool)target;
        else if (binding.Control is Slider slider) {
            slider.Value = Convert.ToInt32(target);
            binding.Commit!(); binding.Commit!();
        } else if (binding.Control is ComboBox combo) {
            var value = JsonSerializer.SerializeToNode(target);
            int index = binding.Values.FindIndex(v => JsonNode.DeepEquals(v, value));
            Require(index >= 0, "Missing choice: " + value);
            combo.SelectedIndex = index;
            // Reproduces a poll arriving before the command's response. This
            // used to reset the choice to the previous value on the screen.
            Refresh();
            Require(combo.SelectedIndex == index, "Snapshot rolled back an unacknowledged selection: " + method);
        } else throw new InvalidOperationException("Not an editable field");
        await binding.LastWrite;
        Require(backend.SentCommandCount == before + 1, "Interaction did not emit exactly one command: " + method);
        Require(backend.Error.Length == 0, "Backend rejected native control: " + method + ": " + backend.Error);
        Require(JsonNode.DeepEquals(FieldValue(field), JsonSerializer.SerializeToNode(target)),
            "Native edit did not roundtrip: " + method + " wanted " + target + " received " + FieldValue(field));
        exercisedCommands.Add(field["group"] + "." + method);
    }
    private async Task PrerequisiteAsync(string key, object value)
    {
        var binding = fields.FirstOrDefault(b => b.Field["key"]!.ToString() == key);
        if (binding is not null && binding.Control.IsEnabled && !JsonNode.DeepEquals(FieldValue(binding.Field), JsonSerializer.SerializeToNode(value)))
            await SetFieldAsync(binding, value);
    }
    private async Task ConfirmForTestAsync(Action open, bool accept)
    {
        open();
        await UntilAsync(() => confirmation?.IsLoaded == true, "Confirmation was not shown");
        if (accept) {
            var primary = FindButton(confirmation!, "PrimaryButton");
            Require(primary is not null, "Native confirmation primary button missing");
            Invoke(primary!);
        } else confirmation!.Hide();
        await UntilAsync(() => confirmation is null, "Confirmation did not close");
    }

    internal async Task ParityTestAsync()
    {
        Require(!app.Live && backend.Flag("app", "previewMode"), "Parity checks cannot use real devices");
        var description = await backend.RequestAsync("app", "describe");
        var profiles = description["contract"]!["profiles"]!.AsArray().Select(p => p!.ToString()).ToArray();
        Require(profiles.Length == 12, "Expected all twelve device configurations");
        var contract = description["contract"]!["commands"]!.AsArray()
            .Select(c => c!["target"] + "." + c["method"]).ToHashSet();
        foreach (string profile in profiles) {
            backend.Start(backend.ExecutablePath, false, profile, app.Chinese ? "zh_CN" : "en", "system");
            await UntilAsync(() => backend.Ready, "Profile backend did not start: " + profile);
            int edits = 0, unavailable = 0;
            foreach (var spec in Catalog["pages"]!.AsArray()) {
                string id = spec!["id"]!.ToString();
                if (id is "application" or "devices" or "connection") continue;
                int before = backend.SentCommandCount;
                Navigate(id); Refresh(); page.UpdateLayout(); await Task.Delay(20);
                Require(before == backend.SentCommandCount, "Page initialization wrote settings: " + id);
                if (id == "noise") { await PrerequisiteAsync("mode", 2); await PrerequisiteAsync("adaptiveEnabled", true); }
                if (id == "sound") await PrerequisiteAsync("listeningModeEnabled", true);
                if (id == "behavior") await PrerequisiteAsync("speakToChatEnabled", true);
                foreach (var b in fields.ToArray()) {
                    string kind = b.Field["kind"]!.ToString(), method = b.Field["method"]!.ToString();
                    string cap = b.Field["capability"]!.ToString(), group = b.Field["group"]!.ToString();
                    if (cap.Length > 0 && !backend.Flag(group, cap) || b.Field["index"] is not null && FieldValue(b.Field) is null) {
                        Require(!b.Control.IsEnabled && b.Reason.Text.Length > 0, "Unsupported control lacks an explanation: " + method);
                        unavailable++; continue;
                    }
                    if (kind == "button") {
                        if (method == "powerOff") continue;
                        int beforeButton = backend.SentCommandCount;
                        Invoke((Button)b.Control); await b.LastWrite;
                        Require(backend.SentCommandCount == beforeButton + 1 && backend.Error.Length == 0, "Native playback action failed: " + method);
                        exercisedCommands.Add(group + "." + method); continue;
                    }
                    if (b.Field["key"]!.ToString() == "listeningRoomSize") await PrerequisiteAsync("listeningModeEnabled", true);
                    Require(b.Row.Visibility == Visibility.Visible && b.Control.IsEnabled, "Incorrectly hidden/grey control: " + profile + ": " + method);
                    var original = FieldValue(b.Field)!.DeepClone();
                    if (kind == "toggle") {
                        await SetFieldAsync(b, !original.GetValue<bool>());
                        await SetFieldAsync(b, original.GetValue<bool>()); edits += 2;
                    } else if (kind == "slider") {
                        int target = original.GetValue<int>() == b.Field["min"]!.GetValue<int>() ? b.Field["max"]!.GetValue<int>() : b.Field["min"]!.GetValue<int>();
                        await SetFieldAsync(b, target); edits++;
                        int count = backend.SentCommandCount;
                        b.Number!.Value = original.GetValue<int>(); await b.LastWrite;
                        Require(backend.SentCommandCount == count + 1 && JsonNode.DeepEquals(original, FieldValue(b.Field)), "Numeric input failed: " + method);
                        edits++;
                    } else if (b.Control is ComboBox combo) {
                        var available = b.Values.Select(v => v!.DeepClone()).ToArray();
                        foreach (var choice in available) {
                            if (JsonNode.DeepEquals(choice, FieldValue(b.Field))) continue;
                            await SetFieldAsync(b, Argument(choice)); edits++;
                        }
                        if (!JsonNode.DeepEquals(original, FieldValue(b.Field))) { await SetFieldAsync(b, Argument(original)); edits++; }
                        combo.StartBringIntoView(); await Task.Delay(40); page.UpdateLayout();
                        combo.Focus(FocusState.Programmatic); combo.IsDropDownOpen = true;
                        Require(combo.IsDropDownOpen, "Native picker did not open: " + method);
                        var items = combo.Items.Cast<object>().ToArray(); int selected = combo.SelectedIndex;
                        Refresh(); Refresh();
                        Require(combo.IsDropDownOpen && items.SequenceEqual(combo.Items.Cast<object>()) && selected == combo.SelectedIndex,
                            "Polling disrupted an open native picker: " + method + ", open=" + combo.IsDropDownOpen + ", selected=" + combo.SelectedIndex + ", expected=" + selected);
                        combo.IsDropDownOpen = false;
                    }
                }
            }
            Navigate("devices"); Refresh(); page.UpdateLayout(); await Task.Delay(20);
            var pairing = fields.Single(b => b.Field["method"]!.ToString() == "setPairingMode");
            if (pairing.Control.IsEnabled) {
                bool original = FieldValue(pairing.Field)!.GetValue<bool>();
                await SetFieldAsync(pairing, !original); await SetFieldAsync(pairing, original); edits += 2;
                int beforeDisconnect = backend.SentCommandCount;
                Invoke(deviceButtons.First(p => p.Key.EndsWith(".connection")).Value); await lastDeviceAction;
                Require(backend.SentCommandCount == beforeDisconnect + 1 && backend.Error.Length == 0, "Device row disconnect failed");
                exercisedCommands.Add("deviceManager.disconnectDevice");
                // A deterministic reported multipoint state exercises switching
                // source. This does not claim a physical headphone acknowledgement.
                var devices = backend.State["deviceManager"]!["devices"]!.AsArray();
                if (devices.Count > 1) {
                    string mac = devices[1]!["mac"]!.ToString();
                    devices[1]!["connected"] = false; Refresh(); page.UpdateLayout();
                    int beforeConnect = backend.SentCommandCount;
                    Invoke(deviceButtons["paired." + mac + ".connection"]); await lastDeviceAction;
                    Require(backend.SentCommandCount == beforeConnect + 1 && backend.Error.Length == 0, "Device row connect failed");
                    exercisedCommands.Add("deviceManager.connectDevice");
                    devices = backend.State["deviceManager"]!["devices"]!.AsArray();
                }
                if (devices.Count > 1 && backend.Flag("deviceManager", "sourceSwitchSupported")) {
                    devices[1]!["connected"] = true; devices[1]!["playback"] = false; Refresh(); page.UpdateLayout();
                    Invoke(deviceButtons.First(p => p.Key.EndsWith(".playback")).Value); await lastDeviceAction;
                    Require(backend.Error.Length == 0, "Source switching was rejected"); exercisedCommands.Add("deviceManager.switchDevice");
                }
                var unpair = deviceButtons.First(p => p.Key.EndsWith(".unpair")).Value;
                int cancelCount = backend.SentCommandCount;
                await ConfirmForTestAsync(() => Invoke(unpair), false); await lastDeviceAction;
                Require(cancelCount == backend.SentCommandCount, "Cancel unpair wrote a command");
                await ConfirmForTestAsync(() => Invoke(unpair), true); await lastDeviceAction;
                Require(backend.SentCommandCount == cancelCount + 1 && backend.Error.Length == 0, "Confirmed unpair failed");
                exercisedCommands.Add("deviceManager.unpairDevice");
            }
            Navigate("home"); Refresh(); page.UpdateLayout(); await Task.Delay(20);
            var power = fields.Single(b => b.Field["method"]!.ToString() == "powerOff");
            int cancelPower = backend.SentCommandCount;
            await ConfirmForTestAsync(() => Invoke((Button)power.Control), false); await power.LastWrite;
            Require(backend.SentCommandCount == cancelPower, "Cancel power off wrote a command");
            await ConfirmForTestAsync(() => Invoke((Button)power.Control), true); await power.LastWrite;
            Require(backend.SentCommandCount == cancelPower + 1 && backend.Error.Length == 0 && !power.Control.IsEnabled, "Power off pending state is not reflected");
            exercisedCommands.Add("playback.powerOff");
            ParityResults.Add(new JsonObject { ["profile"] = profile, ["edits"] = edits, ["unsupportedExplained"] = unavailable, ["ok"] = true });
        }
        // Discovery and connection use their own row controls, separate from
        // headphone multipoint management and its reported capabilities.
        Navigate("connection"); Refresh(); page.UpdateLayout(); await Task.Delay(20);
        var disconnect = fields.Single(b => b.Field["method"]!.ToString() == "disconnectDevice");
        Invoke((Button)disconnect.Control); await disconnect.LastWrite;
        Require(!backend.Connected, "Disconnect did not disable device commands"); exercisedCommands.Add("app.disconnectDevice");
        var scan = fields.Single(b => b.Field["method"]!.ToString() == "refreshDevices");
        Invoke((Button)scan.Control); await scan.LastWrite;
        Require(backend.Error.Length == 0, "Discovery failed"); exercisedCommands.Add("app.refreshDevices");
        page.UpdateLayout(); Invoke(deviceButtons.First(p => p.Key.StartsWith("discover.")).Value); await lastDeviceAction;
        await UntilAsync(() => backend.Connected, "Discovered device did not connect"); exercisedCommands.Add("app.connectDevice");
        int beforeQuick = backend.SentCommandCount;
        var quick = app.QuickWindow;
        try {
            quick.Activate(); quick.page.UpdateLayout(); await Task.Delay(30);
            Require(backend.SentCommandCount == beforeQuick, "Quick panel initialization wrote device settings");
            var mode = quick.fields.Single(b => b.Field["method"]!.ToString() == "setMode");
            if (FieldValue(mode.Field)?.GetValue<int>() != 2) await quick.SetFieldAsync(mode, 2);
            var ambient = quick.fields.Single(b => b.Field["method"]!.ToString() == "setAmbientLevel");
            await quick.SetFieldAsync(ambient, FieldValue(ambient.Field)!.GetValue<int>() == 1 ? 20 : 1);
            var focus = quick.fields.Single(b => b.Field["method"]!.ToString() == "setFocusOnVoice");
            await quick.SetFieldAsync(focus, !FieldValue(focus.Field)!.GetValue<bool>());
            var volume = quick.fields.Single(b => b.Field["method"]!.ToString() == "setVolume");
            await quick.SetFieldAsync(volume, FieldValue(volume.Field)!.GetValue<int>() == 12 ? 13 : 12);
            foreach (var b in quick.fields.Where(b => b.Field["kind"]!.ToString() == "button")) {
                int count = backend.SentCommandCount; Invoke((Button)b.Control); await b.LastWrite;
                Require(backend.SentCommandCount == count + 1 && backend.Error.Length == 0, "Quick panel playback failed");
            }
        } finally { quick.AppWindow.Hide(); Activate(); }
        // Exercise the enabled states of the actual discovery widgets while a
        // headset is initializing, rather than treating pipe readiness as a connection.
        Navigate("connection"); Refresh();
        string connectionState = backend.Text("connection", "state");
        backend.State["connection"]!["state"] = "initializing"; Refresh();
        Require(!fields.Single(b => b.Field["method"]!.ToString() == "refreshDevices").Control.IsEnabled,
            "Scan remained available during initialization");
        Require(fields.Single(b => b.Field["method"]!.ToString() == "disconnectDevice").Control.IsEnabled,
            "Cannot cancel initialization");
        Require(deviceButtons.Where(p => p.Key.StartsWith("discover.")).All(p => !p.Value.IsEnabled), "Discovery allowed overlapping connections");
        backend.State["connection"]!["state"] = connectionState; Refresh();
        Navigate("home");
        string executable = backend.ExecutablePath;
        backend.Start(executable + ".missing", false, profiles[0], app.Chinese ? "zh_CN" : "en", "system");
        Require(fields.All(b => !b.Control.IsEnabled), "Failure left old device controls enabled");
        await UntilAsync(() => backend.Error == "backend-start-failed", "Missing child error is not visible");
        Require(backend.State.Count == 0 && fields.All(b => !b.Control.IsEnabled), "Missing child retained stale device state");
        backend.Start(executable, false, profiles[0], app.Chinese ? "zh_CN" : "en", "system");
        await UntilAsync(() => backend.Ready, "Retry did not recover native controls");
        Require(fields.Single(b => b.Field["method"]!.ToString() == "setVolume").Control.IsEnabled, "Retry left supported controls grey");
        exercisedCommands.Add("preferences.setLocale"); exercisedCommands.Add("preferences.setThemeMode"); // SmokeTestAsync checks real preference widgets.
        Require(contract.SetEquals(exercisedCommands), "Native operation coverage missing: " + string.Join(", ", contract.Except(exercisedCommands)));
        ParityResults.Add(new JsonObject { ["commands"] = exercisedCommands.Count, ["discovery"] = true, ["confirmationCancelAndAccept"] = true,
            ["quickControls"] = true, ["initializationCancel"] = true, ["failureUiAndRetry"] = true });
    }
}
