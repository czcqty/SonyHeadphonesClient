using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.IO;
using System.Threading.Tasks;
using System.Text.Json.Nodes;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;
using Windows.System;

namespace SoundConnect.WinUI;

internal sealed class MainWindow : Window
{
    private readonly bool compact;
    private readonly List<Action> bindings = new();
    private readonly App app = App.CurrentApp;
    private BackendClient backend => app.Backend;
    private Grid root = null!;
    private StackPanel page = null!;
    private NavigationView? navigation;
    private string route = "home";
    private bool updating;
    private bool exiting;
    private string deviceSignature = "";
    private readonly DispatcherTimer dismissTimer = new() { Interval = TimeSpan.FromMilliseconds(150) };
    private string T(string zh, string en) => app.T(zh, en);

    public MainWindow(bool compact, string initialRoute = "home")
    {
        this.compact = compact;
        route = initialRoute;
        Title = compact ? "Sound Connect" : "Sound Connect · WinUI";
        SystemBackdrop = compact ? new DesktopAcrylicBackdrop() : new MicaBackdrop();
        var presenter = (OverlappedPresenter)AppWindow.Presenter;
        if (compact)
        {
            presenter.SetBorderAndTitleBar(true, false);
            presenter.IsResizable = false;
            presenter.IsMaximizable = false;
            presenter.IsMinimizable = false;
            AppWindow.IsShownInSwitchers = false;
        }
        double scale = GetDpiForWindow(WinRT.Interop.WindowNative.GetWindowHandle(this)) / 96.0;
        AppWindow.Resize(new SizeInt32((int)((compact ? 420 : 1080) * scale), (int)((compact ? 400 : 800) * scale)));
        AppWindow.Closing += (_, e) => {
            if (!exiting) { e.Cancel = true; AppWindow.Hide(); }
        };
        Closed += (_, _) => { dismissTimer.Stop(); backend.Changed -= Refresh; };
        Activated += (_, e) => {
            if (compact && e.WindowActivationState == WindowActivationState.Deactivated) dismissTimer.Start();
            else dismissTimer.Stop();
        };
        dismissTimer.Tick += (_, _) => {
            // A WinUI popup may briefly take focus during activation. Keep the
            // panel while focus belongs to its window or an attached popup.
            if (GetOpenPopups().Any()) return;
            if (GetForegroundWindow() == WinRT.Interop.WindowNative.GetWindowHandle(this)) return;
            dismissTimer.Stop(); AppWindow.Hide();
        };
        backend.Changed += Refresh;
        Rebuild();
    }

    private IReadOnlyList<Popup> GetOpenPopups() => root.XamlRoot is null
        ? Array.Empty<Popup>() : VisualTreeHelper.GetOpenPopupsForXamlRoot(root.XamlRoot);

    internal void ApplyTheme() { root.RequestedTheme = app.Theme; }

    internal void Rebuild()
    {
        bindings.Clear();
        root = new Grid { RequestedTheme = app.Theme };
        root.KeyDown += (_, e) => {
            if (compact && e.Key == VirtualKey.Escape && !GetOpenPopups().Any()) {
                AppWindow.Hide(); e.Handled = true;
            }
        };
        if (compact) BuildPage();
        else
        {
            navigation = new NavigationView {
                PaneDisplayMode = NavigationViewPaneDisplayMode.Left,
                IsBackButtonVisible = NavigationViewBackButtonVisible.Collapsed,
                IsSettingsVisible = false, OpenPaneLength = 220,
                PaneTitle = "Sound Connect", IsPaneOpen = true
            };
            var routes = new[] {
                ("home", T("概览", "Overview"), Symbol.Home),
                ("sound", T("声音与降噪", "Sound & noise control"), Symbol.Volume),
                ("equalizer", T("均衡器", "Equalizer"), Symbol.Audio),
                ("behavior", T("耳机行为", "Headphone behavior"), Symbol.Audio),
                ("touch", T("触控与语音", "Touch & voice"), Symbol.TouchPointer),
                ("devices", T("设备", "Devices"), Symbol.Link),
                ("appearance", T("应用设置", "App settings"), Symbol.Setting)
            };
            foreach (var (key, label, icon) in routes)
                navigation.MenuItems.Add(new NavigationViewItem { Content = label, Tag = key, Icon = new SymbolIcon(icon) });
            navigation.SelectionChanged += (_, e) => {
                if (e.SelectedItem is NavigationViewItem item) { route = (string)item.Tag; BuildPage(); }
            };
            root.Children.Add(navigation);
            navigation.SelectedItem = navigation.MenuItems.Cast<NavigationViewItem>().First(x => (string)x.Tag == route);
        }
        Content = root;
        ApplyTheme();
        Refresh();
    }

