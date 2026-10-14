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
    private static object Argument(JsonNode? node)
    {
        if (node is JsonValue value) {
            if (value.TryGetValue<bool>(out var boolean)) return boolean;
            if (value.TryGetValue<int>(out var integer)) return integer;
        }
        return node?.ToString() ?? "";
    }

    internal void SmokeTest()
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
            bool Enabled() => kind == "label" || group == "app" ||
                (backend.Connected && (cap.Length == 0 || backend.Flag(group, cap)) &&
                 (field["index"] is null || Current() is not null));
            void Send(object? value = null) {
                if (updating) return;
                var args = new List<object>();
                if (field["index"] is JsonNode index) args.Add(index.GetValue<int>());
                if (kind != "button") args.Add(value!);
                backend.Send(group, method, args.ToArray());
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
                            names.Add(item?["name"]?.ToString() ?? "");
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
                    combo.IsEnabled = kind == "devices" ? values.Count > 0 && !backend.Connected
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
                bindings.Add(() => text.Text = label + ": " + (Current()?.ToString() ?? "—"));
                page.Children.Add(text);
            }
        }
    }
}
