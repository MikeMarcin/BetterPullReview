# Native Windows desktop course for the Alt chords of [[BPR-1022]], pressed at a person's pace.
#
# Something on a desktop can take the Alt out of a chord before any application
# sees it. On the Windows station a low-level keyboard hook replaced the Alt of
# every back-to-back injected chord with a key of virtual-key code 0
# ([[BPR-1109]]). So this course never judges the application on a chord it
# cannot show arrived. It presses each chord as a person does: Alt down, a gap,
# the key down, the key up, Alt up. It records the keyboard input as raw input
# delivers it, after every low-level hook has run. Only synthetic input (input
# with no device handle, as keybd_event makes) is recorded, so none of the
# owner's typing is. A chord counts only when exactly its four keys arrived: the
# Alt as VK_MENU with WM_SYSKEYDOWN, the key as WM_SYSKEYDOWN and WM_SYSKEYUP,
# then the Alt's release, and no key with virtual-key code 0. Otherwise the run
# stops as "environment: Alt rewritten" with exit code 4, and the application's
# behaviour is not judged.
#
# Runs in the interactive session against an already-built application and an
# existing owner-private data root the caller created
# (tests/end_to_end/bpr_narrow_owner_private.py makes one). Keyboard only; the
# pointer never moves. A WSLg (msrdc) window in front stops the run before it
# starts. Before every key press the course checks by process id that the
# application's own window holds the foreground. If it does not, nothing more is
# pressed, every pressed key is released, and the run stops with exit code 3.
#
#   sweep   Alt with the unassigned virtual key 0x97, which no application acts
#           on, Repeats times at each gap in SweepGaps, interleaved. Records how
#           many chords arrived intact at each gap. Judges nothing.
#   chords  At GapMilliseconds: Alt+Space opens the system menu and Esc closes
#           it; Alt+F opens nothing and the keyboard stays where it was; Alt+Up
#           opens nothing; Alt+F4 closes the window.
#
# QtEventLog names a file for the application's qt.qpa.events log, which shows
# the modifiers Qt's key mapper gave each key; it is recorded, not judged.
#
# Exit codes: 0 every check passed, or the sweep completed; 1 a check of the
# application failed; 2 the course itself failed; 3 not run, because the
# application's window was not in front; 4 environment: an Alt was rewritten
# before the application saw it.
param(
    [Parameter(Mandatory)] [string]$Executable,
    [Parameter(Mandatory)] [string]$DataRoot,
    [Parameter(Mandatory)] [string]$Output,
    [Parameter(Mandatory)] [string]$Revision,
    [string]$QtBin = '',
    [ValidateSet('sweep', 'chords')] [string]$Mode = 'chords',
    [int]$GapMilliseconds = 150,
    [int[]]$SweepGaps = @(50, 100, 150, 250),
    [int]$Repeats = 5,
    [int]$HoldMilliseconds = 60,
    [string]$QtEventLog = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, UIAutomationClient, UIAutomationTypes
Add-Type -ReferencedAssemblies System.Windows.Forms -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
namespace BprAltChordCourse {
public static class Clock {
    static readonly Stopwatch Watch = Stopwatch.StartNew();
    public static double Now { get { return Watch.Elapsed.TotalMilliseconds; } }
    // Waits to within a fraction of a millisecond: sleeps while far off, then spins.
    public static void Wait(double ms) {
        double until = Now + ms;
        while (true) {
            double left = until - Now;
            if (left <= 0) return;
            if (left > 3) Thread.Sleep(1); else Thread.SpinWait(64);
        }
    }
}
public sealed class RawKey {
    public double At; public ushort VKey, MakeCode, Flags; public uint Message;
    public bool Up { get { return (Flags & 1) != 0; } }
    public string Describe(double origin) {
        return string.Format("+{0:F1}ms vk=0x{1:X2} make=0x{2:X2} {3}{4} msg=0x{5:X}", At - origin, VKey, MakeCode, Up ? "up" : "down", (Flags & 2) != 0 ? " E0" : "", Message);
    }
}
// Keyboard input as raw input delivers it, after every low-level hook has run, read on a thread of
// its own. Only synthetic input (no device handle) is kept. The window is never shown.
public sealed class RawSink : NativeWindow {
    [StructLayout(LayoutKind.Sequential)] struct RAWINPUTDEVICE { public ushort UsagePage, Usage; public uint Flags; public IntPtr Target; }
    [StructLayout(LayoutKind.Sequential)] struct RAWINPUTHEADER { public uint Type, Size; public IntPtr Device, WParam; }
    [StructLayout(LayoutKind.Sequential)] struct RAWKEYBOARD { public ushort MakeCode, Flags, Reserved, VKey; public uint Message, ExtraInformation; }
    [StructLayout(LayoutKind.Sequential)] struct MSG { public IntPtr Hwnd; public uint Message; public IntPtr WParam, LParam; public uint Time; public int X, Y; }
    [DllImport("user32.dll", SetLastError = true)] static extern bool RegisterRawInputDevices(RAWINPUTDEVICE[] d, uint n, uint size);
    [DllImport("user32.dll")] static extern uint GetRawInputData(IntPtr h, uint cmd, IntPtr data, ref uint size, uint header);
    [DllImport("user32.dll")] static extern int GetMessageW(out MSG m, IntPtr h, uint min, uint max);
    [DllImport("user32.dll")] static extern IntPtr DispatchMessageW(ref MSG m);
    [DllImport("user32.dll")] static extern bool PostThreadMessageW(uint tid, uint m, IntPtr w, IntPtr l);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    readonly List<RawKey> keys = new List<RawKey>();
    readonly ManualResetEvent ready = new ManualResetEvent(false);
    Thread thread; uint threadId; string failure;

    public static RawSink Start() {
        var sink = new RawSink();
        sink.thread = new Thread(sink.Run);
        sink.thread.IsBackground = true;
        sink.thread.Start();
        if (!sink.ready.WaitOne(10000)) throw new Exception("the raw input sink did not start");
        if (sink.failure != null) throw new Exception("the raw input sink failed: " + sink.failure);
        return sink;
    }
    void Run() {
        try {
            threadId = GetCurrentThreadId();
            CreateHandle(new CreateParams());
            Register(0x100, Handle);  // RIDEV_INPUTSINK: also while another window is in front
        } catch (Exception e) { failure = e.Message; ready.Set(); return; }
        ready.Set();
        MSG m;
        while (GetMessageW(out m, IntPtr.Zero, 0, 0) > 0) DispatchMessageW(ref m);
        try { Register(0x1, IntPtr.Zero); } catch (Exception) { }  // RIDEV_REMOVE
        DestroyHandle();
    }
    static void Register(uint flags, IntPtr target) {
        var d = new RAWINPUTDEVICE[] { new RAWINPUTDEVICE { UsagePage = 1, Usage = 6, Flags = flags, Target = target } };
        if (!RegisterRawInputDevices(d, 1, (uint)Marshal.SizeOf(typeof(RAWINPUTDEVICE)))) throw new Exception("RegisterRawInputDevices " + Marshal.GetLastWin32Error());
    }
    public bool Stop() { PostThreadMessageW(threadId, 0x12, IntPtr.Zero, IntPtr.Zero); return thread.Join(5000); }
    public int Count { get { lock (keys) return keys.Count; } }
    public List<RawKey> Since(int index) { lock (keys) return keys.GetRange(index, keys.Count - index); }
    protected override void WndProc(ref Message m) {
        if (m.Msg == 0x00FF) {
            double at = Clock.Now;
            uint size = 0, header = (uint)Marshal.SizeOf(typeof(RAWINPUTHEADER));
            GetRawInputData(m.LParam, 0x10000003, IntPtr.Zero, ref size, header);
            IntPtr buffer = Marshal.AllocHGlobal((int)size);
            try {
                if (GetRawInputData(m.LParam, 0x10000003, buffer, ref size, header) == size) {
                    var h = (RAWINPUTHEADER)Marshal.PtrToStructure(buffer, typeof(RAWINPUTHEADER));
                    if (h.Type == 1 && h.Device == IntPtr.Zero) {
                        var k = (RAWKEYBOARD)Marshal.PtrToStructure(new IntPtr(buffer.ToInt64() + header), typeof(RAWKEYBOARD));
                        lock (keys) keys.Add(new RawKey { At = at, VKey = k.VKey, MakeCode = k.MakeCode, Flags = k.Flags, Message = k.Message });
                    }
                }
            } finally { Marshal.FreeHGlobal(buffer); }
        }
        base.WndProc(ref m);
    }
}
public static class Desk {
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] struct GUITHREADINFO { public uint cbSize, flags; public IntPtr hwndActive, hwndFocus, hwndCapture, hwndMenuOwner, hwndMoveSize, hwndCaret; public RECT rcCaret; }
    delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr h, uint cmd);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
    [DllImport("user32.dll")] static extern bool GetGUIThreadInfo(uint tid, ref GUITHREADINFO g);
    [DllImport("user32.dll")] static extern IntPtr GetKeyboardLayout(uint tid);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);

    public static string Text(IntPtr h) { var s = new StringBuilder(512); GetWindowTextW(h, s, 512); return s.ToString(); }
    public static string Class(IntPtr h) { var s = new StringBuilder(256); GetClassNameW(h, s, 256); return s.ToString(); }
    public static uint Pid(IntPtr h) { uint p; GetWindowThreadProcessId(h, out p); return p; }
    static uint Thread(IntPtr h) { uint p; return GetWindowThreadProcessId(h, out p); }
    public static string Name(uint pid) { try { return Process.GetProcessById((int)pid).ProcessName; } catch (Exception) { return "?"; } }
    public static string Describe(IntPtr h) { uint p = Pid(h); return string.Format("0x{0:X}|pid={1}|{2}|{3}", h.ToInt64(), p, Name(p), Class(h)); }
    public static string Foreground() { return Describe(GetForegroundWindow()); }
    public static string Layout(IntPtr h) { return string.Format("0x{0:X}", GetKeyboardLayout(Thread(h)).ToInt64()); }
    // The application's window: its largest visible, unowned, titled top-level window.
    public static IntPtr MainWindow(uint pid) {
        IntPtr found = IntPtr.Zero; long area = 0;
        EnumWindows((h, l) => {
            if (Pid(h) == pid && IsWindowVisible(h) && GetWindow(h, 4) == IntPtr.Zero && Text(h).Length > 0) {
                RECT r; GetWindowRect(h, out r); long a = (long)(r.R - r.L) * (r.B - r.T);
                if (a > area) { area = a; found = h; }
            }
            return true; }, IntPtr.Zero);
        return found;
    }
    // Every visible top-level window of the process except the main one, as hwnd|class|title.
    public static List<string> Popups(uint pid, IntPtr main) {
        var r = new List<string>();
        EnumWindows((h, l) => { if (h != main && Pid(h) == pid && IsWindowVisible(h)) r.Add(string.Format("0x{0:X}|{1}|{2}", h.ToInt64(), Class(h), Text(h))); return true; }, IntPtr.Zero);
        return r;
    }
    // The GUI thread's GUI_* flags; 0x4 is GUI_INMENUMODE.
    public static uint MenuFlags(IntPtr hwnd) {
        var g = new GUITHREADINFO(); g.cbSize = (uint)Marshal.SizeOf(g);
        return GetGUIThreadInfo(Thread(hwnd), ref g) ? g.flags : 0xFFFFFFFF;
    }
    // Puts hwnd in front by sharing the current foreground thread's input state for the call.
    public static bool Front(IntPtr hwnd) {
        IntPtr fg = GetForegroundWindow();
        if (fg == hwnd) return true;
        uint me = GetCurrentThreadId();
        uint fgt = fg == IntPtr.Zero ? 0 : Thread(fg);
        uint tt = Thread(hwnd);
        bool a1 = fgt != 0 && fgt != me && AttachThreadInput(me, fgt, true);
        bool a2 = tt != 0 && tt != me && tt != fgt && AttachThreadInput(me, tt, true);
        BringWindowToTop(hwnd); SetForegroundWindow(hwnd);
        if (a2) AttachThreadInput(me, tt, false);
        if (a1) AttachThreadInput(me, fgt, false);
        return GetForegroundWindow() == hwnd;
    }
}
// Key presses that go out only while the application holds the foreground.
public static class Keys {
    [DllImport("user32.dll")] static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("user32.dll")] static extern uint MapVirtualKeyW(uint code, uint type);
    public const byte Alt = 0x12;
    // Arrow keys are extended keys.
    static uint Extended(byte vk) { return vk >= 0x21 && vk <= 0x28 ? 1u : 0u; }
    static void Send(byte vk, bool up) { keybd_event(vk, (byte)MapVirtualKeyW(vk, 0), Extended(vk) | (up ? 2u : 0u), UIntPtr.Zero); }
    static bool Ours(uint pid, ref string lost) {
        IntPtr fg = Desk.GetForegroundWindow();
        if (Desk.Pid(fg) == pid) return true;
        lost = Desk.Describe(fg);
        return false;
    }
    // Presses down[0], waits firstGap, presses the rest hold apart, then releases them in reverse
    // order, hold apart. Each press goes out only while pid holds the foreground; otherwise nothing
    // more is pressed, every pressed key is released and lost names the foreground. Returns the
    // injection time of every event sent.
    public static List<double> Sequence(uint pid, byte[] down, double firstGap, double hold, out string lost) {
        var times = new List<double>();
        var pressed = new Stack<byte>();
        string foreground = null;
        try {
            for (int i = 0; i < down.Length; i++) {
                if (i > 0) Clock.Wait(i == 1 ? firstGap : hold);
                if (!Ours(pid, ref foreground)) break;
                Send(down[i], false); pressed.Push(down[i]); times.Add(Clock.Now);
            }
            while (foreground == null && pressed.Count > 0) {
                Clock.Wait(hold);
                Send(pressed.Pop(), true); times.Add(Clock.Now);
            }
        } finally {
            while (pressed.Count > 0) Send(pressed.Pop(), true);
        }
        lost = foreground;
        return times;
    }
    // Why an Alt chord did not arrive as one, or null when it did: exactly the Alt as VK_MENU with
    // WM_SYSKEYDOWN, the key as WM_SYSKEYDOWN and WM_SYSKEYUP, then the Alt's release, and no key
    // with virtual-key code 0.
    public static string ChordRewritten(List<RawKey> got, byte key) {
        foreach (var k in got) if (k.VKey == 0) return "a key with virtual-key code 0 (vk=0x00) arrived in the chord";
        if (got.Count != 4) return string.Format("{0} keys arrived, not the chord's 4", got.Count);
        if (got[0].VKey != Alt || got[0].Up || got[0].Message != 0x104) return "the chord did not begin with VK_MENU as WM_SYSKEYDOWN";
        if (got[1].VKey != key || got[1].Up || got[1].Message != 0x104) return "the key did not arrive as WM_SYSKEYDOWN, so no Alt was held for it";
        if (got[2].VKey != key || !got[2].Up || got[2].Message != 0x105) return "the key's release did not arrive as WM_SYSKEYUP";
        if (got[3].VKey != Alt || !got[3].Up) return "the Alt's release did not arrive last";
        return null;
    }
}
public sealed class Launched {
    public Process P; readonly StringBuilder o = new StringBuilder(), e = new StringBuilder();
    public string Output { get { lock (o) return o.ToString(); } }
    public string Errors { get { lock (e) return e.ToString(); } }
    public static Launched Start(ProcessStartInfo si) {
        var l = new Launched();
        si.UseShellExecute = false; si.RedirectStandardOutput = true; si.RedirectStandardError = true;
        l.P = new Process(); l.P.StartInfo = si;
        l.P.OutputDataReceived += (s, a) => { if (a.Data != null) lock (l.o) l.o.AppendLine(a.Data); };
        l.P.ErrorDataReceived += (s, a) => { if (a.Data != null) lock (l.e) l.e.AppendLine(a.Data); };
        l.P.Start(); l.P.BeginOutputReadLine(); l.P.BeginErrorReadLine();
        return l;
    }
}
}
'@