    private void BuildPage()
    {
        bindings.Clear(); deviceSignature = "";
        page = new StackPanel { Spacing = compact ? 16 : 24, Margin = new Thickness(compact ? 20 : 32), MaxWidth = 860, HorizontalAlignment = HorizontalAlignment.Stretch };
        var scroll = new ScrollViewer { Content = page, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        if (compact) { root.Children.Clear(); root.Children.Add(scroll); }
        else navigation!.Content = scroll;
        var status = new InfoBar { IsClosable = false, Severity = InfoBarSeverity.Informational };
        if (!compact) page.Children.Add(status);
        bindings.Add(() => {
            status.IsOpen = !app.Live || backend.Error.Length > 0 || !backend.Connected;
            status.Severity = backend.Error.Length > 0 ? InfoBarSeverity.Error : InfoBarSeverity.Informational;
            status.Title = backend.Error.Length > 0 ? T("连接错误", "Connection error") : app.Live ? T("设备连接", "Device connection") : T("预览模式", "Preview mode");
            status.Message = backend.Error.Length > 0 ? backend.Error : app.Live ? T("请在设备页连接耳机。", "Connect headphones on the Devices page.") : T("使用模拟设备，操作不会发送至真实耳机。", "Simulated device. Changes are not sent to real headphones.");
        });
        if (compact || route == "home") Overview();
        else if (route == "sound") Sound();
        else if (route == "equalizer") Equalizer();
        else if (route == "behavior") Behavior();
        else if (route == "touch") Touch();
        else if (route == "devices") Devices();
        else Appearance();
        Refresh();
    }

    private void Refresh()
    {
        updating = true;
        try { foreach (var binding in bindings.ToArray()) binding(); }
        finally { updating = false; }
    }

    private TextBlock Heading(string text, bool large = false)
    {
        var block = new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap,
            Style = (Style)Application.Current.Resources[large ? "TitleTextBlockStyle" : "SubtitleTextBlockStyle"] };
        page.Children.Add(block); return block;
    }

    private Button ActionButton(string text, Action action)
    {
        var button = new Button { Content = text };
        button.Click += (_, _) => action(); return button;
    }

