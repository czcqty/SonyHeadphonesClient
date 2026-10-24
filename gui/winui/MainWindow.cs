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

internal sealed partial class MainWindow : Window
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
    private string actionMessage = "";
    private bool popupEscape;
    private readonly DispatcherTimer dismissTimer = new() { Interval = TimeSpan.FromMilliseconds(150) };
    private string T(string zh, string en) => app.T(zh, en);

    public MainWindow(bool compact, string initialRoute = "home")
    {
        this.compact = compact;
        route = initialRoute == "appearance" ? "application" : initialRoute;
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
        AppWindow.Resize(new SizeInt32((int)((compact ? 440 : 1080) * scale), (int)((compact ? 560 : 800) * scale)));
        AppWindow.Closing += (_, e) => {
            if (!exiting) { e.Cancel = true; if (app.HasTray) AppWindow.Hide(); else app.Quit(); }
        };
        Closed += (_, _) => { dismissTimer.Stop(); backend.Changed -= Refresh; };
        Activated += (_, e) => {
            if (compact && app.HasTray && e.WindowActivationState == WindowActivationState.Deactivated) dismissTimer.Start();
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
        // Keep the window's attached root across locale changes. Replacing it
        // detaches XamlRoot until the next layout pass, breaking popups/dialogs.
        if (root is null) {
            root = new Grid { RequestedTheme = app.Theme };
            root.KeyDown += (_, e) => {
                if (compact && e.Key == VirtualKey.Escape && !popupEscape && !GetOpenPopups().Any()) {
                    AppWindow.Hide(); if (!app.HasTray) app.ShowMain(); e.Handled = true;
                }
            };
            Content = root;
        }
        root.Children.Clear();
        root.RowDefinitions.Clear();
        root.KeyboardAccelerators.Clear();
        for (int i = 1; i <= 3; ++i) {
            int id = i;
            var accelerator = new KeyboardAccelerator { Key = i == 1 ? VirtualKey.P : i == 2 ? VirtualKey.N : VirtualKey.M,
                Modifiers = VirtualKeyModifiers.Control | VirtualKeyModifiers.Menu };
            accelerator.Invoked += (_, e) => { if (!app.HasGlobalKey(id)) app.InvokeShortcut(id); e.Handled = true; };
            root.KeyboardAccelerators.Add(accelerator);
        }
        if (compact) BuildPage();
        else
        {
            navigation = new NavigationView {
                PaneDisplayMode = NavigationViewPaneDisplayMode.Left,
                IsBackButtonVisible = NavigationViewBackButtonVisible.Collapsed,
                IsSettingsVisible = false, OpenPaneLength = 220,
                PaneTitle = "Sound Connect", IsPaneOpen = true
            };
            foreach (var entry in Catalog["pages"]!.AsArray()) {
                var spec = entry!.AsObject();
                navigation.MenuItems.Add(new NavigationViewItem { Content = CatalogText(spec["label"]),
                    Tag = spec["id"]!.GetValue<string>() });
            }
            navigation.SelectionChanged += (_, e) => {
                if (e.SelectedItem is NavigationViewItem item) { route = (string)item.Tag; BuildPage(); }
            };
            root.Children.Add(navigation);
            var view = navigation;
            view.Loaded += (_, _) => {
                if (navigation == view && view.SelectedItem is null)
                    view.SelectedItem = view.MenuItems.Cast<NavigationViewItem>().FirstOrDefault(x => (string)x.Tag == route) ?? view.MenuItems[0];
            };
            BuildPage();
        }
        ApplyTheme();
        Refresh();
    }

    private void BuildPage()
    {
        bindings.Clear();
        fields.Clear(); deviceButtons.Clear();
        page = new StackPanel { Spacing = compact ? 16 : 24, Margin = new Thickness(compact ? 20 : 32), MaxWidth = 860, HorizontalAlignment = HorizontalAlignment.Stretch };
        var scroll = new ScrollViewer { Content = page, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        if (compact) {
            root.Children.Clear(); root.RowDefinitions.Clear();
            root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            root.Children.Add(scroll);
            var open = ActionButton(T("打开完整设置", "Open full settings"), () => { AppWindow.Hide(); app.ShowMain(); });
            open.Margin = new Thickness(20, 8, 20, 12); open.HorizontalAlignment = HorizontalAlignment.Stretch;
            Grid.SetRow(open, 1); root.Children.Add(open);
        }
        else navigation!.Content = scroll;
        var status = new InfoBar { IsClosable = false, Severity = InfoBarSeverity.Informational };
        page.Children.Add(status);
        var retry = ActionButton(Dynamic("backend-retry"), backend.Restart);
        page.Children.Add(retry);
        bindings.Add(() => {
            retry.Visibility = backend.Ready ? Visibility.Collapsed : Visibility.Visible;
            retry.IsEnabled = backend.Error != "backend-starting";
        });
        bindings.Add(() => {
            status.IsOpen = !app.Live || backend.Error.Length > 0 || ConnectionError().Length > 0 || app.ShortcutError.Length > 0 || !backend.Connected || actionMessage.Length > 0;
            status.Severity = backend.Error.Length > 0 ? InfoBarSeverity.Error : InfoBarSeverity.Informational;
            status.Title = backend.Error.Length > 0 ? T("操作未完成", "Operation failed") : app.Live ? T("设备连接", "Device connection") : T("预览模式", "Preview mode");
            status.Message = backend.Error.Length > 0 ? Dynamic("Backend error: %1").Replace("%1", Dynamic(backend.Error)) : ConnectionError().Length > 0 ? ConnectionError() : app.ShortcutError.Length > 0 ? T("快捷键注册失败（可能已被占用）：", "Shortcut registration failed (possibly already in use): ") + app.ShortcutError : app.Live ? T("请在设备页连接耳机。", "Connect headphones on the Devices page.") : T("使用模拟设备，操作不会发送至真实耳机。", "Simulated device. Changes are not sent to real headphones.");
        });
        bindings.Add(() => { if (backend.Error.Length == 0 && ConnectionError().Length == 0 && actionMessage.Length > 0) status.Message = actionMessage; });
        bindings.Add(() => { if (!backend.Ready && backend.Diagnostic.Length > 0) status.Message += "\n" + backend.Diagnostic; });
        if (!compact) {
            var connect = ActionButton(T("打开连接页", "Open Connection"), () => Navigate("connection"));
            bindings.Add(() => connect.Visibility = backend.Ready && !backend.Connected && route != "connection" ? Visibility.Visible : Visibility.Collapsed);
            page.Children.Add(connect);
        }
        if (compact) Overview();
        else {
            CatalogPage(route);
            if (route == "application") {
                page.Children.Add(new TextBlock { Text = T("Ctrl+Alt+P 播放/暂停 · Ctrl+Alt+N 下一曲 · Ctrl+Alt+M 快捷面板。预览设置仅本次生效。", "Ctrl+Alt+P play/pause · Ctrl+Alt+N next · Ctrl+Alt+M quick controls. Preview preferences are session only."), TextWrapping = TextWrapping.Wrap });
                page.Children.Add(ActionButton(T("退出应用", "Quit application"), app.Quit));
            }
        }
        Refresh();
    }

    private void Navigate(string destination)
    {
        var item = navigation?.MenuItems.Cast<NavigationViewItem>().FirstOrDefault(x => (string)x.Tag == destination);
        if (item is not null && !ReferenceEquals(navigation!.SelectedItem, item)) navigation.SelectedItem = item;
        else { route = destination; BuildPage(); }
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

    private void Overview()
    {
        CatalogPage("quick");
        var media = new TextBlock { TextWrapping = TextWrapping.Wrap };
        page.Children.Add(media);
        bindings.Add(() => media.Text = backend.Connected ? string.Join(" · ", new[] {
            backend.Text("playback", "title"), backend.Text("playback", "artist"), backend.Text("playback", "codec")
        }.Where(s => s.Length > 0)) : "");
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