$desk = [BprAltChordCourse.Desk]
$keys = [BprAltChordCourse.Keys]
$ae = [Windows.Automation.AutomationElement]
$VK = @{ Esc = 0x1B; Space = 0x20; F = 0x46; Up = 0x26; F4 = 0x73; Unassigned = 0x97 }
$record = [ordered]@{
    schema = 'bpr.windows-alt-chord-course.v1'; mode = $Mode; revision = $Revision
    executable = $Executable; executable_sha256 = (Get-FileHash $Executable -Algorithm SHA256).Hash.ToLower()
    course_sha256 = (Get-FileHash $PSCommandPath -Algorithm SHA256).Hash.ToLower()
    host = [ordered]@{ computer = $env:COMPUTERNAME; os = [Environment]::OSVersion.VersionString; session = [Diagnostics.Process]::GetCurrentProcess().SessionId }
    data_root = $DataRoot; gap_ms = $GapMilliseconds; hold_ms = $HoldMilliseconds; sweep_gaps_ms = $SweepGaps; repeats = $Repeats
    started_utc = (Get-Date).ToUniversalTime().ToString('o'); foreground_before = $desk::Foreground()
    gestures = @(); checks = [ordered]@{}; errors = @()
}
$app = $null
$sink = $null
$script:main = [IntPtr]::Zero
$script:failed = $false

