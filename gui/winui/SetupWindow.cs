using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Threading.Tasks;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;
using Windows.Storage.Pickers;

namespace SoundConnect.WinUI;

internal sealed class SetupWindow : Window
{
    private readonly string installer;
    private readonly bool chinese = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "zh";
    private readonly TextBox directory = new();
    private readonly CheckBox desktop = new();
    private readonly Button browse = new();
    private readonly Button install = new();
    private readonly Button close = new();
    private readonly ProgressBar progress = new() { IsIndeterminate = true, Visibility = Visibility.Collapsed };
    private readonly InfoBar result = new() { IsClosable = false };
    private bool installing;
    private bool installed;

    private string T(string zh, string en) => chinese ? zh : en;

    internal SetupWindow(string installer, string version, string initialDirectory)
    {
        this.installer = installer;
        Title = T("SonyHeadphonesClient 安装程序", "SonyHeadphonesClient Setup");
        AppWindow.SetIcon(Path.Combine(AppContext.BaseDirectory, "app-icon.ico"));
        SystemBackdrop = new MicaBackdrop();
        var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
        uint dpi = GetDpiForWindow(hwnd);
        var area = DisplayArea.GetFromWindowId(AppWindow.Id, DisplayAreaFallback.Nearest).WorkArea;
        AppWindow.Resize(new SizeInt32(Math.Min(area.Width, (int)(560 * dpi / 96)),
            Math.Min(area.Height, (int)(430 * dpi / 96))));

        var content = new StackPanel { Spacing = 16, Padding = new Thickness(24) };
        var heading = new TextBlock {
            Text = T($"安装 SonyHeadphonesClient {version}", $"Install SonyHeadphonesClient {version}"),
            Style = (Style)Application.Current.Resources["TitleTextBlockStyle"],
            TextWrapping = TextWrapping.Wrap
        };
        content.Children.Add(heading);
        directory.Header = T("安装位置", "Install location");
        directory.Text = Path.TrimEndingDirectorySeparator(Path.GetFullPath(initialDirectory));
        var location = new Grid { ColumnSpacing = 8 };
        location.ColumnDefinitions.Add(new ColumnDefinition());
        location.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        location.Children.Add(directory);
        browse.Content = T("浏览…", "Browse…");
        browse.VerticalAlignment = VerticalAlignment.Bottom;
        browse.Click += async (_, _) => await BrowseAsync(hwnd);
        Grid.SetColumn(browse, 1);
        location.Children.Add(browse);
        content.Children.Add(location);
        desktop.Content = T("创建桌面快捷方式", "Create a desktop shortcut");
        content.Children.Add(desktop);
        AutomationProperties.SetName(progress, T("正在安装", "Installing"));
        content.Children.Add(progress);
        content.Children.Add(result);

        var actions = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8,
            HorizontalAlignment = HorizontalAlignment.Right };
        install.Content = T("安装", "Install");
        install.Style = (Style)Application.Current.Resources["AccentButtonStyle"];
        install.Click += async (_, _) => await InstallAsync();
        close.Content = T("取消", "Cancel");
        close.Click += (_, _) => Close();
        actions.Children.Add(install);
        actions.Children.Add(close);
        content.Children.Add(actions);
        Content = new ScrollViewer { Content = content, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };

        AppWindow.Closing += (_, args) => args.Cancel = installing;
        Closed += (_, _) => Environment.ExitCode = installed ? 0 : 1;
    }

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);

    private async Task BrowseAsync(IntPtr hwnd)
    {
        try {
            var picker = new FolderPicker();
            WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);
            picker.FileTypeFilter.Add("*");
            var folder = await picker.PickSingleFolderAsync();
            if (folder is not null) directory.Text = folder.Path;
        } catch (Exception error) {
            ShowResult(InfoBarSeverity.Error, error.Message);
        }
    }

    private async Task InstallAsync()
    {
        if (installing || installed) return;
        try {
            string path = directory.Text.Trim();
            if (!Path.IsPathFullyQualified(path) || path.IndexOfAny(Path.GetInvalidPathChars()) >= 0 ||
                path.Contains('*') || path.Contains('?')) {
                throw new ArgumentException(T("请输入完整的安装文件夹路径。", "Enter a full installation folder path."));
            }
            path = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
            if (path == Path.GetPathRoot(path)) {
                throw new ArgumentException(T("请选择驱动器下的文件夹。", "Choose a folder on the drive."));
            }

            installing = true;
            directory.IsEnabled = browse.IsEnabled = desktop.IsEnabled = install.IsEnabled = close.IsEnabled = false;
            progress.Visibility = Visibility.Visible;
            ShowResult(InfoBarSeverity.Informational, T("正在安装 SonyHeadphonesClient…", "Installing SonyHeadphonesClient…"));
            // NSIS consumes the entire tail after /D= as an unquoted directory, including spaces.
            var start = new ProcessStartInfo(installer) {
                UseShellExecute = false,
                Arguments = "/S " + (desktop.IsChecked == true ? "/DESKTOP " : "") + "/D=" + path
            };
            using var process = Process.Start(start) ?? throw new Win32Exception();
            await process.WaitForExitAsync();
            if (process.ExitCode != 0) {
                string message = process.ExitCode switch {
                    3 => T("请退出正在运行的 SonyHeadphonesClient 后重试。", "Quit SonyHeadphonesClient and try again."),
                    4 => T("另一个安装正在进行。请稍后重试。", "Another installation is in progress. Try again when it finishes."),
                    5 => T("此文件夹中已有其他卸载程序。请选择另一个文件夹。", "This folder contains another uninstaller. Choose a different folder."),
                    _ => T($"安装失败（代码 {process.ExitCode}）。请检查安装位置后重试。",
                           $"Installation failed (code {process.ExitCode}). Check the install location and try again.")
                };
                ShowResult(InfoBarSeverity.Error, message);
                return;
            }
            installed = true;
            ShowResult(InfoBarSeverity.Success, T("安装完成。", "Installation complete."));
            install.Visibility = Visibility.Collapsed;
            close.Content = T("完成", "Finish");
        } catch (Exception error) {
            ShowResult(InfoBarSeverity.Error, error.Message);
        } finally {
            installing = false;
            progress.Visibility = Visibility.Collapsed;
            directory.IsEnabled = browse.IsEnabled = desktop.IsEnabled = install.IsEnabled = !installed;
            close.IsEnabled = true;
            if (installed) close.Focus(FocusState.Programmatic);
        }
    }

    private void ShowResult(InfoBarSeverity severity, string message)
    {
        result.Severity = severity;
        result.Message = message;
        result.IsOpen = true;
    }
}
