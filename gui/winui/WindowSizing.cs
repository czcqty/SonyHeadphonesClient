using System;
using System.Runtime.InteropServices;
using Forms = System.Windows.Forms;

namespace SoundConnect.WinUI;

// Let Windows enforce an interactive minimum in physical pixels at the current
// monitor DPI. NavigationView still owns the adaptive navigation and styling.
internal sealed class WindowSizing : Forms.NativeWindow, IDisposable
{
    internal WindowSizing(IntPtr hwnd) => AssignHandle(hwnd);

    protected override void WndProc(ref Forms.Message message)
    {
        base.WndProc(ref message);
        if (message.Msg != 0x0024) return; // WM_GETMINMAXINFO
        var limits = Marshal.PtrToStructure<MinMaxInfo>(message.LParam);
        double scale = GetDpiForWindow(Handle) / 96.0;
        var area = Forms.Screen.FromHandle(Handle).WorkingArea;
        limits.MinTrackSize.X = Math.Min(area.Width, (int)Math.Ceiling(480 * scale));
        limits.MinTrackSize.Y = Math.Min(area.Height, (int)Math.Ceiling(400 * scale));
        Marshal.StructureToPtr(limits, message.LParam, false);
    }

    public void Dispose() => ReleaseHandle();
    [StructLayout(LayoutKind.Sequential)] private struct Point { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] private struct MinMaxInfo
    {
        public Point Reserved, MaxSize, MaxPosition, MinTrackSize, MaxTrackSize;
    }
    [DllImport("user32.dll")] private static extern uint GetDpiForWindow(IntPtr hwnd);
}