function Stop-Run([string]$kind, [string]$message) { throw "${kind}: $message" }
function Focused { try { $f = $ae::FocusedElement; if ($null -eq $f) { 'none' } else { '{0}|{1}|pid={2}' -f $f.Current.ControlType.ProgrammaticName, $f.Current.Name, $f.Current.ProcessId } } catch { "error: $($_.Exception.Message)" } }
function State {
    $flags = $desk::MenuFlags($script:main)
    [ordered]@{ utc = (Get-Date).ToUniversalTime().ToString('HH:mm:ss.fff'); menu_flags = ('0x{0:X}' -f $flags); menu_mode = (($flags -band 0x4) -ne 0)
        popups = @($desk::Popups([uint32]$app.P.Id, $script:main)); focused = (Focused)
        foreground_ours = ($desk::Pid($desk::GetForegroundWindow()) -eq [uint32]$app.P.Id); exited = $app.P.HasExited }
}
function Guard([string]$name) {
    if (-not $desk::IsWindow($script:main)) { Stop-Run 'harness error' "the application's window is gone before $name" }
    $now = $desk::MainWindow([uint32]$app.P.Id)
    if ($now -ne $script:main) { Stop-Run 'harness error' ("the application's window changed before {0}: 0x{1:X}" -f $name, $now.ToInt64()) }
    if ($desk::Pid($desk::GetForegroundWindow()) -ne [uint32]$app.P.Id) { Stop-Run 'not run' "the application's window is not in front before $name; the foreground is $($desk::Foreground())" }
}
# One gesture: the guard, the keys, a pause, then the raw input it produced and the state after it.
# A chord's raw input is asserted before anything else is read from it.
function Gesture([string]$name, [byte[]]$down, [int]$gap, [bool]$chord, [int]$settle = 700) {
    Guard $name
    $first = $sink.Count
    $lost = $null
    $times = $keys::Sequence([uint32]$app.P.Id, $down, $gap, $HoldMilliseconds, [ref]$lost)
    Start-Sleep -Milliseconds $settle
    $raw = $sink.Since($first)
    $origin = if ($times.Count -gt 0) { $times[0] } else { 0 }
    $g = [ordered]@{ step = $name; gap_ms = $gap; injected = @($times | ForEach-Object { '+{0:F1}ms' -f ($_ - $origin) })
        raw = @($raw | ForEach-Object { $_.Describe($origin) }) }
    if ($chord -and $raw.Count -ge 2) { $g.raw_alt_to_key_ms = [math]::Round($raw[1].At - $raw[0].At, 1); $g.raw_alt_after_injection_ms = [math]::Round($raw[0].At - $origin, 1) }
    if ($lost) { $g.foreground_lost = $lost; $script:record.gestures += $g; Stop-Run 'not run' "the foreground left the application during $name; it is $lost" }
    if ($chord) {
        $why = $keys::ChordRewritten($raw, $down[1])
        $g.alt_intact = ($null -eq $why)
        $g.raw_verdict = if ($null -eq $why) { 'intact: VK_MENU arrived as WM_SYSKEYDOWN, the key as WM_SYSKEYDOWN, no vk=0x00' } else { "rewritten: $why" }
    }
    $g.state = if ($app.P.HasExited) { [ordered]@{ exited = $true } } else { State }
    $script:record.gestures += $g
    $after = if ($g.state.exited) { 'exited' } else { 'popups={0} menu_mode={1}' -f @($g.state.popups).Count, $g.state.menu_mode }
    Write-Host ('{0}: {1} | raw {2} | {3}' -f $name, $g.raw_verdict, ($g.raw -join ' ; '), $after)
    $g
}
# A chord whose Alt did not arrive stops the run: the application never saw the chord.
function Chord([string]$name, [byte]$key) {
    $g = Gesture $name @($keys::Alt, $key) $GapMilliseconds $true
    if (-not $g.alt_intact) { Stop-Run 'environment: Alt rewritten' "$name; $($g.raw_verdict); raw $($g.raw -join ' ; ')" }
    $g
}
function Check([string]$name, [bool]$ok) {
    $script:record.checks[$name] = $ok
    if (-not $ok) { $script:failed = $true }
    Write-Host ('CHECK {0}: {1}' -f $name, $(if ($ok) { 'pass' } else { 'FAIL' }))
}

