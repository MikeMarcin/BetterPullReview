# Native Windows desktop course for profile activation and New Window ([[BPR-0980]]).
#
# Runs in the ordinary user's interactive session against an already-built,
# signed-out application and an existing owner-private data root the caller
# created (tests/end_to_end/bpr_narrow_owner_private.py makes one). It never
# signs in, changes a setting or policy, or touches another profile.
#
# The course stands a small launcher window of its own in for the shell. Before
# each launch the launcher is made the foreground window and receives one F24
# key press, so it is both the foreground process and the process that
# received the last input event, which is what the shell is after a person's
# click. Each launch is then a direct child of it. Without that press Windows
# grants a child no foreground right at all, and no activation can be judged.
# No pointer moves, and no key is sent while any other window is in front.
#
# Everything is read back through user32 and the process table: the foreground
# window, the top-level window holding keyboard focus, the application's
# top-level windows and every better_pull_review.exe. The record is bound to
# the executable's SHA-256 and every PID.
#
#   course       first launch; a second ordinary launch; Profiles > New Window;
#                an ordinary launch with the new window most recently used;
#                --new-window; then each window's own close.
#   close-probe  first launch, Profiles > New Window, close the new window.
param(
    [Parameter(Mandatory)] [string]$Executable,
    [Parameter(Mandatory)] [string]$DataRoot,
    [Parameter(Mandatory)] [string]$Output,
    [Parameter(Mandatory)] [string]$Revision,
    [string]$QtBin = '',
    [ValidateSet('course', 'close-probe')] [string]$Mode = 'course'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, UIAutomationClient, UIAutomationTypes, WindowsBase
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
namespace BprActivationCourse {
public static class Desk {
    [StructLayout(LayoutKind.Sequential)] struct LASTINPUTINFO { public uint cbSize; public uint dwTime; }
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] struct GUITHREADINFO { public uint cbSize, flags; public IntPtr hwndActive, hwndFocus, hwndCapture, hwndMenuOwner, hwndMoveSize, hwndCaret; public RECT rcCaret; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool GetLastInputInfo(ref LASTINPUTINFO p);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr h, uint cmd);
    [DllImport("user32.dll")] static extern bool SystemParametersInfoW(uint a, uint b, ref uint v, uint w);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
    [DllImport("user32.dll")] static extern bool GetGUIThreadInfo(uint tid, ref GUITHREADINFO g);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr h, uint f);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h, ref POINT p);

    public static uint IdleMs() { var i = new LASTINPUTINFO(); i.cbSize = (uint)Marshal.SizeOf(i); GetLastInputInfo(ref i); return (uint)Environment.TickCount - i.dwTime; }
    public static uint LockTimeoutMs() { uint v = 0; SystemParametersInfoW(0x2000, 0, ref v, 0); return v; }
    public static string Text(IntPtr h) { var s = new StringBuilder(1024); GetWindowTextW(h, s, 1024); return s.ToString(); }
    public static uint Pid(IntPtr h) { uint p; GetWindowThreadProcessId(h, out p); return p; }
    // Visible, unowned, titled top-level windows of one process, topmost first.
    public static List<IntPtr> TopLevel(uint pid) {
        var r = new List<IntPtr>();
        EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p);
            if (p == pid && IsWindowVisible(h) && GetWindow(h, 4) == IntPtr.Zero && Text(h).Length > 0) r.Add(h); return true; }, IntPtr.Zero);
        return r;
    }
    // The top-level window that holds keyboard focus in the foreground thread.
    public static IntPtr FocusTop() {
        uint p; uint tid = GetWindowThreadProcessId(GetForegroundWindow(), out p);
        var g = new GUITHREADINFO(); g.cbSize = (uint)Marshal.SizeOf(g);
        return GetGUIThreadInfo(tid, ref g) && g.hwndFocus != IntPtr.Zero ? GetAncestor(g.hwndFocus, 2) : IntPtr.Zero;
    }
    // Puts hwnd in front, as a click on it would, by sharing the current
    // foreground thread's input state for the duration of the call. Used for
    // the launcher, and for an application window before its own menu.
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
    // One F24 press and release, only while hwnd is in front.
    public static bool Nudge(IntPtr hwnd) {
        if (GetForegroundWindow() != hwnd) return false;
        keybd_event(0x87, 0, 0, UIntPtr.Zero); keybd_event(0x87, 0, 2, UIntPtr.Zero);
        return true;
    }
}
public sealed class Launched {
    public Process P; readonly StringBuilder o = new StringBuilder();
    public string Output { get { lock (o) return o.ToString(); } }
    public static Launched Start(ProcessStartInfo si) {
        var l = new Launched();
        si.UseShellExecute = false; si.RedirectStandardOutput = true; si.RedirectStandardError = true;
        l.P = new Process(); l.P.StartInfo = si;
        l.P.OutputDataReceived += (s, e) => { if (e.Data != null) lock (l.o) l.o.AppendLine(e.Data); };
        l.P.ErrorDataReceived += (s, e) => { if (e.Data != null) lock (l.o) l.o.AppendLine("ERR " + e.Data); };
        l.P.Start(); l.P.BeginOutputReadLine(); l.P.BeginErrorReadLine();
        return l;
    }
}
}
'@