    private StackPanel Row(params UIElement[] elements)
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        foreach (var element in elements) row.Children.Add(element);
        return row;
    }

    private void Toggle(string label, string group, string property, string supported, string method)
    {
        var toggle = new ToggleSwitch { Header = label, OnContent = T("开", "On"), OffContent = T("关", "Off") };
        toggle.Toggled += (_, _) => { if (!updating) backend.Send(group, method, toggle.IsOn); };
        bindings.Add(() => { toggle.IsEnabled = backend.Connected && backend.Flag(group, supported); toggle.IsOn = backend.Flag(group, property); });
        page.Children.Add(toggle);
    }

    private void Slider(string label, string group, string supported, int min, int max, Func<int> value, Action<int> commit)
    {
        var slider = new Slider { Header = label, Minimum = min, Maximum = max, StepFrequency = 1, TickFrequency = 1, HorizontalAlignment = HorizontalAlignment.Stretch };
        AutomationProperties.SetName(slider, label);
        bool dirty = false;
        slider.ValueChanged += (_, _) => { if (!updating) dirty = true; };
        void Commit() { if (!dirty) return; dirty = false; commit((int)Math.Round(slider.Value)); }
        slider.AddHandler(UIElement.PointerReleasedEvent, new PointerEventHandler((_, _) => Commit()), true);
        slider.AddHandler(UIElement.KeyUpEvent, new KeyEventHandler((_, _) => Commit()), true);
        slider.LostFocus += (_, _) => Commit();
        bindings.Add(() => {
            slider.IsEnabled = backend.Connected && backend.Flag(group, supported);
            if (!dirty) slider.Value = value();
        });
        page.Children.Add(slider);
    }

    private void Choice(string label, string group, string property, string supported, string method, string[] labels, int[]? values = null)
    {
        values ??= Enumerable.Range(0, labels.Length).ToArray();
        var combo = new ComboBox { Header = label, ItemsSource = labels, MinWidth = compact ? 200 : 280, HorizontalAlignment = HorizontalAlignment.Left };
        combo.SelectionChanged += (_, _) => { if (!updating && combo.SelectedIndex >= 0) backend.Send(group, method, values[combo.SelectedIndex]); };
        bindings.Add(() => { combo.IsEnabled = backend.Connected && backend.Flag(group, supported); combo.SelectedIndex = Array.IndexOf(values, backend.Number(group, property)); });
        page.Children.Add(combo);
    }

    private void Overview()
    {
        var model = Heading("Sound Connect", !compact);
        var battery = new TextBlock { TextWrapping = TextWrapping.Wrap };
        page.Children.Add(battery);
        bindings.Add(() => {
            model.Text = backend.Text("app", "modelName");
            var parts = new List<string> { T("电量", "Battery") + $" {backend.Number("playback", "batteryLeft")}%" };
            if (backend.Flag("playback", "hasRightBattery")) parts.Add(T("右", "R") + $" {backend.Number("playback", "batteryRight")}%");
            if (backend.Flag("playback", "hasCaseBattery")) parts.Add(T("充电盒", "Case") + $" {backend.Number("playback", "batteryCase")}%");
            battery.Text = backend.Connected ? string.Join("   ·   ", parts) : T("未连接耳机", "Headphones disconnected");
        });
        NoiseModes();
        Slider(T("音量", "Volume"), "playback", "volumeSupported", 0, 30,
            () => backend.Number("playback", "volume"), v => backend.Send("playback", "setVolume", v));
        var playback = Row(ActionButton(T("上一曲", "Previous"), () => backend.Send("playback", "previousTrack")),
            ActionButton(T("播放 / 暂停", "Play / Pause"), () => backend.Send("playback", "togglePlayback")),
            ActionButton(T("下一曲", "Next"), () => backend.Send("playback", "nextTrack")));
        bindings.Add(() => {
            foreach (var button in playback.Children.Cast<Button>())
                button.IsEnabled = backend.Connected && backend.Flag("playback", "playbackSupported");
        });
        page.Children.Add(playback);
        if (compact)
        {
            var open = ActionButton(T("打开完整设置", "Open full settings"), () => { AppWindow.Hide(); app.ShowMain(); });
            open.HorizontalAlignment = HorizontalAlignment.Stretch;
            page.Children.Add(open);
        }
        else
        {
            var details = new TextBlock { TextWrapping = TextWrapping.Wrap };
            page.Children.Add(details);
            bindings.Add(() => details.Text = $"{T("固件", "Firmware")}  {backend.Text("playback", "firmware")}    ·    {backend.Text("playback", "codec")}");
            page.Children.Add(ActionButton(T("打开托盘快捷面板", "Open tray quick controls"), app.TogglePanel));
        }
    }

    private void NoiseModes()
    {
        page.Children.Add(new TextBlock { Text = T("环境声音控制", "Ambient sound control") });
        var modes = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12 };
        var labels = new[] { T("关闭", "Off"), T("降噪", "Noise canceling"), T("环境声", "Ambient sound") };
        for (int i = 0; i < labels.Length; i++) {
            int mode = i;
            var radio = new RadioButton { Content = labels[i], MinWidth = 0, GroupName = "NoiseMode" };
            // Click is a user action; SelectionChanged also fires when WinUI
            // realizes the items or the backend synchronizes their state.
            radio.Click += (_, _) => backend.Send("noiseControl", "setMode", mode);
            bindings.Add(() => {
                radio.IsEnabled = backend.Connected && backend.Flag("noiseControl", mode == 2 ? "ambientSupported" : "supported");
                radio.IsChecked = backend.Number("noiseControl", "mode") == mode;
            });
            modes.Children.Add(radio);
        }
        page.Children.Add(modes);
    }

    private void Sound()
    {
        Heading(T("声音与降噪", "Sound & noise control"), true);
        NoiseModes();
        Slider(T("环境声级别", "Ambient sound level"), "noiseControl", "ambientSupported", 1, 20,
            () => backend.Number("noiseControl", "ambientLevel"), v => backend.Send("noiseControl", "setAmbientLevel", v));
        Toggle(T("专注于语音", "Focus on voice"), "noiseControl", "focusOnVoice", "ambientSupported", "setFocusOnVoice");
        Toggle(T("自适应降噪", "Adaptive noise cancellation"), "noiseControl", "adaptiveEnabled", "adaptiveSupported", "setAdaptiveEnabled");
        Toggle("DSEE", "settings", "dseeEnabled", "dseeSupported", "setDseeEnabled");
        Toggle(T("优先音质", "Prioritize sound quality"), "settings", "soundQualityPrior", "connectionQualitySupported", "setSoundQualityPrior");
        Toggle(T("自适应音量", "Adaptive volume"), "settings", "adaptiveVolumeEnabled", "adaptiveVolumeSupported", "setAdaptiveVolumeEnabled");
        Toggle(T("安全聆听", "Safe listening"), "settings", "safeListeningEnabled", "safeListeningSupported", "setSafeListeningEnabled");
    }

    private void Equalizer()
    {
        Heading(T("均衡器", "Equalizer"), true);
        var preset = new ComboBox { Header = T("预设", "Preset"), MinWidth = 280 };
        string optionsSignature = "";
        bindings.Add(() => {
            preset.IsEnabled = backend.Connected && backend.Flag("equalizer", "supported");
            var options = backend.State["equalizer"]?["presetOptions"]?.AsArray();
            string signature = options?.ToJsonString() ?? "";
            if (signature != optionsSignature) { optionsSignature = signature; preset.ItemsSource = options?.Select(x => x?["id"]?.GetValue<string>() ?? "").ToList(); }
            preset.SelectedItem = backend.Text("equalizer", "presetId");
        });
        preset.SelectionChanged += (_, _) => { if (!updating && preset.SelectedItem is string id) backend.Send("equalizer", "selectPreset", id); };
        page.Children.Add(preset);
        for (int i = 0; i < 5; i++) {
            int band = i;
            Slider(new[] { "400 Hz", "1 kHz", "2.5 kHz", "6.3 kHz", "16 kHz" }[i], "equalizer", "supported", -10, 10,
                () => backend.State["equalizer"]?["bands"] is JsonArray bands && band < bands.Count ? bands[band]!.GetValue<int>() : 0,
                v => backend.Send("equalizer", "setBand", band, v));
        }
        Slider("Clear Bass", "equalizer", "supported", -10, 10,
            () => backend.Number("equalizer", "clearBass"), v => backend.Send("equalizer", "setClearBass", v));
    }

    private void Behavior()
    {
        Heading(T("耳机行为", "Headphone behavior"), true);
        Toggle(T("取下耳机时自动暂停", "Pause when headphones are removed"), "settings", "autoPauseEnabled", "autoPauseSupported", "setAutoPauseEnabled");
        Toggle(T("自动关机", "Automatic power off"), "settings", "autoPowerOffEnabled", "autoPowerOffSupported", "setAutoPowerOffEnabled");
        Toggle(T("头部手势", "Head gestures"), "settings", "headGestureEnabled", "headGestureSupported", "setHeadGestureEnabled");
        Toggle(T("智能免摘", "Speak-to-Chat"), "settings", "speakToChatEnabled", "speakToChatSupported", "setSpeakToChatEnabled");
        Toggle(T("捕捉语音", "Capture voice"), "settings", "captureVoiceEnabled", "captureVoiceSupported", "setCaptureVoiceEnabled");
    }

    private void Touch()
    {
        Heading(T("触控与语音", "Touch & voice"), true);
        foreach (var side in new[] { "Left", "Right" })
        {
            var combo = new ComboBox { Header = side == "Left" ? T("左侧触控", "Left touch control") : T("右侧触控", "Right touch control"), MinWidth = 280 };
            string signature = ""; List<int> values = new();
            bindings.Add(() => {
                combo.IsEnabled = backend.Connected && backend.Flag("settings", "touchSupported");
                var options = backend.State["settings"]?["touchOptions"]?.AsArray();
                string next = options?.ToJsonString() ?? "";
                if (next != signature) {
                    signature = next;
                    values = options?.Select(x => x!["value"]!.GetValue<int>()).ToList() ?? new();
                    combo.ItemsSource = options?.Select(x => x!["name"]!.GetValue<string>()).ToList();
                }
                combo.SelectedIndex = values.IndexOf(backend.Number("settings", "touch" + side));
            });
            combo.SelectionChanged += (_, _) => { if (!updating && combo.SelectedIndex >= 0) backend.Send("settings", "setTouch" + side, values[combo.SelectedIndex]); };
            page.Children.Add(combo);
        }
        Toggle(T("语音提示", "Voice guidance"), "settings", "voiceGuidanceEnabled", "voiceGuidanceSupported", "setVoiceGuidanceEnabled");
        Slider(T("语音提示音量", "Voice guidance volume"), "settings", "voiceGuidanceVolumeSupported", -2, 2,
            () => backend.Number("settings", "voiceGuidanceVolume"), v => backend.Send("settings", "setVoiceGuidanceVolume", v));
    }

    private void Devices()
    {
        Heading(T("设备", "Devices"), true);
        page.Children.Add(Row(ActionButton(T("刷新设备", "Refresh devices"), () => backend.Send("app", "refreshDevices")),
            ActionButton(T("断开连接", "Disconnect"), () => backend.Send("app", "disconnectDevice"))));
        var devices = new StackPanel { Spacing = 12 }; page.Children.Add(devices);
        bindings.Add(() => {
            var list = backend.State["connection"]?["devices"]?.AsArray();
            var signature = list?.ToJsonString() ?? "";
            if (signature == deviceSignature) return;
            deviceSignature = signature; devices.Children.Clear();
            if (list is null) return;
            foreach (var device in list) {
                string name = device?["name"]?.GetValue<string>() ?? "Headphones";
                string mac = device?["mac"]?.GetValue<string>() ?? "";
                devices.Children.Add(ActionButton(name + "  ·  " + mac, () => backend.Send("app", "connectDevice", name, mac)));
            }
        });
        Toggle(T("多点配对模式", "Multipoint pairing mode"), "deviceManager", "pairingMode", "supported", "setPairingMode");
    }

    private void Appearance()
    {
        Heading(T("应用设置", "App settings"), true);
        var theme = new ComboBox { Header = T("外观", "Appearance"), MinWidth = 280,
            ItemsSource = new[] { T("跟随系统", "Follow system"), T("浅色", "Light"), T("深色", "Dark") },
            SelectedIndex = app.Theme == ElementTheme.Light ? 1 : app.Theme == ElementTheme.Dark ? 2 : 0 };
        theme.SelectionChanged += (_, _) => app.ChangeTheme(theme.SelectedIndex == 1 ? ElementTheme.Light : theme.SelectedIndex == 2 ? ElementTheme.Dark : ElementTheme.Default);
        page.Children.Add(theme);
        var language = new ComboBox { Header = T("语言", "Language"), MinWidth = 280,
            ItemsSource = new[] { "简体中文", "English" }, SelectedIndex = app.Chinese ? 0 : 1 };
        language.SelectionChanged += (_, _) => app.ChangeLanguage(language.SelectedIndex == 0);
        page.Children.Add(language);
        page.Children.Add(new TextBlock { Text = T("WinUI 实验分支。外观选择仅在本次运行中生效。", "WinUI experimental branch. Appearance choices apply to this session only."), TextWrapping = TextWrapping.Wrap });
        page.Children.Add(ActionButton(T("退出应用", "Quit application"), app.Quit));
    }

    internal void PositionAtTray()
    {
        GetCursorPos(out POINT point);
        var area = DisplayArea.GetFromPoint(new PointInt32(point.X, point.Y), DisplayAreaFallback.Nearest).WorkArea;
        int width = AppWindow.Size.Width, height = AppWindow.Size.Height;
        AppWindow.Move(new PointInt32(Math.Clamp(point.X - width / 2, area.X, Math.Max(area.X, area.X + area.Width - width)),
            Math.Clamp(point.Y - height - 12, area.Y, Math.Max(area.Y, area.Y + area.Height - height))));
    }

    internal void ExitWindow() { exiting = true; Close(); }
    internal async Task CaptureAsync(string path)
    {
        // RenderTargetBitmap captures the actual WinUI control tree. Compositor
        // backdrop/titlebar are outside that tree, so use the system page brush
        // for deterministic captures, without claiming to capture Mica itself.
        var background = (Grid)Microsoft.UI.Xaml.Markup.XamlReader.Load(
            "<Grid xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' Background='{ThemeResource ApplicationPageBackgroundThemeBrush}'/>");
        background.RequestedTheme = root.ActualTheme;
        root.Children.Insert(0, background);
        await Task.Delay(100);
        var bitmap = new Microsoft.UI.Xaml.Media.Imaging.RenderTargetBitmap();
        await bitmap.RenderAsync(root);
        var pixels = await bitmap.GetPixelsAsync();
        using var stream = new Windows.Storage.Streams.InMemoryRandomAccessStream();
        var encoder = await Windows.Graphics.Imaging.BitmapEncoder.CreateAsync(Windows.Graphics.Imaging.BitmapEncoder.PngEncoderId, stream);
        var bytes = new byte[pixels.Length];
        Windows.Storage.Streams.DataReader.FromBuffer(pixels).ReadBytes(bytes);
        encoder.SetPixelData(Windows.Graphics.Imaging.BitmapPixelFormat.Bgra8, Windows.Graphics.Imaging.BitmapAlphaMode.Premultiplied,
            (uint)bitmap.PixelWidth, (uint)bitmap.PixelHeight, 96, 96, bytes);
        await encoder.FlushAsync();
        stream.Seek(0);
        using var reader = new Windows.Storage.Streams.DataReader(stream.GetInputStreamAt(0));
        await reader.LoadAsync((uint)stream.Size);
        var png = new byte[stream.Size]; reader.ReadBytes(png);
        await File.WriteAllBytesAsync(path, png);
    }
    [StructLayout(LayoutKind.Sequential)] private struct POINT { public int X; public int Y; }
    [DllImport("user32.dll")] private static extern bool GetCursorPos(out POINT point);
    [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] private static extern uint GetDpiForWindow(IntPtr hwnd);
}