$exitCode = 0
try {
    if ($record.foreground_before -match '\|msrdc\|') { Stop-Run 'not run' "a WSLg window holds the foreground: $($record.foreground_before)" }
    if (@(Get-Process better_pull_review -ErrorAction SilentlyContinue).Count -ne 0) { Stop-Run 'harness error' 'a better_pull_review.exe is already running' }
    if (-not (Test-Path $DataRoot)) { Stop-Run 'harness error' "data root $DataRoot is absent" }
    $sink = [BprAltChordCourse.RawSink]::Start()
    $si = New-Object Diagnostics.ProcessStartInfo
    $si.FileName = $Executable
    $si.Arguments = (@('--test-application-data-root', $DataRoot) | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
    $si.WorkingDirectory = Split-Path $Executable
    if ($QtBin) { $si.EnvironmentVariables['PATH'] = $QtBin + ';' + $env:PATH; $si.EnvironmentVariables['QT_QPA_FONTDIR'] = "$env:WINDIR\Fonts" }
    if ($si.EnvironmentVariables.ContainsKey('QT_QPA_PLATFORM')) { $si.EnvironmentVariables.Remove('QT_QPA_PLATFORM') }
    if ($QtEventLog) { $si.EnvironmentVariables['QT_LOGGING_RULES'] = 'qt.qpa.events=true'; $si.EnvironmentVariables['QT_FORCE_STDERR_LOGGING'] = '1' }
    $app = [BprAltChordCourse.Launched]::Start($si)
    $record.pid = $app.P.Id

    $until = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $until -and $script:main -eq [IntPtr]::Zero -and -not $app.P.HasExited) { Start-Sleep -Milliseconds 250; $script:main = $desk::MainWindow([uint32]$app.P.Id) }
    if ($script:main -eq [IntPtr]::Zero) { Stop-Run 'harness error' 'the application showed no window' }
    # A drawer's short-lived window can come first ([[BPR-1074]]); take the window that remains.
    Start-Sleep -Seconds 3
    $settled = $desk::MainWindow([uint32]$app.P.Id)
    if ($settled -ne $script:main) { $record.errors += ('first window 0x{0:X} replaced by 0x{1:X}' -f $script:main.ToInt64(), $settled.ToInt64()); $script:main = $settled }
    $record.main = '0x{0:X}|{1}' -f $script:main.ToInt64(), $desk::Text($script:main)
    $record.keyboard_layout = $desk::Layout($script:main)
    if ($desk::Foreground() -match '\|msrdc\|') { Stop-Run 'not run' "a WSLg window took the foreground: $($desk::Foreground())" }
    $taken = $false
    for ($i = 0; $i -lt 8 -and -not $taken; $i++) { $taken = $desk::Front($script:main); if (-not $taken) { Start-Sleep -Milliseconds 750 } }
    $record.foreground_attempts = $i
    if (-not $taken) { Stop-Run 'not run' "could not bring the application's window in front; the foreground is $($desk::Foreground())" }
    Start-Sleep -Milliseconds 500
    $start = State
    $record.start = $start

    if ($Mode -eq 'sweep') {
        $tally = [ordered]@{}
        foreach ($gap in $SweepGaps) { $tally["$gap"] = [ordered]@{ gap_ms = $gap; intact = 0; rewritten = 0; raw_alt_to_key_ms = @() } }
        foreach ($round in 1..$Repeats) {
            foreach ($gap in $SweepGaps) {
                $g = Gesture "sweep-$gap-ms-$round" @($keys::Alt, [byte]$VK.Unassigned) $gap $true 500
                if ($g.alt_intact) { $tally["$gap"].intact++ } else { $tally["$gap"].rewritten++ }
                if ($g.Contains('raw_alt_to_key_ms')) { $tally["$gap"].raw_alt_to_key_ms += $g.raw_alt_to_key_ms }
            }
        }
        $record.sweep = @($tally.Values)
        $clean = @($record.sweep | Where-Object { $_.intact -eq $Repeats } | ForEach-Object { $_.gap_ms })
        $record.lowest_clean_gap_ms = if ($clean.Count -gt 0) { ($clean | Measure-Object -Minimum).Minimum } else { $null }
    } else {
        $g = Chord 'alt-space' $VK.Space
        Check 'Alt+Space opens the system menu' ((@($g.state.popups | Where-Object { $_ -like '*|#32768|*' })).Count -eq 1 -and $g.state.menu_mode)
        $g = Gesture 'alt-space-esc' @([byte]$VK.Esc) 0 $false
        Check 'Esc closes the system menu' (@($g.state.popups).Count -eq 0 -and -not $g.state.menu_mode)
        $g = Chord 'alt-f' $VK.F
        Check 'Alt+F opens nothing and the keyboard stays where it was' (@($g.state.popups).Count -eq 0 -and -not $g.state.menu_mode -and $g.state.focused -eq $start.focused)
        $g = Chord 'alt-up' $VK.Up
        Check 'Alt+Up opens nothing' (@($g.state.popups).Count -eq 0 -and -not $g.state.menu_mode)
        $g = Chord 'alt-f4' $VK.F4
        $closed = $app.P.WaitForExit(15000)
        Check 'Alt+F4 closes the window' $closed
        if ($closed) { $app.P.WaitForExit(); Check 'the application exits 0 after Alt+F4' ($app.P.ExitCode -eq 0) }
    }
    if ($script:failed) { $exitCode = 1 }
    $record.outcome = if ($Mode -eq 'sweep') { 'sweep complete' } elseif ($script:failed) { 'fail' } else { 'pass' }
} catch {
    $message = $_.Exception.Message
    $record.errors += $message
    $record.outcome = if ($message -like 'environment: Alt rewritten:*') { 'environment: Alt rewritten' } elseif ($message -like 'not run:*') { 'not run' } else { 'harness error' }
    $exitCode = switch ($record.outcome) { 'environment: Alt rewritten' { 4 } 'not run' { 3 } default { 2 } }
    Write-Host "$($record.outcome.ToUpper()): $message"
} finally {
    if ($app -and -not $app.P.HasExited) {
        if ($desk::IsWindow($script:main)) { [void]$desk::PostMessageW($script:main, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) }
        if (-not $app.P.WaitForExit(15000)) { $record.errors += "stopped pid $($app.P.Id) after WM_CLOSE timed out"; $app.P.Kill(); [void]$app.P.WaitForExit(10000) }
    }
    if ($app -and $app.P.HasExited) { $app.P.WaitForExit(); $record.exit_code = $app.P.ExitCode }
    if ($sink) { $record.raw_sink_stopped = $sink.Stop() }
    if ($app -and $QtEventLog) {
        $app.Errors | Set-Content -Encoding UTF8 $QtEventLog
        # Which modifiers Qt's key mapper gave each key it offered to the shortcut map.
        $record.qt_shortcut_override = @($app.Errors -split "`r?`n" | ForEach-Object { if ($_ -match 'possibleKeyCombinations QKeyEvent\(ShortcutOverride, (Key_\w+)[^)]*\).*QFlags<Qt::KeyboardModifier>\(([^)]*)\)') { '{0} {1}' -f $Matches[1], $Matches[2] } })
    }
    $record.foreground_after = $desk::Foreground()
    $record.finished_utc = (Get-Date).ToUniversalTime().ToString('o')
    $record.remaining_bpr_processes = @(Get-Process better_pull_review -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
    $record | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $Output
    Write-Host "OUTCOME=$($record.outcome) EXIT=$exitCode APP_EXIT=$($record.exit_code) REMAINING=$(@($record.remaining_bpr_processes).Count)"
}
exit $exitCode
