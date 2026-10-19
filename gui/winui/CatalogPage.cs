using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace SoundConnect.WinUI;

// This adapter owns WinUI controls only; the shared catalog names backend
// operations and capabilities. No device command mapping is duplicated here.
internal sealed partial class MainWindow
{
    private static readonly JsonObject Catalog = JsonNode.Parse(
        File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "catalog.json")))!.AsObject();
    private string CatalogText(JsonNode? node) => node is JsonObject labels
        ? labels[app.Chinese ? "zh_CN" : "en"]?.GetValue<string>() ?? labels["en"]?.GetValue<string>() ?? ""
        : node?.ToString() ?? "";
    internal string Dynamic(string text)
    {
        var messages = Catalog["messages"]!.AsObject();
        if (messages[text] is JsonNode labels) return CatalogText(labels);
        foreach (var pair in messages) {
            int index = pair.Key.IndexOf("%1", StringComparison.Ordinal);
            if (index < 0) continue;
            string prefix = pair.Key[..index], suffix = pair.Key[(index + 2)..];
            if (text.StartsWith(prefix, StringComparison.Ordinal) && text.EndsWith(suffix, StringComparison.Ordinal))
                return CatalogText(pair.Value).Replace("%1", text.Substring(prefix.Length, text.Length - prefix.Length - suffix.Length));
        }
        return text;
    }
    private string ConnectionError()
    {
        string code = backend.Text("connection", "errorCode"), detail = backend.Text("connection", "errorDetail");
        return code.Length > 0 ? Dynamic(code) + (detail.Length > 0 ? ": " + detail : "") : detail;
    }
    private string Display(JsonNode? value, string key)
    {
        if (key == "errorDetail") return ConnectionError();
        if (value is null) return "—";
        if (value is JsonValue scalar && scalar.TryGetValue<bool>(out var flag)) return Dynamic(flag ? "Yes" : "No");
        if (value is JsonArray items) return string.Join(" · ", items.Select(item => item?["name"]?.ToString() ?? ""));
        return key is "state" or "errorDetail" or "alertMessage" ? Dynamic(value.ToString()) : value.ToString();
    }
    private static object Argument(JsonNode? node)
    {
        if (node is JsonValue value) {
            if (value.TryGetValue<bool>(out var boolean)) return boolean;
            if (value.TryGetValue<int>(out var integer)) return integer;
        }
        return node?.ToString() ?? "";
    }

    internal async System.Threading.Tasks.Task SmokeTestAsync()
    {
        if (app.Live || !backend.Flag("app", "previewMode")) throw new InvalidOperationException("Smoke tests require a ready preview backend");
        int before = backend.SentCommandCount;
        foreach (var spec in Catalog["pages"]!.AsArray()) {
            route = spec!["id"]!.GetValue<string>(); BuildPage(); Refresh();
        }
        if (backend.SentCommandCount != before) throw new InvalidOperationException("Page synchronization emitted a command");
        route = "sound"; BuildPage();
        var toggle = page.Children.OfType<ToggleSwitch>().First(t => t.IsEnabled);
        toggle.IsOn = !toggle.IsOn; Refresh();
        if (backend.SentCommandCount != before + 1) throw new InvalidOperationException("Toggle emitted duplicate commands");
        route = "home"; BuildPage();
        var slider = page.Children.OfType<Slider>().First(s => s.IsEnabled);
        slider.Value = slider.Value == 12 ? 13 : 12;
        smokeSliderCommit!(); smokeSliderCommit!(); Refresh();
        if (backend.SentCommandCount != before + 2) throw new InvalidOperationException("Slider emitted duplicate commands");
        bool chinese = app.Chinese;
        route = "application"; BuildPage();
        var choices = page.Children.OfType<ComboBox>().ToArray();
        choices[0].SelectedIndex = chinese ? 0 : 1;
        await System.Threading.Tasks.Task.Delay(250);
        choices = page.Children.OfType<ComboBox>().ToArray();
        choices[1].SelectedIndex = 2;
        await System.Threading.Tasks.Task.Delay(250);
        if (app.Chinese == chinese || app.Theme != ElementTheme.Dark || backend.Flag("preferences", "persistent"))
            throw new InvalidOperationException("Preference state did not reach native controls");
        if (backend.SentCommandCount != before + 4) throw new InvalidOperationException("Preference synchronization emitted duplicate writes");
        var playKey = root.KeyboardAccelerators.First(k => k.Key == Windows.System.VirtualKey.P);
        app.InvokeShortcut(1);
        if (backend.SentCommandCount != before + 5) throw new InvalidOperationException("Shortcut did not send exactly once");
        // Real Win32 registration/message dispatch without synthesized global input.
        using (var keys = new Hotkeys(app.InvokeShortcut)) {
            if (keys.Registered(2)) {
                keys.TestDispatch(2);
                if (backend.SentCommandCount != before + 6) throw new InvalidOperationException("Global shortcut dispatch failed");
            }
        }
        if (Dynamic("Device function 99") != (app.Chinese ? "设备功能 99" : "Device function 99"))
            throw new InvalidOperationException("Dynamic translation failed");
        var popup = new Microsoft.UI.Xaml.Controls.Primitives.Popup { Child = new TextBlock { Text = "Popup lifecycle" }, XamlRoot = root.XamlRoot };
        popup.IsOpen = true;
        if (!GetOpenPopups().Any()) throw new InvalidOperationException("Attached popup not detected");
        popup.IsOpen = false;
        app.TogglePanel(); app.TogglePanel();
        int beforeCancel = backend.SentCommandCount;
        var cancel = ConfirmCommand("playback", "powerOff", Array.Empty<object>());
        await System.Threading.Tasks.Task.Delay(100);
        confirmation?.Hide(); await cancel;
        if (beforeCancel != backend.SentCommandCount) throw new InvalidOperationException("Cancel emitted a device command");
        if (root.RequestedTheme != ElementTheme.Dark) throw new InvalidOperationException("Native theme not applied");
        if (Dynamic("rock") != (app.Chinese ? "摇滚" : "Rock")) throw new InvalidOperationException("Preset translation failed");
        Refresh();
        int beforeRestart = backend.SentCommandCount;
        backend.Restart();
        if (backend.Ready || backend.State.Count != 0) throw new InvalidOperationException("Restart retained old state");
        for (int i = 0; i < 100 && !backend.Ready; i++) await System.Threading.Tasks.Task.Delay(100);
        if (!backend.Ready || backend.SentCommandCount != beforeRestart)
            throw new InvalidOperationException("Recovery failed or replayed commands: " + backend.Error);
    }

    private ContentDialog? confirmation;
    private async System.Threading.Tasks.Task ConfirmCommand(string group, string method, object[] args)
    {
        if (confirmation is not null) return;
        bool power = method == "powerOff";
        var dialog = new ContentDialog { XamlRoot = root.XamlRoot, RequestedTheme = app.Theme,
            Title = power ? T("是否关闭耳机电源？", "Power off headphones?") : T("是否取消设备配对？", "Unpair device?"),
            PrimaryButtonText = power ? T("关机", "Power off") : T("取消配对", "Unpair"),
            CloseButtonText = T("取消", "Cancel"), DefaultButton = ContentDialogButton.Close };
        confirmation = dialog;
        try { if (await dialog.ShowAsync() == ContentDialogResult.Primary) backend.Send(group, method, args); }
        finally { confirmation = null; }
    }

    private void CatalogPage(string id)
    {
        var spec = Catalog["pages"]!.AsArray().First(p => p!["id"]!.GetValue<string>() == id)!;
        Heading(CatalogText(spec["label"]), true);
        foreach (var entry in spec["fields"]!.AsArray()) {
            var field = entry!.AsObject();
            string group = field["group"]!.GetValue<string>(), key = field["key"]!.GetValue<string>();
            string method = field["method"]!.GetValue<string>(), kind = field["kind"]!.GetValue<string>();
            string cap = field["capability"]!.GetValue<string>(), label = CatalogText(field["label"]);
            JsonNode? Current() {
                var value = backend.State[group]?[key];
                if (field["index"] is JsonNode index) {
                    int i = index.GetValue<int>();
                    return value is JsonArray array && i < array.Count ? array[i] : null;
                }
                return value;
            }
            bool Enabled() => kind == "label" || (backend.Ready && (group == "app" || group == "preferences")) ||
                (backend.Connected && (cap.Length == 0 || backend.Flag(group, cap)) &&
                 (field["index"] is null || Current() is not null));
            void Send(object? value = null) {
                if (updating) return;
                var args = new List<object>();
                if (field["index"] is JsonNode index) args.Add(index.GetValue<int>());
                if (kind != "button") args.Add(value!);
                if (method is "powerOff" or "unpairDevice") _ = ConfirmCommand(group, method, args.ToArray());
                else backend.Send(group, method, args.ToArray());
            }
            if (kind == "toggle") {
                Toggle(label, group, key, cap, method);
            } else if (kind == "slider") {
                Slider(label, group, cap, field["min"]!.GetValue<int>(), field["max"]!.GetValue<int>(),
                    () => Current()?.GetValue<int>() ?? 0, v => Send(v));
                var slider = (Slider)page.Children.Last();
                bindings.Add(() => slider.IsEnabled = Enabled());
            } else if (kind == "button") {
                var button = ActionButton(label, () => Send());
                bindings.Add(() => button.IsEnabled = Enabled());
                page.Children.Add(button);
            } else if (kind == "choice" || kind == "devices") {
                var combo = new ComboBox { Header = label, MinWidth = 200,
                    HorizontalAlignment = HorizontalAlignment.Stretch };
                var values = new List<JsonNode?>();
                string signature = "";
                bindings.Add(() => {
                    var names = new List<string>();
                    var next = new List<JsonNode?>();
                    var source = field["source"]?.GetValue<string>();
                    if (kind == "devices") {
                        foreach (var item in Current()?.AsArray() ?? new JsonArray()) {
                            names.Add(item?["name"]?.ToString() ?? ""); next.Add(item);
                        }
                    } else if (source is not null) {
                        foreach (var item in backend.State[group]?[source]?.AsArray() ?? new JsonArray()) {
                            names.Add(source == "devices" ? item?["name"]?.ToString() ?? "" : Dynamic(item?["name"]?.ToString() ?? ""));
                            next.Add(item?[field["valueKey"]!.GetValue<string>()]);
                        }
                    } else {
                        foreach (var item in field["options"]?.AsArray() ?? new JsonArray()) {
                            string optionCap = item?["capability"]?.ToString() ?? "";
                            if (optionCap.Length > 0 && !backend.Flag(group, optionCap)) continue;
                            names.Add(CatalogText(item?["label"])); next.Add(item?["value"]);
                        }
                    }
                    var nextSignature = string.Join("\n", names.Zip(next, (n, v) => n + ":" + v?.ToJsonString()));
                    if (signature != nextSignature) { signature = nextSignature; combo.ItemsSource = names; values = next; }
                    combo.SelectedIndex = kind == "devices" || key.Length == 0 ? -1
                        : values.FindIndex(v => JsonNode.DeepEquals(v, Current()));
                    combo.IsEnabled = kind == "devices" ? backend.Ready && values.Count > 0 && !backend.Connected && backend.Text("connection", "state") != "connecting"
                        : Enabled() && values.Count > 0;
                });
                combo.SelectionChanged += (_, _) => {
                    if (updating || combo.SelectedIndex < 0 || combo.SelectedIndex >= values.Count) return;
                    var selected = values[combo.SelectedIndex];
                    if (kind == "devices") backend.Send("app", "connectDevice", selected!["name"]!.ToString(), selected["mac"]!.ToString());
                    else Send(Argument(selected));
                };
                AutomationProperties.SetName(combo, label);
                page.Children.Add(combo);
            } else {
                var text = new TextBlock { TextWrapping = TextWrapping.Wrap };
                bindings.Add(() => text.Text = label + ": " + Display(Current(), key));
                page.Children.Add(text);
            }
        }
    }
}
