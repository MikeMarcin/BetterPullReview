# Native Windows capture of the strip as the title row ([[BPR-0653]], criteria
# 2 and 4), for the owner to review remotely.
#
# It launches a tests-disabled image itself, once, bound to its SHA-256 and to
# its resource bundle's, with `--render-evidence` and no value, so the image
# runs signed out on a private temporary profile: no account and no repository
# content can appear. The foreground is taken as the Settings course takes it
# ([[BPR-0980]]): a launcher window of the course's own is put in front and gets
# one F24 press, and the image starts as its child.
#
# It then captures, at the display scale Windows is set to (never changed):
#   rest            the three caption buttons at rest;
#   hover-*         each button hovered;
#   snap-flyout     the snap layouts flyout, from hovering Maximize;
#   pressed-*       each button pressed and held. The pointer then moves off
#                   onto empty strip space before the release. The title row
#                   host acts only on a release over the part that was pressed,
#                   so neither Close nor Minimize can fire;
#   drag-*          empty strip space dragged with real pointer input
#                   (SendInput): before, mid-drag with the button held, after;
#   maximized       after a click on Maximize (a release over Maximize is the
#                   one release on a button this capture makes);
#   restored        after a double-click on empty strip space.
#
# Pointer rules. The pointer position is saved first and restored at the end.
# The pointer moves only inside the image's window. Before every move and every
# press, the image's window must be in front, it must be the window under the
# target point, and the point must answer the expected WM_NCHITTEST code;
# otherwise the capture stops. A button held when anything fails is taken to
# empty strip space and released there. No key is sent after the launcher's F24.
#
# Each capture is the window's visible frame only (DWM's extended frame bounds),
# never the desktop: a full-window PNG, downscaled to at most 1920 pixels wide,
# and the strip and caption-button crops at full resolution. index.json records
# each image's state, rects, DPI, scale, pointer, time and SHA-256.
# -DryRun checks the image, the bundle, the output and the display, and prints
# the plan. It launches nothing and sends no input.
param(
    [Parameter(Mandatory)] [string]$Executable,
    [Parameter(Mandatory)] [string]$ExpectedSha256,
    [Parameter(Mandatory)] [string]$ExpectedBundleSha256,
    [Parameter(Mandatory)] [string]$Revision,
    [Parameter(Mandatory)] [string]$OutputDirectory,
    # A Qt scale override, for a display that is not at the scale under review.
    # Every image is then labelled "Qt override, not native".
    [string]$QtScaleFactor = '',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, System.Drawing, UIAutomationClient, UIAutomationTypes, WindowsBase
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;
namespace BprCaptionCapture {
public sealed class Launched {
    public Process P; readonly StringBuilder o = new StringBuilder(), e = new StringBuilder();
    public string Output { get { lock (o) return o.ToString(); } }
    public string Error { get { lock (e) return e.ToString(); } }
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
public static class Desk {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public MOUSEINPUT mi; }
    [StructLayout(LayoutKind.Sequential)] struct MONITORINFO { public uint cbSize; public RECT rcMonitor, rcWork; public uint dwFlags; }
    [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
    [DllImport("user32.dll")] static extern IntPtr GetThreadDpiAwarenessContext();
    [DllImport("user32.dll")] static extern bool AreDpiAwarenessContextsEqual(IntPtr a, IntPtr b);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern IntPtr SendMessageTimeoutW(IntPtr h, uint m, IntPtr w, IntPtr l, uint flags, uint ms, out IntPtr result);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr h, uint f);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int size);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
    [DllImport("user32.dll")] static extern IntPtr MonitorFromWindow(IntPtr h, uint f);
    [DllImport("user32.dll")] static extern bool GetMonitorInfoW(IntPtr m, ref MONITORINFO i);
    [DllImport("user32.dll")] public static extern uint GetDoubleClickTime();
    delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);

    // Per-monitor aware on this thread, so every rect, point and pixel below is physical.
    public static bool PhysicalCoordinates() {
        SetThreadDpiAwarenessContext(new IntPtr(-4));
        return AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), new IntPtr(-4));
    }
    public static string Text(IntPtr h) { var s = new StringBuilder(1024); GetWindowTextW(h, s, 1024); return s.ToString(); }
    public static string ClassName(IntPtr h) { var s = new StringBuilder(256); GetClassNameW(h, s, 256); return s.ToString(); }
    public static uint Pid(IntPtr h) { uint p; GetWindowThreadProcessId(h, out p); return p; }
    public static bool Front(IntPtr hwnd) {
        IntPtr fg = GetForegroundWindow();
        if (fg == hwnd) return true;
        uint me = GetCurrentThreadId(), p;
        uint fgt = fg == IntPtr.Zero ? 0 : GetWindowThreadProcessId(fg, out p);
        uint tt = GetWindowThreadProcessId(hwnd, out p);
        bool a1 = fgt != 0 && fgt != me && AttachThreadInput(me, fgt, true);
        bool a2 = tt != 0 && tt != me && tt != fgt && AttachThreadInput(me, tt, true);
        BringWindowToTop(hwnd); SetForegroundWindow(hwnd);
        if (a2) AttachThreadInput(me, tt, false);
        if (a1) AttachThreadInput(me, fgt, false);
        return GetForegroundWindow() == hwnd;
    }
    public static bool Nudge(IntPtr hwnd) {
        if (GetForegroundWindow() != hwnd) return false;
        keybd_event(0x87, 0, 0, UIntPtr.Zero); keybd_event(0x87, 0, 2, UIntPtr.Zero);
        return true;
    }
    // The visible frame, without the invisible resize borders.
    public static RECT Frame(IntPtr h) { RECT r; if (DwmGetWindowAttribute(h, 9, out r, Marshal.SizeOf(typeof(RECT))) != 0) GetWindowRect(h, out r); return r; }
    public static RECT Outer(IntPtr h) { RECT r; GetWindowRect(h, out r); return r; }
    public static RECT Work(IntPtr h) { var i = new MONITORINFO(); i.cbSize = (uint)Marshal.SizeOf(i); GetMonitorInfoW(MonitorFromWindow(h, 2), ref i); return i.rcWork; }
    public static RECT Monitor(IntPtr h) { var i = new MONITORINFO(); i.cbSize = (uint)Marshal.SizeOf(i); GetMonitorInfoW(MonitorFromWindow(h, 2), ref i); return i.rcMonitor; }
    // The window's WM_NCHITTEST answer at a screen point, or -1 if it did not answer.
    public static long HitTest(IntPtr h, int x, int y) {
        IntPtr result;
        var lp = new IntPtr(((y & 0xFFFF) << 16) | (x & 0xFFFF));
        return SendMessageTimeoutW(h, 0x84, IntPtr.Zero, lp, 0x2 /* abort if hung */, 2000, out result) == IntPtr.Zero ? -1 : result.ToInt64();
    }
    public static IntPtr RootAt(int x, int y) { var p = new POINT { X = x, Y = y }; return GetAncestor(WindowFromPoint(p), 2); }
    static void Send(uint flags, int x, int y) {
        int vx = GetSystemMetrics(76), vy = GetSystemMetrics(77), vw = GetSystemMetrics(78), vh = GetSystemMetrics(79);
        var i = new INPUT { type = 0 };
        i.mi.dwFlags = flags | 0x8000 /* ABSOLUTE */ | 0x4000 /* VIRTUALDESK */;
        i.mi.dx = (int)Math.Round((x - vx) * 65535.0 / Math.Max(1, vw - 1));
        i.mi.dy = (int)Math.Round((y - vy) * 65535.0 / Math.Max(1, vh - 1));
        if (SendInput(1, new[] { i }, Marshal.SizeOf(typeof(INPUT))) != 1) throw new InvalidOperationException("SendInput refused");
    }
    public static void MoveTo(int x, int y) { Send(0x0001, x, y); }
    public static void Down(int x, int y) { Send(0x0001 | 0x0002, x, y); }
    public static void Up(int x, int y) { Send(0x0001 | 0x0004, x, y); }
    // Visible top-level windows of other processes whose frame meets a rect, by
    // class and rect only: another application's title may be private.
    public static string[] Overlapping(uint exceptPid, RECT area) {
        var found = new System.Collections.Generic.List<string>();
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p != exceptPid && IsWindowVisible(h)) {
                var r = Frame(h);
                if (r.L < area.R && area.L < r.R && r.T < area.B && area.T < r.B && r.R > r.L && r.B > r.T)
                    found.Add(string.Format("0x{0:X} pid={1} class={2} rect={3},{4},{5},{6}", h.ToInt64(), p, ClassName(h), r.L, r.T, r.R, r.B));
            }
            return true;
        }, IntPtr.Zero);
        return found.ToArray();
    }
    public static Bitmap Grab(RECT r) {
        var b = new Bitmap(r.R - r.L, r.B - r.T, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(b)) g.CopyFromScreen(r.L, r.T, 0, 0, new Size(r.R - r.L, r.B - r.T), CopyPixelOperation.SourceCopy);
        return b;
    }
    public static Bitmap Scaled(Bitmap source, double factor) {
        if (factor >= 1.0) return (Bitmap)source.Clone();
        var b = new Bitmap(Math.Max(1, (int)Math.Round(source.Width * factor)), Math.Max(1, (int)Math.Round(source.Height * factor)), PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(b)) { g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic; g.DrawImage(source, 0, 0, b.Width, b.Height); }
        return b;
    }
    public static Bitmap Crop(Bitmap source, int x, int y, int w, int h) {
        x = Math.Max(0, x); y = Math.Max(0, y); w = Math.Min(w, source.Width - x); h = Math.Min(h, source.Height - y);
        return source.Clone(new Rectangle(x, y, w, h), source.PixelFormat);
    }
}
}
'@

