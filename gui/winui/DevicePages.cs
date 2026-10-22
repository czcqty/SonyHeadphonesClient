using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace SoundConnect.WinUI;

internal sealed partial class MainWindow
{
    private readonly Dictionary<string, Button> deviceButtons = new();
    private Task lastDeviceAction = Task.CompletedTask;

    private Button DeviceAction(string label, string id, string group, string method, params object[] args)
    {
        var button = new Button { Content = label };
        AutomationProperties.SetName(button, label);
        AutomationProperties.SetAutomationId(button, id);
        deviceButtons[id] = button;
        async Task Execute() {
            if (!button.IsEnabled) return;
            button.IsEnabled = false;
            try {
                if (method == "unpairDevice") await ConfirmCommand(group, method, args);
                else {
                    var reply = await backend.RequestAsync(group, method, args);
                    if (reply["ok"]?.GetValue<bool>() == true && method != "connectDevice")
                        actionMessage = T("请求已提交，正在等待设备状态更新。", "Request submitted. Waiting for the device to report its state.");
                }
            } catch (OperationCanceledException) { }
            catch (TimeoutException) { }
            finally { button.IsEnabled = backend.Connected || group == "app" && backend.Ready; Refresh(); }
        }
        button.Click += (_, _) => lastDeviceAction = Execute();
        return button;
    }

    private void ConnectionDevices()
    {
        var activity = new ProgressRing { Width = 28, Height = 28, HorizontalAlignment = HorizontalAlignment.Left };
        var progress = new TextBlock { TextWrapping = TextWrapping.Wrap };
        var list = new StackPanel { Spacing = 16 };
        page.Children.Add(activity); page.Children.Add(progress); page.Children.Add(list);
        string signature = "";
        bindings.Add(() => {
            activity.IsActive = backend.Ready && ConnectionBusy;
            activity.Visibility = activity.IsActive ? Visibility.Visible : Visibility.Collapsed;
            progress.Text = Dynamic(backend.Text("connection", "state"));
            string devices = backend.State["connection"]?["devices"]?.ToJsonString() ?? "[]";
            string next = devices + backend.Ready + backend.Connected + ConnectionBusy;
            if (signature == next) return;
            signature = next; list.Children.Clear();
            foreach (string id in deviceButtons.Keys.Where(k => k.StartsWith("discover.")).ToArray()) deviceButtons.Remove(id);
            foreach (var item in JsonNode.Parse(devices)!.AsArray()) {
                string name = item?["name"]?.ToString() ?? "", mac = item?["mac"]?.ToString() ?? "";
                var row = new StackPanel { Spacing = 6 };
                row.Children.Add(new TextBlock { Text = name.Length > 0 ? name : mac, FontSize = 18, TextWrapping = TextWrapping.Wrap });
                row.Children.Add(new TextBlock { Text = mac });
                var connect = DeviceAction(T("连接", "Connect") + " " + (name.Length > 0 ? name : mac), "discover." + mac, "app", "connectDevice", name, mac);
                connect.IsEnabled = backend.Ready && !backend.Connected && !ConnectionBusy;
                row.Children.Add(connect); list.Children.Add(row);
            }
            if (list.Children.Count == 0 && backend.Ready && !ConnectionBusy)
                list.Children.Add(new TextBlock { Text = T("没有发现耳机。请打开耳机并扫描设备。", "No headphones found. Turn them on and scan for devices."), TextWrapping = TextWrapping.Wrap });
        });
    }

    private void ManagedDevices()
    {
        var list = new StackPanel { Spacing = 24 };
        page.Children.Add(list);
        string signature = "";
        bindings.Add(() => {
            string devices = backend.Connected ? backend.State["deviceManager"]?["devices"]?.ToJsonString() ?? "[]" : "[]";
            string next = devices + backend.Connected + backend.Flag("deviceManager", "supported") + backend.Flag("deviceManager", "sourceSwitchSupported");
            if (signature == next) return;
            signature = next; list.Children.Clear();
            foreach (string id in deviceButtons.Keys.Where(k => k.StartsWith("paired.")).ToArray()) deviceButtons.Remove(id);
            foreach (var item in JsonNode.Parse(devices)!.AsArray()) {
                string name = item?["name"]?.ToString() ?? "", mac = item?["mac"]?.ToString() ?? "";
                bool connected = item?["connected"]?.GetValue<bool>() ?? false, playback = item?["playback"]?.GetValue<bool>() ?? false;
                string displayName = name.Length > 0 ? name : mac;
                var row = new StackPanel { Spacing = 8 };
                row.Children.Add(new TextBlock { Text = displayName, FontSize = 18, TextWrapping = TextWrapping.Wrap });
                row.Children.Add(new TextBlock { Text = mac + " · " + Dynamic(connected ? "connected" : "disconnected") + (playback ? T(" · 当前播放设备", " · Current playback device") : ""), TextWrapping = TextWrapping.Wrap });
                var connect = DeviceAction((connected ? T("断开连接", "Disconnect") : T("连接", "Connect")) + " " + displayName,
                    "paired." + mac + ".connection", "deviceManager", connected ? "disconnectDevice" : "connectDevice", mac);
                connect.IsEnabled = backend.Flag("deviceManager", "supported"); row.Children.Add(connect);
                if (backend.Flag("deviceManager", "sourceSwitchSupported") && connected && !playback)
                    row.Children.Add(DeviceAction(T("用于播放", "Use for playback") + " " + displayName, "paired." + mac + ".playback", "deviceManager", "switchDevice", mac));
                var unpair = DeviceAction(T("取消配对", "Unpair") + " " + displayName, "paired." + mac + ".unpair", "deviceManager", "unpairDevice", mac);
                unpair.IsEnabled = backend.Flag("deviceManager", "supported"); row.Children.Add(unpair);
                list.Children.Add(row);
            }
            if (list.Children.Count == 0)
                list.Children.Add(new TextBlock { Text = backend.Connected ? T("耳机尚未报告已配对设备。", "The headphones have not reported paired devices.") : T("连接耳机后可管理已配对设备。", "Connect headphones to manage paired devices."), TextWrapping = TextWrapping.Wrap });
        });
    }
}
