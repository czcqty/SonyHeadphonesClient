using System;
using System.IO;
using System.Linq;
using Microsoft.UI.Xaml;
using Forms = System.Windows.Forms;

namespace SoundConnect.WinUI;

public partial class App : Application
{
    internal static App CurrentApp => (App)Current;
    internal BackendClient Backend { get; private set; } = null!;
    internal bool Chinese { get; private set; } = true;
    internal bool Live { get; private set; }
    internal ElementTheme Theme { get; private set; } = ElementTheme.Default;
    private MainWindow? main;
    private MainWindow? panel;
    private Forms.NotifyIcon? tray;
    private Hotkeys? hotkeys;
    private readonly DispatcherTimer trayMonitor = new() { Interval = TimeSpan.FromSeconds(2) };
    internal bool HasTray => tray is not null && Hotkeys.TrayAvailable;
    internal bool HasGlobalKey(int id) => hotkeys?.Registered(id) == true;
    internal string ShortcutError => hotkeys?.Error ?? "";
    internal string T(string zh, string en) => Chinese ? zh : en;

    public App()
    {
        InitializeComponent();
        UnhandledException += (_, e) => {
            var directory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SoundConnect", "logs");
            Directory.CreateDirectory(directory);
            File.WriteAllText(Path.Combine(directory, "winui-error.log"), e.Exception.ToString());
        };
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        var arguments = Environment.GetCommandLineArgs();
        string Option(string name, string fallback) {
            int i = Array.IndexOf(arguments, name);
            return i >= 0 && i + 1 < arguments.Length ? arguments[i + 1] : fallback;
        }
        Live = arguments.Contains("--live");
        Chinese = Option("--locale", "en") != "en";
        Theme = Option("--theme", "system") switch { "light" => ElementTheme.Light, "dark" => ElementTheme.Dark, _ => ElementTheme.Default };
        var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
        Backend = new BackendClient(callback => dispatcher.TryEnqueue(() => callback()));
        Backend.Changed += ApplyPreferences;
        main = new MainWindow(false, Option("--route", "home"));
        main.Activate();
        UpdateTray();
        if (Live) hotkeys = new Hotkeys(InvokeShortcut);
        trayMonitor.Tick += (_, _) => UpdateTray();
        trayMonitor.Start();
        Backend.Start(Option("--backend", Path.Combine(AppContext.BaseDirectory, "backend", "SoundConnectBackend.exe")),
            Live, Option("--profile", "WH-1000XM6"), Option("--locale", ""), Option("--theme", ""));
        if (arguments.Contains("--panel")) TogglePanel();
        if (arguments.Contains("--smoke-test") || arguments.Contains("--parity-test"))
        {
            if (Live) throw new InvalidOperationException("Smoke testing cannot use --live");
            var smokeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(3) };
            smokeTimer.Tick += async (_, _) => {
                smokeTimer.Stop();
                try {
                    await main.SmokeTestAsync();
                    if (arguments.Contains("--parity-test")) await main.ParityTestAsync();
                    File.WriteAllText(Option("--smoke-report", Path.Combine(AppContext.BaseDirectory, "smoke.json")),
                        System.Text.Json.JsonSerializer.Serialize(new { ok = true, sentCommands = Backend.SentCommandCount, parity = main.ParityResults }));
                } catch (Exception error) {
                    Environment.ExitCode = 1;
                    File.WriteAllText(Option("--smoke-report", Path.Combine(AppContext.BaseDirectory, "smoke.json")),
                        System.Text.Json.JsonSerializer.Serialize(new { ok = false, error = error.ToString(), parity = main.ParityResults }));
                }
                Quit();
            };
            smokeTimer.Start();
        }
        string capture = Option("--capture", "");
        if (capture.Length > 0)
        {
            var timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(2) };
            timer.Tick += async (_, _) => {
                timer.Stop();
                try {
                    await (arguments.Contains("--panel") ? panel! : main).CaptureAsync(capture);
                    File.WriteAllText(capture + ".json", System.Text.Json.JsonSerializer.Serialize(new {
                        sentCommands = Backend.SentCommandCount, state = Backend.State, error = Backend.Error
                    }));
                }
                catch (Exception e) { File.WriteAllText(capture + ".error.txt", e.ToString()); }
                Quit();
            };
            timer.Start();
        }
    }

    private void UpdateTray()
    {
        if (!Hotkeys.TrayAvailable) {
            tray?.ContextMenuStrip?.Dispose(); tray?.Dispose(); tray = null;
            if (main?.AppWindow.IsVisible == false) ShowMain();
            return;
        }
        if (tray is not null) return;
        {
            tray = new Forms.NotifyIcon {
                Text = "Sound Connect",
                Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!) ?? System.Drawing.SystemIcons.Application,
                Visible = true
            };
            tray.MouseClick += (_, e) => { if (e.Button == Forms.MouseButtons.Left) TogglePanel(); };
            RebuildTrayMenu();
        }
    }
    internal void ShowMain() { main?.Activate(); }
    internal MainWindow QuickWindow => panel ??= new MainWindow(true);
    internal void TogglePanel()
    {
        if (!HasTray) { ShowMain(); return; }
        panel = QuickWindow;
        if (panel.AppWindow.IsVisible) panel.AppWindow.Hide();
        else { panel.PositionAtTray(); panel.Activate(); }
    }
    private void RebuildTrayMenu()
    {
        if (tray is null) return;
        var old = tray.ContextMenuStrip;
        var menu = new Forms.ContextMenuStrip();
        menu.Items.Add(T("打开 Sound Connect", "Open Sound Connect"), null, (_, _) => ShowMain());
        menu.Items.Add(T("快捷控制", "Quick controls"), null, (_, _) => TogglePanel());
        menu.Items.Add(T("播放 / 暂停", "Play / Pause"), null, (_, _) => InvokeShortcut(1)).Name = "device";
        menu.Items.Add(T("下一曲", "Next track"), null, (_, _) => InvokeShortcut(2)).Name = "device";
        menu.Items.Add(T("断开连接", "Disconnect"), null, (_, _) => Backend.Send("app", "disconnectDevice")).Name = "device";
        menu.Items.Add(new Forms.ToolStripSeparator());
        menu.Items.Add(T("退出", "Quit"), null, (_, _) => Quit());
        tray.ContextMenuStrip = menu; old?.Dispose();
        foreach (Forms.ToolStripItem item in menu.Items) if (item.Name == "device") item.Enabled = Backend.Connected;
    }
    internal void InvokeShortcut(int id)
    {
        if (id == 3) TogglePanel();
        else if (Backend.Connected && Backend.Flag("playback", "playbackSupported")) Backend.Send("playback", id == 1 ? "togglePlayback" : "nextTrack");
    }
    private void ApplyPreferences()
    {
        if (tray?.ContextMenuStrip is not null)
            foreach (Forms.ToolStripItem item in tray.ContextMenuStrip.Items) if (item.Name == "device") item.Enabled = Backend.Connected;
        var prefs = Backend.State["preferences"];
        if (prefs is null) return;
        bool chinese = prefs["locale"]?.ToString() == "zh_CN";
        var theme = prefs["themeMode"]?.ToString() switch { "light" => ElementTheme.Light, "dark" => ElementTheme.Dark, _ => ElementTheme.Default };
        if (Chinese != chinese) { Chinese = chinese; RebuildTrayMenu(); main?.Rebuild(); panel?.Rebuild(); }
        if (Theme != theme) { Theme = theme; main?.ApplyTheme(); panel?.ApplyTheme(); }
    }
    internal void ChangeLanguage(bool chinese)
    {
        Backend.Send("preferences", "setLocale", chinese ? "zh_CN" : "en");
    }
    internal void ChangeTheme(ElementTheme theme)
    {
        Backend.Send("preferences", "setThemeMode", theme == ElementTheme.Light ? "light" : theme == ElementTheme.Dark ? "dark" : "system");
    }
    private bool quitting;
    internal async void Quit()
    {
        if (quitting) return;
        quitting = true;
        trayMonitor.Stop(); hotkeys?.Dispose();
        tray?.ContextMenuStrip?.Dispose(); tray?.Dispose(); tray = null;
        await Backend.CloseAsync();
        panel?.ExitWindow(); main?.ExitWindow();
        Exit();
    }
}
