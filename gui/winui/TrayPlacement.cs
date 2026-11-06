using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using Windows.Graphics;

namespace SoundConnect.WinUI;

internal static class TrayPlacement
{
    // Explorer owns the notification icons on the system taskbar. An icon in
    // the overflow flyout belongs to this same tray.
    private static IntPtr Taskbar => FindWindow("Shell_TrayWnd", null);
    public static bool Available => Taskbar != IntPtr.Zero;

    public static RectInt32 PanelBounds(IntPtr panel)
    {
        var taskbar = Taskbar;
        var screen = taskbar == IntPtr.Zero ? Screen.PrimaryScreen! : Screen.FromHandle(taskbar);
        Rectangle monitor = screen.Bounds, work = screen.WorkingArea;
        bool right = true, bottom = true;
        if (taskbar != IntPtr.Zero) {
            var tray = FindWindowEx(taskbar, IntPtr.Zero, "TrayNotifyWnd", null);
            bool foundTray = GetWindowRect(tray, out var anchor) && anchor.Right > anchor.Left && anchor.Bottom > anchor.Top;
            if (foundTray || GetWindowRect(taskbar, out anchor)) {
                bool horizontal = anchor.Right - anchor.Left >= anchor.Bottom - anchor.Top;
                right = (!foundTray && horizontal) || anchor.Left + anchor.Right >= monitor.Left + monitor.Right;
                bottom = (!foundTray && !horizontal) || anchor.Top + anchor.Bottom >= monitor.Top + monitor.Bottom;
            }
        }
        uint dpi = GetDpiForWindow(taskbar == IntPtr.Zero ? panel : taskbar);
        double scale = dpi == 0 ? 1 : dpi / 96.0;
        int width = Math.Min(work.Width, (int)Math.Round(440 * scale));
        int height = Math.Min(work.Height, (int)Math.Round(560 * scale));
        int margin = (int)Math.Round(8 * scale);
        int gapX = Math.Min(margin, work.Width - width);
        int gapY = Math.Min(margin, work.Height - height);
        return new RectInt32(right ? work.Right - width - gapX : work.Left + gapX,
            bottom ? work.Bottom - height - gapY : work.Top + gapY, width, height);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr FindWindow(string className, string? title);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string className, string? title);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] private static extern uint GetDpiForWindow(IntPtr window);
}