$desk = [BprActivationCourse.Desk]
$ae = [Windows.Automation.AutomationElement]
$scope = [Windows.Automation.TreeScope]
$trueCondition = [Windows.Automation.Condition]::TrueCondition
$record = [ordered]@{
    schema = 'bpr.windows-profile-activation-course.v1'; mode = $Mode; revision = $Revision
    executable = $Executable; executable_sha256 = (Get-FileHash $Executable -Algorithm SHA256).Hash.ToLower()
    host = [ordered]@{ computer = $env:COMPUTERNAME; os = [Environment]::OSVersion.VersionString
        session = [Diagnostics.Process]::GetCurrentProcess().SessionId; user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
        elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) }
    data_root = $DataRoot; course_pid = $PID; steps = [ordered]@{}; checks = [ordered]@{}; launches = @(); errors = @()
}
$launched = New-Object System.Collections.Generic.List[object]
$owner = $null
$form = $null
$originalForeground = $desk::GetForegroundWindow()

function Require($condition, [string]$message) { if (-not $condition) { throw $message } }
function Hex($h) { '0x{0:X}' -f ([IntPtr]$h).ToInt64() }
function Pump([int]$ms) { $until = [DateTime]::UtcNow.AddMilliseconds($ms); while ([DateTime]::UtcNow -lt $until) { [Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 20 } }
function Wait-Until([int]$ms, [scriptblock]$predicate) { $until = [DateTime]::UtcNow.AddMilliseconds($ms); while ([DateTime]::UtcNow -lt $until) { if (& $predicate) { return $true }; Pump 100 }; [bool](& $predicate) }
function OwnerWindows { @($desk::TopLevel([uint32]$owner.P.Id)) }
function State([string]$name) {
    $fg = $desk::GetForegroundWindow()
    $s = [ordered]@{ utc = (Get-Date).ToUniversalTime().ToString('o'); idle_ms = $desk::IdleMs()
        foreground = [ordered]@{ hwnd = (Hex $fg); pid = $desk::Pid($fg); title = $desk::Text($fg) }
        focus_top = (Hex $desk::FocusTop())
        windows = @(OwnerWindows | ForEach-Object { [ordered]@{ hwnd = (Hex $_); title = $desk::Text($_) } })
        bpr_processes = @(Get-Process better_pull_review -ErrorAction SilentlyContinue | ForEach-Object { $_.Id }) }
    $record.steps[$name] = $s
    $s
}
function Launch([string]$name, [string[]]$arguments) {
    Require ($desk::Front($form.Handle)) "launcher could not take the foreground before $name"
    Pump 300
    Require ($desk::Nudge($form.Handle)) "launcher lost the foreground before $name"
    Pump 200
    Require ($desk::GetForegroundWindow() -eq $form.Handle) "launcher lost the foreground before $name"
    $si = New-Object Diagnostics.ProcessStartInfo
    $si.FileName = $Executable
    $si.Arguments = ((@('--test-application-data-root', $DataRoot) + $arguments) | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
    $si.WorkingDirectory = Split-Path $Executable
    if ($QtBin) { $si.EnvironmentVariables['PATH'] = $QtBin + ';' + $env:PATH; $si.EnvironmentVariables['QT_QPA_FONTDIR'] = "$env:WINDIR\Fonts" }
    $l = [BprActivationCourse.Launched]::Start($si)
    $launched.Add($l)
    $entry = [ordered]@{ step = $name; pid = $l.P.Id; arguments = $arguments; parent_pid = $PID; idle_ms_at_launch = $desk::IdleMs(); started_utc = (Get-Date).ToUniversalTime().ToString('o') }
    $record.launches += $entry
    [pscustomobject]@{ L = $l; Entry = $entry }
}
function Finish-Launch($item) {
    $done = Wait-Until 30000 { $item.L.P.HasExited }
    if ($done) { $item.L.P.WaitForExit(); $item.Entry.exit_code = $item.L.P.ExitCode }
    $item.Entry.output = $item.L.Output
    Pump 3000
}
function Menu-Items([string]$name) {
    $c = [Windows.Automation.PropertyCondition]::new($ae::ProcessIdProperty, $owner.P.Id)
    $found = @()
    foreach ($w in @($ae::RootElement.FindAll($scope::Children, $c))) {
        $found += @($w.FindAll($scope::Descendants, $trueCondition) | Where-Object {
            $_.Current.ControlType -eq [Windows.Automation.ControlType]::MenuItem -and -not $_.Current.IsOffscreen -and
            (($_.Current.Name.Replace('&', '') -split "`t")[0] -eq $name) })
    }
    ,$found
}
# The strip's Application menu control, then a menu, then its item, as
# windows_settings_inventory.ps1 drives it: a posted click, then UI Automation.
# A menu an accessible invoke leaves open is closed with posted Escape
# messages, never injected keys, so the application never becomes the
# last-input process and gains no foreground right from the course.
function Menu-Command([IntPtr]$hwnd, [string]$menu, [string]$item) {
    Require ($desk::Front($hwnd)) "could not put $(Hex $hwnd) in front for its menu"
    Pump 300
    $window = $ae::FromHandle($hwnd)
    $button = $window.FindFirst($scope::Descendants, [Windows.Automation.PropertyCondition]::new($ae::NameProperty, 'Application menu'))
    Require ($null -ne $button) 'missing the Application menu control'
    $r = $button.Current.BoundingRectangle
    $pt = New-Object BprActivationCourse.Desk+POINT
    $pt.X = [int]($r.X + $r.Width / 2); $pt.Y = [int]($r.Y + $r.Height / 2)
    [void]$desk::ScreenToClient($hwnd, [ref]$pt)
    $lp = [IntPtr](($pt.Y -shl 16) -bor ($pt.X -band 0xFFFF))
    Require ($desk::PostMessage($hwnd, 0x201, [IntPtr]1, $lp) -and $desk::PostMessage($hwnd, 0x202, [IntPtr]0, $lp)) 'posted click refused'
    Require (Wait-Until 8000 { (Menu-Items $menu).Count -eq 1 }) "menu $menu did not appear"
    $parent = (Menu-Items $menu)[0]; $expand = $null
    if ($parent.TryGetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern, [ref]$expand)) { $expand.Expand() }
    else { $parent.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke() }
    Require (Wait-Until 8000 { (Menu-Items $item).Count -eq 1 }) "menu item $item did not appear"
    (Menu-Items $item)[0].GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Pump 500
    for ($i = 0; $i -lt 3 -and (Menu-Items $item).Count -gt 0; $i++) {
        $top = (Menu-Items $item)[0]; $walker = [Windows.Automation.TreeWalker]::ControlViewWalker
        while ($true) { $up = $walker.GetParent($top); if ($null -eq $up -or $up -eq $ae::RootElement) { break }; $top = $up }
        $h = [IntPtr]$top.Current.NativeWindowHandle
        [void]$desk::PostMessage($h, 0x100, [IntPtr]0x1B, [IntPtr]0x00010001)
        [void]$desk::PostMessage($h, 0x101, [IntPtr]0x1B, [IntPtr]0xC0010001)
        Pump 300
    }
}
function Is-Front([IntPtr]$hwnd, $state) { $state.foreground.hwnd -eq (Hex $hwnd) -and $state.focus_top -eq (Hex $hwnd) }
function Only-Owner($state) { @($state.bpr_processes).Count -eq 1 -and $state.bpr_processes[0] -eq $owner.P.Id }

try {
    Require (@(Get-Process better_pull_review -ErrorAction SilentlyContinue).Count -eq 0) 'a better_pull_review.exe is already running'
    Require (Test-Path $DataRoot) "data root $DataRoot is absent"
    $record.checks.preflight_idle_ms = $desk::IdleMs()
    $record.checks.foreground_lock_timeout_ms = $desk::LockTimeoutMs()
    $form = New-Object Windows.Forms.Form
    $form.Text = 'BetterPullReview activation course launcher'
    $form.StartPosition = 'Manual'; $form.Location = New-Object Drawing.Point(24, 24); $form.Size = New-Object Drawing.Size(420, 110)
    $form.Show(); Pump 500

    $first = Launch 'first-launch' @()
    $owner = $first.L
    $record.owner_pid = $owner.P.Id
    Require (Wait-Until 120000 { $owner.P.HasExited -or (OwnerWindows).Count -ge 1 }) 'the first launch showed no window'
    Require (-not $owner.P.HasExited) "the first launch exited $($owner.P.ExitCode)"
    Pump 3000
    $w1 = (OwnerWindows)[0]; $record.w1 = Hex $w1
    $s = State 'first-launch'
    $record.checks.first_launch_one_window_in_front = ((OwnerWindows).Count -eq 1 -and (Is-Front $w1 $s))

    if ($Mode -eq 'course') {
        $second = Launch 'second-ordinary-launch' @(); Finish-Launch $second
        $s = State 'second-ordinary-launch'
        $record.checks.second_launch_queued_exit_zero = ($second.Entry.exit_code -eq 0 -and $second.Entry.output -match 'PROFILE_ACTIVATION_QUEUED')
        $record.checks.second_launch_no_second_window = ((OwnerWindows).Count -eq 1)
        $record.checks.second_launch_one_process = (Only-Owner $s)
        $record.checks.second_launch_first_window_in_front_with_focus = (Is-Front $w1 $s)
    }

    Menu-Command $w1 'Profiles' 'New Window'
    Require (Wait-Until 60000 { (OwnerWindows).Count -eq 2 }) 'Profiles > New Window opened no second window'
    Pump 2000
    $w2 = @(OwnerWindows | Where-Object { $_ -ne $w1 })[0]; $record.w2 = Hex $w2
    $s = State 'profiles-new-window'
    $record.checks.menu_new_window_in_front = (Is-Front $w2 $s)
    if (-not (Is-Front $w2 $s)) { Require ($desk::Front($w2)) 'could not bring the new window in front'; Pump 1500; [void](State 'new-window-brought-in-front') }

    if ($Mode -eq 'close-probe') {
        $ae::FromHandle($w2).GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close()
        Pump 5000
        $record.checks.alive_after_closing_new_window = (-not $owner.P.HasExited)
        if ($owner.P.HasExited) { $owner.P.WaitForExit(); $record.checks.exit_code_after_closing_new_window = $owner.P.ExitCode }
    } else {
        $third = Launch 'third-ordinary-launch' @(); Finish-Launch $third
        $s = State 'third-ordinary-launch'
        $record.checks.third_launch_queued_exit_zero = ($third.Entry.exit_code -eq 0 -and $third.Entry.output -match 'PROFILE_ACTIVATION_QUEUED')
        $record.checks.third_launch_windows_unchanged = ((OwnerWindows).Count -eq 2)
        $record.checks.third_launch_one_process = (Only-Owner $s)
        $record.checks.third_launch_most_recent_window_in_front_with_focus = (Is-Front $w2 $s)

        $fourth = Launch 'new-window-launch' @('--new-window'); Finish-Launch $fourth
        $opened = Wait-Until 60000 { (OwnerWindows).Count -eq 3 }
        Pump 2000
        $s = State 'new-window-launch'
        $w3 = @(OwnerWindows | Where-Object { $_ -ne $w1 -and $_ -ne $w2 })
        $record.w3 = if ($w3.Count -gt 0) { Hex $w3[0] } else { $null }
        $record.checks.new_window_launch_queued_exit_zero = ($fourth.Entry.exit_code -eq 0 -and $fourth.Entry.output -match 'PROFILE_ACTIVATION_QUEUED')
        $record.checks.new_window_launch_third_window = $opened
        $record.checks.new_window_launch_one_process = (Only-Owner $s)

        # Each window's own close; the last one ends the process.
        foreach ($h in @(OwnerWindows)) {
            try { $ae::FromHandle($h).GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close() } catch { $record.errors += "close $(Hex $h): $($_.Exception.Message)" }
            Pump 1500
        }
        $closed = Wait-Until 90000 { $owner.P.HasExited }
        $record.checks.closed = $closed
        if ($closed) { $owner.P.WaitForExit(); $record.checks.close_exit_code = $owner.P.ExitCode }
    }
} catch {
    $record.errors += ($_.Exception.Message + ' @ ' + $_.InvocationInfo.PositionMessage)
} finally {
    foreach ($l in $launched) {
        if (-not $l.P.HasExited) { $record.errors += "stopped leftover pid $($l.P.Id)"; try { $l.P.Kill(); [void]$l.P.WaitForExit(10000) } catch {} }
    }
    if ($owner) { $record.owner_output = $owner.Output }
    # Hand the foreground back only while this course or its application holds it.
    $now = $desk::GetForegroundWindow()
    $mine = ($form -and $now -eq $form.Handle) -or ($owner -and $desk::Pid($now) -eq [uint32]$owner.P.Id)
    $record.foreground_restored = [bool]($originalForeground -ne [IntPtr]::Zero -and $mine -and $desk::Front($originalForeground))
    if ($form) { $form.Close(); $form.Dispose() }
    $record.finished_utc = (Get-Date).ToUniversalTime().ToString('o')
    $record.remaining_bpr_processes = @(Get-Process better_pull_review -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
    $record | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $Output
}
if ($record.errors.Count -gt 0) { exit 1 }
