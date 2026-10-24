using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using System.Threading.Tasks;

namespace SoundConnect.WinUI;

// This adapter owns WinUI controls only; the shared catalog names backend
// operations and capabilities. No device command mapping is duplicated here.
internal sealed partial class MainWindow
{
    private sealed class FieldBinding
    {
        internal required JsonObject Field;
        internal required Control Control;
        internal required FrameworkElement Row;
        internal required TextBlock Reason;
        internal bool Pending;
        internal Task LastWrite = Task.CompletedTask;
        internal Action? Commit;
        internal NumberBox? Number;
        internal readonly List<JsonNode?> Values = new();
    }
    private readonly List<FieldBinding> fields = new();
    private bool ConnectionBusy => backend.Text("connection", "state") is "scanning" or "connecting" or "initializing";

    private JsonNode? FieldValue(JsonObject field)
    {
        var value = backend.State[field["group"]!.ToString()]?[field["key"]!.ToString()];
        if (field["index"] is JsonNode index) {
            int i = index.GetValue<int>();
            return value is JsonArray array && i < array.Count ? array[i] : null;
        }
        return value;
    }
    private string UnavailableReason(JsonObject field)
    {
        if (!backend.Ready) return T("正在等待后端状态。", "Waiting for backend state.");
        string group = field["group"]!.ToString(), method = field["method"]!.ToString();
        if (group is "app" or "preferences") {
            if (method == "refreshDevices" && (backend.Connected || ConnectionBusy))
                return T("请先取消或断开当前连接。", "Cancel or disconnect the current connection first.");
            if (method == "disconnectDevice" && !backend.Connected && !ConnectionBusy)
                return T("当前没有设备连接。", "No connection is active.");
            return "";
        }
        if (!backend.Connected) return T("请先在“连接”页连接耳机。", "Connect headphones on the Connection page first.");
        string cap = field["capability"]!.ToString();
        if (cap.Length > 0 && !backend.Flag(group, cap))
            return T("此耳机未报告对该功能的支持。", "These headphones do not report support for this feature.");
        if (field["index"] is not null && FieldValue(field) is null)
            return T("此耳机未提供该均衡器频段。", "This equalizer band is not provided by these headphones.");
        if (method == "powerOff" && backend.Flag("playback", "powerOffPending"))
            return T("正在关闭耳机电源。", "Powering off headphones.");
        return "";
    }
    private bool FieldVisible(JsonObject field)
    {
        string key = field["key"]!.ToString();
        return key switch {
            "touchLeft" or "touchRight" => !backend.Flag("settings", "touchPanelSupported") || backend.Flag("settings", "touchSupported"),
            "adaptiveEnabled" => backend.Flag("noiseControl", "adaptiveSupported"),
            "ambientLevel" or "focusOnVoice" => backend.Number("noiseControl", "mode") == 2,
            "adaptiveSensitivity" => backend.Flag("noiseControl", "adaptiveEnabled"),
            "listeningRoomSize" => backend.Flag("settings", "listeningModeEnabled"),
            "speakToChatSensitivity" or "speakToChatModeOut" => backend.Flag("settings", "speakToChatEnabled"),
            "batteryRight" or "batteryRightCharging" => backend.Flag("playback", "hasRightBattery"),
            "batteryCase" or "batteryCaseCharging" => backend.Flag("playback", "hasCaseBattery"),
            "safeListeningPressure" => backend.Flag("playback", "safeListeningSupported"),
            _ => true
        };
    }
    private async Task EditAsync(FieldBinding binding, object? value)
    {
        if (updating || binding.Pending || UnavailableReason(binding.Field).Length > 0) return;
        var field = binding.Field;
        string group = field["group"]!.ToString(), method = field["method"]!.ToString();
        var args = new List<object>();
        if (field["index"] is JsonNode index) args.Add(index.GetValue<int>());
        if (field["kind"]!.ToString() != "button") args.Add(value!);
        binding.Pending = true;
        Refresh();
        try {
            if (method is "powerOff" or "unpairDevice") await ConfirmCommand(group, method, args.ToArray());
            else {
                actionMessage = "";
                await backend.RequestAsync(group, method, args.ToArray());
            }
        } catch (OperationCanceledException) { }
        catch (TimeoutException) { }
        finally { binding.Pending = false; Refresh(); }
    }
    private static readonly JsonObject Catalog = JsonNode.Parse(
        File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "catalog.json")))!.AsObject();
    private string CatalogText(JsonNode? node) => node is JsonObject labels
        ? labels[app.Chinese ? "zh_CN" : "en"]?.GetValue<string>() ?? labels["en"]?.GetValue<string>() ?? ""
        : node?.ToString() ?? "";
    internal string Dynamic(string text)
    {
        if (text == "backend-command-timeout") return T("后端未能及时完成操作，请重试启动。", "The backend did not finish the operation in time. Retry startup.");
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
        var toggleBinding = fields.First(b => b.Control is ToggleSwitch && b.Control.IsEnabled);
        var toggle = (ToggleSwitch)toggleBinding.Control;
        toggle.IsOn = !toggle.IsOn; Refresh();
        await toggleBinding.LastWrite;
        if (backend.SentCommandCount != before + 1) throw new InvalidOperationException("Toggle emitted duplicate commands");
        route = "home"; BuildPage();
        var sliderBinding = fields.First(b => b.Control is Slider && b.Control.IsEnabled);
        var slider = (Slider)sliderBinding.Control;
        slider.Value = slider.Value == 12 ? 13 : 12;
        sliderBinding.Commit!(); sliderBinding.Commit!(); Refresh();
        await sliderBinding.LastWrite;
        if (backend.SentCommandCount != before + 2) throw new InvalidOperationException("Slider emitted duplicate commands");
        bool chinese = app.Chinese;
        var attachedRoot = root.XamlRoot;
        route = "application"; BuildPage();
        var languageBinding = fields.Single(b => b.Field["method"]!.ToString() == "setLocale");
        ((ComboBox)languageBinding.Control).SelectedIndex = chinese ? 0 : 1;
        await languageBinding.LastWrite;
        if (attachedRoot is null || root.XamlRoot != attachedRoot)
            throw new InvalidOperationException("Locale change detached the window's XAML root");
        var themeBinding = fields.Single(b => b.Field["method"]!.ToString() == "setThemeMode");
        ((ComboBox)themeBinding.Control).SelectedIndex = 2;
        await themeBinding.LastWrite;
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
        try { if (await dialog.ShowAsync() == ContentDialogResult.Primary) await backend.RequestAsync(group, method, args); }
        finally { confirmation = null; }
    }


    private void CatalogPage(string id)
    {
        var spec = id == "quick" ? Catalog["quick"]!.DeepClone() : Catalog["pages"]!.AsArray().First(p => p!["id"]!.ToString() == id)!;
        if (id == "quick") {
            foreach (string method in new[] { "previousTrack", "nextTrack" })
                spec!["fields"]!.AsArray().Add(Catalog["pages"]!.AsArray().First(p => p!["id"]!.ToString() == "home")!["fields"]!.AsArray().First(f => f!["method"]!.ToString() == method)!.DeepClone());
            foreach (string key in new[] { "ambientLevel", "focusOnVoice" })
                spec!["fields"]!.AsArray().Add(Catalog["pages"]!.AsArray().First(p => p!["id"]!.ToString() == "noise")!["fields"]!.AsArray().First(f => f!["key"]!.ToString() == key)!.DeepClone());
        }
        Heading(CatalogText(spec["label"]), true);
        foreach (var entry in spec["fields"]!.AsArray()) {
            var field = entry!.AsObject();
            string group = field["group"]!.ToString(), key = field["key"]!.ToString();
            string method = field["method"]!.ToString(), kind = field["kind"]!.ToString();
            string label = CatalogText(field["label"]);
            if (id == "devices" && (field["source"]?.ToString() == "devices" || key == "devices")) continue;
            if (id == "connection" && kind == "devices") continue;
            if (kind == "label") {
                var text = new TextBlock { TextWrapping = TextWrapping.Wrap };
                bindings.Add(() => {
                    text.Visibility = FieldVisible(field) ? Visibility.Visible : Visibility.Collapsed;
                    text.Text = label + ": " + Display(backend.Connected || group is "app" or "connection" ? FieldValue(field) : null, key);
                });
                page.Children.Add(text); continue;
            }
            var reason = new TextBlock { TextWrapping = TextWrapping.Wrap, Opacity = .8, FontSize = 13 };
            Control control;
            FrameworkElement row;
            if (kind == "toggle") control = new ToggleSwitch { Header = label, OnContent = T("开", "On"), OffContent = T("关", "Off") };
            else if (kind == "slider") control = new Slider { Header = label, Minimum = field["min"]!.GetValue<int>(),
                Maximum = field["max"]!.GetValue<int>(), StepFrequency = 1, HorizontalAlignment = HorizontalAlignment.Stretch };
            else if (kind == "button") control = new Button { Content = label };
            else control = new ComboBox { Header = label, MinWidth = 200, HorizontalAlignment = HorizontalAlignment.Stretch };
            row = control;
            var b = new FieldBinding { Field = field, Control = control, Row = row, Reason = reason };
            fields.Add(b);
            AutomationProperties.SetName(control, label);
            AutomationProperties.SetAutomationId(control, group + "." + method + (field["index"] is null ? "" : "." + field["index"]));
            void Send(object? value = null) { if (!updating && !b.Pending) b.LastWrite = EditAsync(b, value); }
            if (control is ToggleSwitch toggle) {
                toggle.Toggled += (_, _) => Send(toggle.IsOn);
                bindings.Add(() => { if (!b.Pending) toggle.IsOn = FieldValue(field)?.GetValue<bool>() ?? false; });
            } else if (control is Slider slider) {
                var grid = new Grid { ColumnSpacing = 16 };
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(112) });
                var number = new NumberBox { Header = label, Minimum = slider.Minimum, Maximum = slider.Maximum,
                    SmallChange = 1, LargeChange = 1, SpinButtonPlacementMode = NumberBoxSpinButtonPlacementMode.Compact };
                AutomationProperties.SetName(number, label + T("（数值）", " (value)"));
                AutomationProperties.SetAutomationId(number, AutomationProperties.GetAutomationId(slider) + ".value");
                Grid.SetColumn(number, 1); grid.Children.Add(slider); grid.Children.Add(number);
                b.Row = grid; b.Number = number;
                bool dirty = false;
                slider.ValueChanged += (_, _) => { if (!updating) dirty = true; };
                b.Commit = () => { if (!dirty) return; dirty = false; Send((int)Math.Round(slider.Value)); };
                slider.AddHandler(UIElement.PointerReleasedEvent, new Microsoft.UI.Xaml.Input.PointerEventHandler((_, _) => b.Commit()), true);
                slider.AddHandler(UIElement.KeyUpEvent, new Microsoft.UI.Xaml.Input.KeyEventHandler((_, _) => b.Commit()), true);
                slider.LostFocus += (_, _) => b.Commit();
                number.ValueChanged += (_, _) => { if (!updating && !double.IsNaN(number.Value)) Send((int)Math.Round(number.Value)); };
                bindings.Add(() => {
                    if (!b.Pending && !dirty) {
                        var current = FieldValue(field)?.GetValue<int>() ?? 0;
                        slider.Value = current; number.Value = current;
                    }
                });
            } else if (control is Button button) button.Click += (_, _) => Send();
            else if (control is ComboBox combo) {
                string signature = "";
                bool selectionDirty = false;
                bindings.Add(() => {
                    // Device polling must not reset a keyboard highlight or an
                    // open picker. The next closed refresh uses the newest state.
                    if (combo.IsDropDownOpen || b.Pending) return;
                    var names = new List<string>(); var next = new List<JsonNode?>();
                    string? sourceKey = field["source"]?.ToString();
                    if (sourceKey is not null) {
                        foreach (var item in backend.State[group]?[sourceKey]?.AsArray() ?? new JsonArray()) {
                            names.Add(sourceKey == "devices" ? item?["name"]?.ToString() ?? "" : Dynamic(item?["name"]?.ToString() ?? ""));
                            next.Add(item?[field["valueKey"]!.ToString()]);
                        }
                    } else foreach (var item in field["options"]?.AsArray() ?? new JsonArray()) {
                        string cap = item?["capability"]?.ToString() ?? "";
                        if (cap.Length > 0 && !backend.Flag(group, cap)) continue;
                        names.Add(CatalogText(item?["label"])); next.Add(item?["value"]);
                    }
                    string nextSignature = string.Join("\n", names.Zip(next, (n, v) => n + ":" + v?.ToJsonString()));
                    if (signature != nextSignature) {
                        signature = nextSignature; b.Values.Clear(); b.Values.AddRange(next);
                        combo.ItemsSource = names;
                    }
                    combo.SelectedIndex = key.Length == 0 ? -1 : b.Values.FindIndex(v => JsonNode.DeepEquals(v, FieldValue(field)));
                });
                combo.SelectionChanged += (_, _) => {
                    if (updating || combo.SelectedIndex < 0 || combo.SelectedIndex >= b.Values.Count) return;
                    if (combo.IsDropDownOpen) { selectionDirty = true; return; }
                    selectionDirty = false;
                    if (!JsonNode.DeepEquals(b.Values[combo.SelectedIndex], FieldValue(field))) Send(Argument(b.Values[combo.SelectedIndex]));
                };
                combo.DropDownClosed += (_, _) => {
                    if (selectionDirty && combo.SelectedIndex >= 0 && combo.SelectedIndex < b.Values.Count) {
                        selectionDirty = false;
                        if (!JsonNode.DeepEquals(b.Values[combo.SelectedIndex], FieldValue(field))) Send(Argument(b.Values[combo.SelectedIndex]));
                    }
                    Refresh();
                };
                combo.PreviewKeyDown += (_, e) => {
                    if (e.Key != Windows.System.VirtualKey.Escape || !combo.IsDropDownOpen) return;
                    selectionDirty = false; popupEscape = true;
                    // Escape cancels the picker; it must neither submit a
                    // highlighted value nor dismiss its parent quick panel.
                    bool previous = updating; updating = true;
                    try { combo.SelectedIndex = b.Values.FindIndex(v => JsonNode.DeepEquals(v, FieldValue(field))); }
                    finally { updating = previous; }
                    DispatcherQueue.TryEnqueue(() => popupEscape = false);
                };
            }
            bindings.Add(() => {
                bool visible = FieldVisible(field);
                b.Row.Visibility = visible ? Visibility.Visible : Visibility.Collapsed;
                string unavailable = UnavailableReason(field);
                if (unavailable.Length == 0 && control is ComboBox && b.Values.Count == 0)
                    unavailable = T("正在等待耳机提供可用选项。", "Waiting for available choices from the headphones.");
                string explanation = b.Pending ? T("正在应用设置…", "Applying setting…") : unavailable;
                control.IsEnabled = !b.Pending && unavailable.Length == 0;
                if (b.Number is not null) b.Number.IsEnabled = control.IsEnabled;
                reason.Text = explanation;
                reason.Visibility = visible && explanation.Length > 0 ? Visibility.Visible : Visibility.Collapsed;
                ToolTipService.SetToolTip(control, explanation.Length > 0 ? explanation : label);
                AutomationProperties.SetHelpText(control, explanation);
            });
            page.Children.Add(b.Row); page.Children.Add(reason);
        }
        if (id == "connection") ConnectionDevices();
        if (id == "devices") ManagedDevices();
        if (id == "home") {
            foreach (string target in new[] { "noise", "equalizer", "sound", "behavior", "devices" }) {
                string destination = target;
                var title = Catalog["pages"]!.AsArray().First(p => p!["id"]!.ToString() == target)!["label"];
                page.Children.Add(ActionButton(CatalogText(title), () => Navigate(destination)));
            }
        }
    }
}
