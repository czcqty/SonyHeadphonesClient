using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Forms = System.Windows.Forms;

namespace SoundConnect.WinUI;

// A message-only Win32 window receives global keys even while both WinUI
// windows are hidden. Each successful registration is released on shutdown.
internal sealed class Hotkeys : Forms.NativeWindow, IDisposable
{
    private readonly Action<int> action;
    private readonly HashSet<int> registered = new();
    public string Error { get; private set; } = "";
    public static bool TrayAvailable => FindWindow("Shell_TrayWnd", null) != IntPtr.Zero;
    public Hotkeys(Action<int> action)
    {
        this.action = action;
        CreateHandle(new Forms.CreateParams { Caption = "SoundConnectHotkeys", Parent = new IntPtr(-3) });
        uint[] keys = { 0x50, 0x4e, 0x4d };
        for (int i = 0; i < keys.Length; ++i) {
            if (RegisterHotKey(Handle, i + 1, 0x4003, keys[i])) registered.Add(i + 1);
            else Error += $"Ctrl+Alt+{(char)keys[i]} ({Marshal.GetLastWin32Error()}) ";
        }
    }
    internal void TestDispatch(int id)
    {
        var message = Forms.Message.Create(Handle, 0x0312, new IntPtr(id), IntPtr.Zero);
        WndProc(ref message);
    }
    public bool Registered(int id) => registered.Contains(id);
    protected override void WndProc(ref Forms.Message message)
    {
        if (message.Msg == 0x0312 && registered.Contains(message.WParam.ToInt32())) {
            action(message.WParam.ToInt32()); return;
        }
        base.WndProc(ref message);
    }
    public void Dispose()
    {
        foreach (int id in registered) UnregisterHotKey(Handle, id);
        registered.Clear(); DestroyHandle();
    }
    [DllImport("user32.dll", SetLastError = true)] private static extern bool RegisterHotKey(IntPtr hwnd, int id, uint modifiers, uint key);
    [DllImport("user32.dll")] private static extern bool UnregisterHotKey(IntPtr hwnd, int id);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr FindWindow(string name, string? title);
}
