using System;
using System.Runtime.InteropServices;
using Forms = System.Windows.Forms;

namespace SoundConnect.WinUI;

internal sealed class WindowSizing : Forms.NativeWindow, IDisposable
{
    private readonly bool anchored;
    internal WindowSizing(IntPtr hwnd, bool anchored)
    {
        this.anchored = anchored;
        AssignHandle(hwnd);
    }

    protected override void WndProc(ref Forms.Message message)
    {
        // Keep the tray window anchored while retaining the system caption and Close button.
        if (anchored && message.Msg == 0x0112 && (message.WParam.ToInt64() & 0xfff0) == 0xf010) return; // SC_MOVE
        base.WndProc(ref message);
        if (anchored || message.Msg != 0x0024) return; // WM_GETMINMAXINFO for the full window
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