$desk = [BprCaptionCapture.Desk]
$ae = [Windows.Automation.AutomationElement]
$scope = [Windows.Automation.TreeScope]
$HT = @{ caption = 2; minimize = 8; maximize = 9; close = 20 }

function Require($condition, [string]$message) { if (-not $condition) { throw $message } }
function Sha256([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower() }
function Hex($h) { '0x{0:X}' -f ([IntPtr]$h).ToInt64() }
function Rect($r) { [ordered]@{ left = $r.L; top = $r.T; right = $r.R; bottom = $r.B; width = $r.R - $r.L; height = $r.B - $r.T } }
function Pump([int]$ms) { $until = [DateTime]::UtcNow.AddMilliseconds($ms); while ([DateTime]::UtcNow -lt $until) { [Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 20 } }

$physical = $desk::PhysicalCoordinates()
$executablePath = [IO.Path]::GetFullPath($Executable)
$bundlePath = Join-Path (Split-Path $executablePath) 'better_pull_review.rcc'
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$label = if ($QtScaleFactor) { "Qt override QT_SCALE_FACTOR=$QtScaleFactor, not native" } else { 'native display scale' }
$index = [ordered]@{
    schema = 'bpr.windows-caption-capture.v1'; card = 'BPR-0653'; revision = $Revision; status = 'not-run'; failure = $null
    executable = $executablePath; executable_sha256 = $null; bundle_sha256 = $null; arguments = @('--render-evidence')
    scale_label = $label; qt_scale_factor = $QtScaleFactor; physical_coordinates = $physical
    host = [ordered]@{ computer = $env:COMPUTERNAME; os = [Environment]::OSVersion.VersionString; session = [Diagnostics.Process]::GetCurrentProcess().SessionId
        user = [Security.Principal.WindowsIdentity]::GetCurrent().Name; system_dpi = $desk::GetDpiForSystem(); system_scale_percent = [int]($desk::GetDpiForSystem() * 100 / 96) }
    course_pid = $PID; application_pid = $null; exit_code = $null; pointer_saved = $null; pointer_restored = $null
    started_utc = (Get-Date).ToUniversalTime().ToString('o'); finished_utc = $null; images = @(); gestures = @(); errors = @()
}

Require $physical 'could not make this thread per-monitor DPI aware; rects and pixels would not be physical'
Require (Test-Path -LiteralPath $executablePath -PathType Leaf) "no image at $executablePath"
$index.executable_sha256 = Sha256 $executablePath
Require ($index.executable_sha256 -eq $ExpectedSha256.ToLower()) "image digest $($index.executable_sha256) is not the expected $ExpectedSha256"
Require (Test-Path -LiteralPath $bundlePath -PathType Leaf) "no resource bundle at $bundlePath"
$index.bundle_sha256 = Sha256 $bundlePath
Require ($index.bundle_sha256 -eq $ExpectedBundleSha256.ToLower()) "bundle digest $($index.bundle_sha256) is not the expected $ExpectedBundleSha256"
Require (-not (Test-Path -LiteralPath $outputPath)) "the output folder $outputPath already exists"
Require (Test-Path -LiteralPath (Split-Path $outputPath)) "the output folder's parent does not exist"
Require (@(Get-Process better_pull_review -ErrorAction SilentlyContinue).Count -eq 0) 'a better_pull_review.exe is already running'
$saved = New-Object BprCaptionCapture.Desk+POINT
Require ($desk::GetCursorPos([ref]$saved)) 'could not read the pointer position'
$index.pointer_saved = [ordered]@{ x = $saved.X; y = $saved.Y }
if ($DryRun) {
    $index.status = 'dry-run'
    $index | ConvertTo-Json -Depth 8
    Write-Output 'WINDOWS_CAPTION_CAPTURE_DRY_RUN_OK'
    exit 0
}

New-Item -ItemType Directory -Path $outputPath | Out-Null
$launched = $null
$process = $null
$form = $null
$hwnd = [IntPtr]::Zero
$held = $false
$neutral = $null
$originalForeground = $desk::GetForegroundWindow()

function Alive { $process -and -not $process.HasExited -and $desk::IsWindowVisible($hwnd) -and -not $desk::IsIconic($hwnd) }
# Every gesture's guard: our window in front, ours under the point, the expected hit code there.
function Guard([string]$gesture, [int]$x, [int]$y, [long]$code) {
    Require (Alive) "$gesture`: the image's window is gone or minimized"
    $foreground = $desk::GetForegroundWindow()
    Require ($foreground -eq $hwnd) "$gesture`: the image's window is not in front (foreground $(Hex $foreground) class $($desk::ClassName($foreground)) pid $($desk::Pid($foreground)))"
    $frame = $desk::Frame($hwnd)
    Require ($x -ge $frame.L -and $x -lt $frame.R -and $y -ge $frame.T -and $y -lt $frame.B) "$gesture`: ($x,$y) is outside the image's window"
    $monitor = $desk::Monitor($hwnd)
    Require ($x -ge $monitor.L -and $x -lt $monitor.R -and $y -ge $monitor.T -and $y -lt $monitor.B) "$gesture`: ($x,$y) is outside the monitor"
    $root = $desk::RootAt($x, $y)
    Require ($root -eq $hwnd -and $desk::Pid($root) -eq [uint32]$process.Id) "$gesture`: the window under ($x,$y) is $(Hex $root) class $($desk::ClassName($root)) pid $($desk::Pid($root)), not the image's"
    $answer = $desk::HitTest($hwnd, $x, $y)
    Require ($answer -eq $code) "$gesture`: ($x,$y) answers hit code $answer, not $code"
}
function Note([string]$gesture, [int]$x, [int]$y) {
    $index.gestures += [ordered]@{ gesture = $gesture; x = $x; y = $y; utc = (Get-Date).ToUniversalTime().ToString('o'); frame = (Rect $desk::Frame($hwnd)); zoomed = $desk::IsZoomed($hwnd) }
}
# Not `Move`: PowerShell resolves an alias before a function, and `Move` is
# Move-Item's alias.
function Move-Pointer([string]$gesture, $point, [long]$code) { Guard $gesture $point.X $point.Y $code; $desk::MoveTo($point.X, $point.Y); Note $gesture $point.X $point.Y }
function Press([string]$gesture, $point, [long]$code) { Guard $gesture $point.X $point.Y $code; $script:held = $true; $desk::Down($point.X, $point.Y); Note $gesture $point.X $point.Y }
function Release([string]$gesture, $point, [long]$code) { Guard $gesture $point.X $point.Y $code; $desk::Up($point.X, $point.Y); $script:held = $false; Note $gesture $point.X $point.Y }
function Point([int]$x, [int]$y) { $p = New-Object BprCaptionCapture.Desk+POINT; $p.X = $x; $p.Y = $y; $p }
function Save-Png($bitmap, [string]$name) {
    $path = Join-Path $outputPath $name
    $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    [ordered]@{ file = $name; width = $bitmap.Width; height = $bitmap.Height; sha256 = (Sha256 $path) }
}
# One state: the visible frame downscaled to at most 1920 wide, plus full-resolution crops.
function Capture([string]$state, [string]$note = '') {
    Pump 250
    $frame = $desk::Frame($hwnd)
    $buttons = Caption-Buttons
    $pointer = New-Object BprCaptionCapture.Desk+POINT
    [void]$desk::GetCursorPos([ref]$pointer)
    $bitmap = $desk::Grab($frame)
    try {
        $factor = [Math]::Min(1.0, 1920.0 / $bitmap.Width)
        $full = $desk::Scaled($bitmap, $factor)
        $stripBottom = [Math]::Max($buttons.minimize.B, [Math]::Max($buttons.maximize.B, $buttons.close.B)) - $frame.T
        $strip = $desk::Crop($bitmap, 0, 0, $bitmap.Width, $stripBottom + 2)
        $captionLeft = $buttons.minimize.L - $frame.L - 24
        $caption = $desk::Crop($bitmap, $captionLeft, 0, $bitmap.Width - $captionLeft, $stripBottom + 2)
        $images = [ordered]@{ window = (Save-Png $full "$state-window.png"); strip = (Save-Png $strip "$state-strip.png"); caption = (Save-Png $caption "$state-caption-buttons.png") }
        $full.Dispose(); $strip.Dispose(); $caption.Dispose()
    } finally { $bitmap.Dispose() }
    $index.images += [ordered]@{
        state = $state; note = $note; utc = (Get-Date).ToUniversalTime().ToString('o'); scale_label = $label
        window_dpi = $desk::GetDpiForWindow($hwnd); window_scale_percent = [int]($desk::GetDpiForWindow($hwnd) * 100 / 96)
        frame = (Rect $frame); outer = (Rect $desk::Outer($hwnd)); zoomed = $desk::IsZoomed($hwnd); window_image_factor = $factor
        caption_buttons = [ordered]@{ minimize = (Rect $buttons.minimize); maximize = (Rect $buttons.maximize); close = (Rect $buttons.close); maximize_name = $buttons.maximize_name }
        pointer = [ordered]@{ x = $pointer.X; y = $pointer.Y }; foreground = (Hex $desk::GetForegroundWindow())
        overlapping_windows = @($desk::Overlapping([uint32]$process.Id, $frame)); images = $images
    }
}
# The three caption buttons, by accessible name, as the topmost of that name in the strip.
function Caption-Buttons {
    $window = $ae::FromHandle($hwnd)
    $frame = $desk::Frame($hwnd)
    $found = [ordered]@{}
    foreach ($name in @('Minimize', 'Maximize', 'Restore', 'Close')) {
        $condition = [Windows.Automation.AndCondition]::new(
            [Windows.Automation.PropertyCondition]::new($ae::NameProperty, $name),
            [Windows.Automation.PropertyCondition]::new($ae::ControlTypeProperty, [Windows.Automation.ControlType]::Button))
        $best = $null
        foreach ($element in @($window.FindAll($scope::Descendants, $condition))) {
            $r = $element.Current.BoundingRectangle
            if ($r.Width -gt 0 -and $r.Y -lt $frame.T + 200 -and ($null -eq $best -or $r.Y -lt $best.Y)) { $best = $r }
        }
        if ($null -ne $best) {
            $rect = New-Object BprCaptionCapture.Desk+RECT
            $rect.L = [int]$best.X; $rect.T = [int]$best.Y; $rect.R = [int]($best.X + $best.Width); $rect.B = [int]($best.Y + $best.Height)
            $found[$name] = $rect
        }
    }
    Require ($found.Contains('Minimize') -and $found.Contains('Close') -and ($found.Contains('Maximize') -or $found.Contains('Restore'))) "caption buttons not found: $($found.Keys -join ',')"
    $maximizeName = if ($found.Contains('Maximize')) { 'Maximize' } else { 'Restore' }
    [pscustomobject]@{ minimize = $found['Minimize']; maximize = $found[$maximizeName]; close = $found['Close']; maximize_name = $maximizeName }
}
function Center($r) { Point ([int](($r.L + $r.R) / 2)) ([int](($r.T + $r.B) / 2)) }
# Empty strip space: the first point left of the caption buttons, on the buttons'
# centre line, that the window answers as HTCAPTION.
function Empty-Strip {
    $buttons = Caption-Buttons
    $frame = $desk::Frame($hwnd)
    $y = [int](($buttons.minimize.T + $buttons.minimize.B) / 2)
    for ($x = $buttons.minimize.L - 60; $x -gt $frame.L + ($frame.R - $frame.L) / 3; $x -= 30) {
        if ($desk::HitTest($hwnd, $x, $y) -eq $HT.caption) { return (Point $x $y) }
    }
    throw 'no empty strip space answered HTCAPTION'
}
function Hover-Press([string]$part) {
    $buttons = Caption-Buttons
    $target = Center $buttons.$part
    Move-Pointer "hover-$part" $target $HT.$part
    Pump 900
    Capture "hover-$part"
    if ($part -eq 'maximize') {
        $before = @($desk::Overlapping([uint32]$process.Id, $desk::Frame($hwnd)))
        $flyout = @()
        $deadline = [DateTime]::UtcNow.AddSeconds(4)
        while ([DateTime]::UtcNow -lt $deadline -and $flyout.Count -eq 0) {
            Pump 150
            $flyout = @($desk::Overlapping([uint32]$process.Id, $desk::Frame($hwnd)) | Where-Object { $before -notcontains $_ })
        }
        Capture 'snap-flyout' $(if ($flyout.Count -gt 0) { "appeared: $($flyout -join ' | ')" } else { 'no new shell window was seen over the window within 4 s' })
    }
    $script:neutral = Empty-Strip
    $neutral = $script:neutral
    Press "press-$part" $target $HT.$part
    Pump 400
    Capture "pressed-$part"
    # Off the button with it held, then released on empty strip space.
    Move-Pointer "move-off-$part" $neutral $HT.caption
    Pump 200
    Release "release-off-$part" $neutral $HT.caption
    Pump 700
    Require (Alive) "the window did not survive the $part press released off the button"
    Require (-not $desk::IsZoomed($hwnd)) "the $part press released off the button changed the window's state"
}

try {
    $index.status = 'running'
    $form = New-Object Windows.Forms.Form
    $form.Text = 'BetterPullReview caption capture launcher'
    $form.StartPosition = 'Manual'; $form.Location = New-Object Drawing.Point(24, 24); $form.Size = New-Object Drawing.Size(420, 110)
    $form.Show(); Pump 500
    Require ($desk::Front($form.Handle)) 'the launcher could not take the foreground'
    Pump 300
    Require ($desk::Nudge($form.Handle)) 'the launcher lost the foreground before the launch'
    Pump 200
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $executablePath
    $start.Arguments = '--render-evidence'
    $start.WorkingDirectory = Split-Path $executablePath
    if ($QtScaleFactor) { $start.EnvironmentVariables['QT_SCALE_FACTOR'] = $QtScaleFactor }
    $launched = [BprCaptionCapture.Launched]::Start($start)
    $process = $launched.P
    $index.application_pid = $process.Id
    $deadline = [DateTime]::UtcNow.AddSeconds(120)
    while ([DateTime]::UtcNow -lt $deadline -and -not $process.HasExited -and $hwnd -eq [IntPtr]::Zero) {
        Pump 200
        $candidates = @([Diagnostics.Process]::GetProcessById($process.Id).MainWindowHandle) | Where-Object { $_ -ne [IntPtr]::Zero }
        if ($candidates.Count -gt 0) { $hwnd = [IntPtr]$candidates[0] }
    }
    Require ($hwnd -ne [IntPtr]::Zero -and -not $process.HasExited) 'the image showed no window'
    Require ([IO.Path]::GetFullPath((Get-Process -Id $process.Id).Path) -eq $executablePath) 'PID executable mismatch'
    Pump 3000
    $index.window_title = $desk::Text($hwnd)
    $index.window_class = $desk::ClassName($hwnd)
    Require ($desk::Front($hwnd)) 'the image could not take the foreground'
    Pump 500
    Require (-not $desk::IsZoomed($hwnd)) 'the window opened maximized; the capture starts from the restored state'
    $neutral = Empty-Strip

    # At rest: the pointer is parked on empty strip space unless it is already outside the window.
    $pointer = New-Object BprCaptionCapture.Desk+POINT
    [void]$desk::GetCursorPos([ref]$pointer)
    if ($desk::RootAt($pointer.X, $pointer.Y) -eq $hwnd) { Move-Pointer 'park' $neutral $HT.caption; Pump 600 }
    Capture 'rest'

    foreach ($part in @('minimize', 'maximize', 'close')) { Hover-Press $part }

    # A drag of empty strip space, towards the monitor's middle, kept inside its work area.
    $neutral = Empty-Strip
    Move-Pointer 'drag-start' $neutral $HT.caption
    Pump 500
    Capture 'drag-before'
    $frame = $desk::Frame($hwnd); $work = $desk::Work($hwnd)
    $dx = if (($frame.L + $frame.R) / 2 -gt ($work.L + $work.R) / 2) { -160 } else { 160 }
    $dy = if (($frame.T + $frame.B) / 2 -gt ($work.T + $work.B) / 2) { -120 } else { 120 }
    Require ($frame.L + $dx -ge $work.L -and $frame.R + $dx -le $work.R -and $frame.T + $dy -ge $work.T -and $frame.B + $dy -le $work.B) 'the drag would leave the work area'
    Press 'drag-press' $neutral $HT.caption
    $steps = 8
    for ($step = 1; $step -le $steps; $step++) {
        $x = $neutral.X + [int]($dx * $step / $steps); $y = $neutral.Y + [int]($dy * $step / $steps)
        $monitor = $desk::Monitor($hwnd)
        Require (Alive) 'the window is gone mid-drag'
        Require ($x -ge $monitor.L + 8 -and $x -lt $monitor.R - 8 -and $y -ge $monitor.T + 8 -and $y -lt $monitor.B - 8) 'the drag point would reach the monitor edge'
        $desk::MoveTo($x, $y); Note "drag-step-$step" $x $y
        Pump 90
        if ($step -eq $steps / 2) { Capture 'drag-mid' 'the left button is held on empty strip space' }
    }
    $end = Point ($neutral.X + $dx) ($neutral.Y + $dy)
    Release 'drag-release' $end $HT.caption
    Pump 700
    Capture 'drag-after'
    $moved = $desk::Frame($hwnd)
    Require ([Math]::Abs(($moved.L - $frame.L) - $dx) -le 4 -and [Math]::Abs(($moved.T - $frame.T) - $dy) -le 4) "the drag moved the window by ($($moved.L - $frame.L),$($moved.T - $frame.T)), not ($dx,$dy)"

    # Maximize with a click on the button, then restore with a double-click on empty strip space.
    $buttons = Caption-Buttons
    $maximize = Center $buttons.maximize
    Press 'maximize-press' $maximize $HT.maximize
    Pump 120
    Release 'maximize-release' $maximize $HT.maximize
    Pump 1200
    Require ($desk::IsZoomed($hwnd)) 'a click on Maximize did not maximize the window'
    Move-Pointer 'maximized-park' (Empty-Strip) $HT.caption
    Pump 600
    Capture 'maximized' 'after a click on Maximize'
    $neutral = Empty-Strip
    Press 'restore-press-1' $neutral $HT.caption
    $desk::Up($neutral.X, $neutral.Y); $held = $false
    Pump 60
    Press 'restore-press-2' $neutral $HT.caption
    $desk::Up($neutral.X, $neutral.Y); $held = $false
    Note 'restore-double-click' $neutral.X $neutral.Y
    Pump 1200
    Require (-not $desk::IsZoomed($hwnd)) 'a double-click on empty strip space did not restore the window'
    $neutral = Empty-Strip
    Move-Pointer 'restored-park' $neutral $HT.caption
    Pump 600
    Capture 'restored' 'after a double-click on empty strip space'

    Require ($desk::PostMessage($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) 'WM_CLOSE refused'
    Require ($process.WaitForExit(15000)) 'WM_CLOSE did not stop the process'
    $index.exit_code = $process.ExitCode
    Require ($process.ExitCode -eq 0) "WM_CLOSE returned exit $($process.ExitCode)"
    $index.status = 'passed'
} catch {
    $index.status = 'failed'
    $index.failure = $_.Exception.Message + ' @ ' + $_.InvocationInfo.PositionMessage
} finally {
    # A button still held is taken to empty strip space, or failing that to the
    # middle of the window, below the strip, before it is released: never over
    # a caption button.
    if ($held) {
        try {
            $away = $null
            if ($null -ne $neutral -and (Alive) -and $desk::HitTest($hwnd, $neutral.X, $neutral.Y) -eq $HT.caption) { $away = $neutral }
            elseif ($hwnd -ne [IntPtr]::Zero) { $f = $desk::Frame($hwnd); $away = Point ([int](($f.L + $f.R) / 2)) ([int](($f.T + $f.B) / 2)) }
            if ($null -ne $away) { $desk::MoveTo($away.X, $away.Y); Pump 150; $desk::Up($away.X, $away.Y) }
            $index.errors += "a held button was released during cleanup at $($away.X),$($away.Y)"
        } catch { $index.errors += "cleanup release failed: $($_.Exception.Message)" }
    }
    if ($process) {
        if (-not $process.HasExited) {
            try { [void]$desk::PostMessage($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero); [void]$process.WaitForExit(10000) } catch { }
        }
        if (-not $process.HasExited) {
            $index.errors += "stopped leftover pid $($process.Id)"
            try { $process.Kill(); [void]$process.WaitForExit(10000) } catch { }
        }
        if ($process.HasExited) { $process.WaitForExit(); if ($null -eq $index.exit_code) { $index.exit_code = $process.ExitCode } }
    }
    try { $desk::MoveTo($saved.X, $saved.Y); $after = New-Object BprCaptionCapture.Desk+POINT; [void]$desk::GetCursorPos([ref]$after); $index.pointer_restored = [ordered]@{ x = $after.X; y = $after.Y } } catch { $index.errors += "pointer restore failed: $($_.Exception.Message)" }
    $now = $desk::GetForegroundWindow()
    $mine = ($form -and $now -eq $form.Handle) -or ($process -and $desk::Pid($now) -eq [uint32]$process.Id)
    $index.foreground_restored = [bool]($originalForeground -ne [IntPtr]::Zero -and $mine -and $desk::Front($originalForeground))
    if ($form) { $form.Close(); $form.Dispose() }
    if ($launched) { $index.stdout = $launched.Output; $index.stderr = $launched.Error }
    $index.remaining_bpr_processes = @(Get-Process better_pull_review -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
    $index.finished_utc = (Get-Date).ToUniversalTime().ToString('o')
    if (Test-Path -LiteralPath $outputPath) { $index | ConvertTo-Json -Depth 12 | Set-Content -Encoding UTF8 (Join-Path $outputPath 'index.json') }
}
if ($index.status -ne 'passed') { Write-Output "WINDOWS_CAPTION_CAPTURE_FAILED: $($index.failure)"; exit 1 }
Write-Output 'WINDOWS_CAPTION_CAPTURE_OK'
