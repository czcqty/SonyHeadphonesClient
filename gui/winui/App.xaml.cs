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
    internal string T(string zh, string en) => Chinese ? zh : en;

    public App()
    {
        InitializeComponent();
        UnhandledException += (_, e) => {
            File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "winui-error.log"), e.Exception.ToString());
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
        Chinese = Option("--locale", "zh_CN") != "en";
        Theme = Option("--theme", "system") switch { "light" => ElementTheme.Light, "dark" => ElementTheme.Dark, _ => ElementTheme.Default };
        Backend = new BackendClient(Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread());
        main = new MainWindow(false, Option("--route", "home"));
        main.Activate();
        var menu = new Forms.ContextMenuStrip();
        menu.Items.Add(T("打开 Sound Connect", "Open Sound Connect"), null, (_, _) => ShowMain());
        menu.Items.Add(T("快捷控制", "Quick controls"), null, (_, _) => TogglePanel());
        menu.Items.Add(new Forms.ToolStripSeparator());
        menu.Items.Add(T("退出", "Quit"), null, (_, _) => Quit());
        tray = new Forms.NotifyIcon {
            Text = "Sound Connect · WinUI preview",
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!) ?? System.Drawing.SystemIcons.Application,
            ContextMenuStrip = menu, Visible = true
        };
        tray.MouseClick += (_, e) => { if (e.Button == Forms.MouseButtons.Left) TogglePanel(); };
        Backend.Start(Option("--backend", Path.Combine(AppContext.BaseDirectory, "backend", "SonyHeadphonesClient_winui_backend.exe")),
            Live, Option("--profile", "WH-1000XM6"));
        if (arguments.Contains("--panel")) TogglePanel();
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

    internal void ShowMain() { main?.Activate(); }
    internal void TogglePanel()
    {
        panel ??= new MainWindow(true);
        if (panel.AppWindow.IsVisible) panel.AppWindow.Hide();
        else { panel.PositionAtTray(); panel.Activate(); }
    }
    internal void ChangeLanguage(bool chinese)
    {
        Chinese = chinese;
        main?.Rebuild(); panel?.Rebuild();
    }
    internal void ChangeTheme(ElementTheme theme)
    {
        Theme = theme;
        main?.ApplyTheme(); panel?.ApplyTheme();
    }
    internal void Quit()
    {
        tray?.Dispose(); tray = null;
        Backend.Dispose();
        panel?.ExitWindow(); main?.ExitWindow();
        Exit();
    }
}
