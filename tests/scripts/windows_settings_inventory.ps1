# Native Windows Settings course and inventory for a tests-disabled, signed-out
# image ([[BPR-0974]] AC2 and AC3; first written for [[BPR-0593]]).
#
# The course launches the image itself, once, bound to its SHA-256 and to its
# resource bundle's, with `--render-evidence` and no value, so the image runs on
# a private temporary profile and never on the owner's. It never signs in,
# changes a setting, checks for updates, exports, or maintains anything, and it
# never captures an editable value.
#
# The foreground is taken as windows_profile_activation_course.ps1 takes it
# ([[BPR-0980]]). A small launcher window of the course's own stands in for the
# shell. Before the launch it is put in front and receives one F24 key press, so
# the image starts as the child of the foreground, last-input process. After
# that, each Focus puts one of the image's own windows in front by sharing the
# foreground thread's input state for the call (`Front`). No pointer moves:
# clicks are posted messages, menus are driven through UI Automation, and a key
# is sent only while one of the image's windows is in front.
#
#   course     AC2 and AC3. View > Settings..., modeless focus, the Ctrl+comma
#              singleton, Escape, Ctrl+comma reopen, Profiles > New Window, the
#              sibling's menu reuses Settings, caption close, the sibling's
#              shortcut, closing the first window keeps Settings, the seven
#              pages, then File > Quit, which must exit 0.
#   inventory  AC3 alone, for when the course stops before the pages.
#              View > Settings..., the seven pages, Escape, then WM_CLOSE.
#   -DryRun    Checks the image, the bundle, the output and the desktop, and
#              prints the plan. It launches nothing, makes no window, and sends
#              no input.
#
# The record is written whatever happens, with the pages captured so far, the
# PIDs, the exit code and the failure, bound to the image's SHA-256.
param(
    [Parameter(Mandatory)] [string]$Executable,
    [Parameter(Mandatory)] [string]$ExpectedSha256,
    [string]$ExpectedBundleSha256 = '',
    [Parameter(Mandatory)] [string]$Revision,
    [Parameter(Mandatory)] [string]$Output,
    [ValidateSet('course', 'inventory')] [string]$Mode = 'course',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, UIAutomationClient, UIAutomationTypes, WindowsBase
Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
namespace BprSettingsCourse {
// The image's standard output and error, read on the process's own threads.
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
    [StructLayout(LayoutKind.Sequential)] struct LASTINPUTINFO { public uint cbSize; public uint dwTime; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] static extern bool GetLastInputInfo(ref LASTINPUTINFO p);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] static extern bool SystemParametersInfoW(uint a, uint b, ref uint v, uint w);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetDpiForSystem();

    public static uint IdleMs() { var i = new LASTINPUTINFO(); i.cbSize = (uint)Marshal.SizeOf(i); GetLastInputInfo(ref i); return (uint)Environment.TickCount - i.dwTime; }
    public static uint LockTimeoutMs() { uint v = 0; SystemParametersInfoW(0x2000, 0, ref v, 0); return v; }
    public static string Text(IntPtr h) { var s = new StringBuilder(1024); GetWindowTextW(h, s, 1024); return s.ToString(); }
    public static uint Pid(IntPtr h) { uint p; GetWindowThreadProcessId(h, out p); return p; }
    // Puts hwnd in front, as a click on it would, by sharing the current
    // foreground thread's input state for the duration of the call.
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
    // One key, with Control held if asked, only while a window of pid is in front.
    public static bool Key(uint pid, byte key, bool control) {
        if (Pid(GetForegroundWindow()) != pid) return false;
        if (control) keybd_event(0x11, 0, 0, UIntPtr.Zero);
        keybd_event(key, 0, 0, UIntPtr.Zero); keybd_event(key, 0, 2, UIntPtr.Zero);
        if (control) keybd_event(0x11, 0, 2, UIntPtr.Zero);
        return true;
    }
}
}
'@

$desk = [BprSettingsCourse.Desk]
$ae = [Windows.Automation.AutomationElement]
$scope = [Windows.Automation.TreeScope]
$walker = [Windows.Automation.TreeWalker]::RawViewWalker
$trueCondition = [Windows.Automation.Condition]::TrueCondition
$categories = @('General', 'Accounts & hosts', 'Reviews & queue', 'Appearance', 'Notifications', 'Agents & context', 'Storage & privacy')
$workbenchTitle = '^BetterPullReview\s[\u2014-]\s.*profile'
$settingsTitle = '^Settings\s[\u2014-]\s.*profile'
# The file stays ASCII: Windows PowerShell 5.1 reads a file without a BOM in
# the ANSI code page, so the titles' em dash is written as a regex escape.

function Require($condition, [string]$message) { if (-not $condition) { throw $message } }
function Sha256([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower() }
function Hex($h) { '0x{0:X}' -f ([IntPtr]$h).ToInt64() }

$executablePath = [IO.Path]::GetFullPath($Executable)
$bundlePath = Join-Path (Split-Path $executablePath) 'better_pull_review.rcc'
$record = [ordered]@{
    schema = 'bpr.settings-inventory.uia.v1'; course = $Mode; status = 'not-run'; failure = $null
    build = 'windows-tests-disabled-qt6120-release'; revision = $Revision
    executable = $executablePath; executable_sha256 = $null; bundle_sha256 = $null
    platform = "Windows $([Environment]::OSVersion.Version) x64"; source = 'native UI Automation raw tree, user32 posted clicks and keys, no pointer'
    host = [ordered]@{ computer = $env:COMPUTERNAME; session = [Diagnostics.Process]::GetCurrentProcess().SessionId
        user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
        elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
        foreground_lock_timeout_ms = $desk::LockTimeoutMs(); system_dpi = $desk::GetDpiForSystem() }
    course_pid = $PID; application_pid = $null; exit_code = $null; window_names = @(); window_dpi = $null
    started_utc = (Get-Date).ToUniversalTime().ToString('o'); finished_utc = $null
    categories = $categories; native_checks = @(); pages = @(); stdout = ''; stderr = ''; errors = @()
}

# Preflight: every refusal happens before anything is launched or shown.
Require (Test-Path -LiteralPath $executablePath -PathType Leaf) "no image at $executablePath"
$record.executable_sha256 = Sha256 $executablePath
Require ($record.executable_sha256 -eq $ExpectedSha256.ToLower()) "image digest $($record.executable_sha256) is not the expected $ExpectedSha256"
if ($ExpectedBundleSha256) {
    Require (Test-Path -LiteralPath $bundlePath -PathType Leaf) "no resource bundle at $bundlePath"
    $record.bundle_sha256 = Sha256 $bundlePath
    Require ($record.bundle_sha256 -eq $ExpectedBundleSha256.ToLower()) "bundle digest $($record.bundle_sha256) is not the expected $ExpectedBundleSha256"
}
Require (-not (Test-Path -LiteralPath $Output)) "the record $Output already exists"
Require (Test-Path -LiteralPath (Split-Path ([IO.Path]::GetFullPath($Output)))) "the record's folder does not exist"
Require (@(Get-Process better_pull_review -ErrorAction SilentlyContinue).Count -eq 0) 'a better_pull_review.exe is already running'
if ($DryRun) {
    [ordered]@{ dry_run = $true; mode = $Mode; executable = $executablePath; executable_sha256 = $record.executable_sha256
        bundle_sha256 = $record.bundle_sha256; arguments = @('--render-evidence'); output = $Output; host = $record.host
        idle_ms = $desk::IdleMs(); foreground = [ordered]@{ hwnd = (Hex $desk::GetForegroundWindow()); title = $desk::Text($desk::GetForegroundWindow()) } } | ConvertTo-Json -Depth 5
    Write-Output 'WINDOWS_SETTINGS_INVENTORY_DRY_RUN_OK'
    exit 0
}

$launched = $null
$process = $null
$form = $null
$originalForeground = $desk::GetForegroundWindow()

function Pump([int]$ms) { $until = [DateTime]::UtcNow.AddMilliseconds($ms); while ([DateTime]::UtcNow -lt $until) { [Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 20 } }
function Wait-For([string]$message, [scriptblock]$predicate, [int]$seconds = 8) {
    $deadline = [DateTime]::UtcNow.AddSeconds($seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $predicate) { return }
        Pump 80
    }
    throw $message
}
function Windows {
    $condition = [Windows.Automation.PropertyCondition]::new($ae::ProcessIdProperty, $process.Id)
    @($ae::RootElement.FindAll($scope::Children, $condition) | Where-Object { -not $_.Current.IsOffscreen })
}
function Workbenches { @(Windows | Where-Object { $_.Current.Name -match $workbenchTitle }) }
function Settings { @(Windows | Where-Object { $_.Current.Name -match $settingsTitle }) }
function Handle($element) { [IntPtr]$element.Current.NativeWindowHandle }
function Focus($element) {
    $hwnd = Handle $element
    Require ($desk::Front($hwnd)) "could not put $($element.Current.Name) in front"
    Wait-For 'Foreground focus refused' { $desk::GetForegroundWindow() -eq $hwnd }
    Pump 200
}
function Key([byte]$code, [bool]$control = $false) {
    Require ($desk::Key([uint32]$process.Id, $code, $control)) 'Refuse key outside owned process'
    Pump 150
}
function Find($root, [string]$name) {
    $condition = [Windows.Automation.PropertyCondition]::new($ae::NameProperty, $name)
    $element = $root.FindFirst($scope::Descendants, $condition)
    Require ($null -ne $element) "Missing accessible element: $name"
    $element
}
function Click($element, $window) {
    $rect = $element.Current.BoundingRectangle
    Require ($rect.Width -gt 0 -and $rect.Height -gt 0) 'Empty native click target'
    $point = New-Object BprSettingsCourse.Desk+POINT
    $point.X = [int]($rect.X + $rect.Width / 2)
    $point.Y = [int]($rect.Y + $rect.Height / 2)
    [void]$desk::ScreenToClient((Handle $window), [ref]$point)
    $position = [IntPtr](($point.Y -shl 16) -bor ($point.X -band 0xFFFF))
    Require ($desk::PostMessage((Handle $window), 0x201, [IntPtr]1, $position)) 'Mouse down refused'
    Require ($desk::PostMessage((Handle $window), 0x202, [IntPtr]0, $position)) 'Mouse up refused'
    Pump 150
}
function Menu-Items([string]$name) {
    $found = @()
    foreach ($window in (Windows)) {
        $found += @($window.FindAll($scope::Descendants, $trueCondition) | Where-Object {
            $_.Current.ControlType -eq [Windows.Automation.ControlType]::MenuItem -and
            ($_.Current.Name.Replace('&', '') -split "`t")[0] -eq $name -and -not $_.Current.IsOffscreen
        })
    }
    , $found
}
function Menu-Item([string]$name) {
    $found = $null
    try { Wait-For "no visible menu item $name" { (Menu-Items $name).Count -eq 1 } } catch {
        $visible = @()
        foreach ($window in (Windows)) {
            $visible += @($window.FindAll($scope::Descendants, $trueCondition) | Where-Object { $_.Current.ControlType -eq [Windows.Automation.ControlType]::MenuItem -and -not $_.Current.IsOffscreen } | ForEach-Object { $_.Current.Name })
        }
        throw "Expected one visible menu item $name, got $((Menu-Items $name).Count); observed=$($visible -join '|')"
    }
    (Menu-Items $name)[0]
}
# Every visible menu item of the image, in any of its windows.
function Open-Menu-Items {
    $found = @()
    foreach ($window in (Windows)) {
        $found += @($window.FindAll($scope::Descendants, $trueCondition) | Where-Object {
            $_.Current.ControlType -eq [Windows.Automation.ControlType]::MenuItem -and -not $_.Current.IsOffscreen })
    }
    , $found
}
# A menu an accessible invoke leaves open is closed with posted Escape
# messages to its own window, never injected keys, as the activation course
# does. Left open, the next posted click on the Application menu control only
# closes it.
function Close-Menus {
    for ($i = 0; $i -lt 4 -and (Open-Menu-Items).Count -gt 0; $i++) {
        $top = (Open-Menu-Items)[0]
        $control = [Windows.Automation.TreeWalker]::ControlViewWalker
        while ($true) { $up = $control.GetParent($top); if ($null -eq $up -or $up -eq $ae::RootElement) { break }; $top = $up }
        $h = [IntPtr]$top.Current.NativeWindowHandle
        [void]$desk::PostMessage($h, 0x100, [IntPtr]0x1B, [IntPtr]0x00010001)
        [void]$desk::PostMessage($h, 0x101, [IntPtr]0x1B, [IntPtr]0xC0010001)
        Pump 300
    }
}
# The strip's Application menu control by a posted click, then the menu and its
# item through UI Automation, as windows_profile_activation_course.ps1 does.
function Menu-Command($window, [string]$menu, [string]$item) {
    Close-Menus
    Focus $window
    Click (Find $window 'Application menu') $window
    $parent = Menu-Item $menu
    $expand = $null
    if ($parent.TryGetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern, [ref]$expand)) { $expand.Expand() }
    else { $parent.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke() }
    Pump 150
    $command = Menu-Item $item
    Write-Output "MENU_ITEM:$item ACCELERATOR:$($command.Current.AcceleratorKey) ACCESS:$($command.Current.AccessKey)"
    $command.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Pump 500
    if ($item -ne 'Quit') { Close-Menus }
}
function Close($window) { $window.GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close() }
function Nodes($element, [int]$depth = 0) {
    Require ($depth -lt 32) 'Unbounded accessibility depth'
    $current = $element.Current
    if ($current.IsOffscreen) { return }
    $state = ''
    $toggle = $null
    if ($element.TryGetCurrentPattern([Windows.Automation.TogglePattern]::Pattern, [ref]$toggle)) { $state = $toggle.Current.ToggleState.ToString() }
    [ordered]@{ depth = $depth; type = $current.ControlType.ProgrammaticName.Replace('ControlType.', ''); name = $current.Name; enabled = $current.IsEnabled; state = $state }
    $child = $walker.GetFirstChild($element)
    while ($null -ne $child) { Nodes $child ($depth + 1); $child = $walker.GetNextSibling($child) }
}
function Capture-Pages($shared) {
    for ($index = 0; $index -lt $categories.Count; $index++) {
        $category = $categories[$index]
        Focus $shared
        $sidebar = Find $shared 'Settings pages'
        Click (Find $sidebar $category) $shared
        $page = Find $shared 'Settings page'
        Find $page $category | Out-Null
        $nodes = @(Nodes $page)
        Require ($nodes.Count -lt 2000) 'Unbounded page nodes'
        $record.pages += [ordered]@{ index = $index; category = $category; nodes = $nodes }
    }
    $record.native_checks += 'seven-categories-with-current-page-heading'
}

try {
    $record.status = 'running'
    $form = New-Object Windows.Forms.Form
    $form.Text = 'BetterPullReview Settings course launcher'
    $form.StartPosition = 'Manual'; $form.Location = New-Object Drawing.Point(24, 24); $form.Size = New-Object Drawing.Size(420, 110)
    $form.Show(); Pump 500
    Require ($desk::Front($form.Handle)) 'the launcher could not take the foreground'
    Pump 300
    Require ($desk::Nudge($form.Handle)) 'the launcher lost the foreground before the launch'
    Pump 200
    Require ($desk::GetForegroundWindow() -eq $form.Handle) 'the launcher lost the foreground before the launch'

    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $executablePath
    $start.Arguments = '--render-evidence'
    $start.WorkingDirectory = Split-Path $executablePath
    $launched = [BprSettingsCourse.Launched]::Start($start)
    $process = $launched.P
    $record.application_pid = $process.Id

    Wait-For 'Initial signed-out workbench absent' { $process.HasExited -or (Workbenches).Count -eq 1 } 120
    Require (-not $process.HasExited) "the image exited $($process.ExitCode) before its window"
    Require ([IO.Path]::GetFullPath((Get-Process -Id $process.Id).Path) -eq $executablePath) 'PID executable mismatch'
    Pump 1500
    $first = (Workbenches)[0]
    $record.window_dpi = $desk::GetDpiForWindow((Handle $first))

    if ($Mode -eq 'inventory') {
        Menu-Command $first 'View' ('Settings' + [char]0x2026)
        Wait-For 'Native Settings menu did not open one labelled window' { (Settings).Count -eq 1 }
        $record.native_checks += 'native-menu-opens-profile-labelled-settings'
        Capture-Pages (Settings)[0]
        Focus (Settings)[0]
        Key 0x1B
        Wait-For 'Escape did not close Settings' { (Settings).Count -eq 0 }
        Require ($desk::PostMessage((Handle (Workbenches)[0]), 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) 'WM_CLOSE refused'
        Require ($process.WaitForExit(15000)) 'WM_CLOSE did not stop the process'
        $record.exit_code = $process.ExitCode
        Require ($process.ExitCode -eq 0) "WM_CLOSE returned exit $($process.ExitCode)"
        $record.native_checks += 'native-window-close-process-terminated'
    } else {
        Menu-Command $first 'View' ('Settings' + [char]0x2026)
        Wait-For 'Native Settings menu did not open one labelled window' { (Settings).Count -eq 1 }
        $shared = (Settings)[0]
        $identity = Handle $shared
        Focus $first
        Require ((Settings).Count -eq 1) 'Settings is not modeless'
        Key 0xBC $true
        try { Wait-For 'Ctrl-comma did not reuse/focus Settings' { (Settings).Count -eq 1 -and (Handle (Settings)[0]) -eq $identity -and $desk::GetForegroundWindow() -eq $identity } } catch {
            throw "Ctrl-comma: count=$((Settings).Count), identity=$(Hex $identity), actual=$((Settings | ForEach-Object { Hex (Handle $_) }) -join ','), foreground=$(Hex $desk::GetForegroundWindow())"
        }
        $record.native_checks += 'native-menu-ctrl-comma-modeless-singleton-profile-label'
        Key 0x1B
        Wait-For 'Escape did not close Settings' { (Settings).Count -eq 0 }
        Focus $first
        Key 0xBC $true
        Wait-For 'Ctrl-comma did not reopen Settings' { (Settings).Count -eq 1 }
        $identity = Handle (Settings)[0]
        $record.native_checks += 'escape-and-shortcut-reopen'
        Menu-Command $first 'Profiles' 'New Window'
        Wait-For 'New Window did not create same-profile sibling' { (Workbenches).Count -eq 2 } 60
        $second = @(Workbenches | Where-Object { (Handle $_) -ne (Handle $first) })[0]
        Menu-Command $second 'View' ('Settings' + [char]0x2026)
        Wait-For 'Sibling menu did not reuse Settings' { (Settings).Count -eq 1 -and (Handle (Settings)[0]) -eq $identity }
        Close (Settings)[0]
        Wait-For 'Caption close did not close Settings' { (Settings).Count -eq 0 }
        Focus $second
        Key 0xBC $true
        Wait-For 'Sibling shortcut did not reopen Settings' { (Settings).Count -eq 1 }
        Close $first
        Wait-For 'Retiring sibling lost Settings' { (Workbenches).Count -eq 1 -and (Settings).Count -eq 1 }
        $record.native_checks += 'same-profile-sibling-menu-shortcut-and-retirement'
        Capture-Pages (Settings)[0]
        Menu-Command $second 'File' 'Quit'
        Require ($process.WaitForExit(15000)) 'Native Quit did not stop process'
        $record.exit_code = $process.ExitCode
        Require ($process.ExitCode -eq 0) "Native Quit returned exit $($process.ExitCode)"
        $record.native_checks += 'native-quit-exit-zero'
    }
    $record.status = 'passed'
} catch {
    $record.status = 'failed'
    $record.failure = $_.Exception.Message + ' @ ' + $_.InvocationInfo.PositionMessage
} finally {
    if ($process) {
        try { $record.window_names = @(Windows | ForEach-Object { $_.Current.Name }) } catch { }
        if (-not $process.HasExited) {
            $record.errors += "stopped leftover pid $($process.Id)"
            try { $process.Kill(); [void]$process.WaitForExit(10000) } catch { }
        }
        if ($process.HasExited) {
            # Also waits for the redirected output to reach its end.
            $process.WaitForExit()
            if ($null -eq $record.exit_code) { $record.exit_code = $process.ExitCode }
        }
    }
    if ($launched) { $record.stdout = $launched.Output; $record.stderr = $launched.Error }
    # Hand the foreground back only while this course or its image holds it.
    $now = $desk::GetForegroundWindow()
    $mine = ($form -and $now -eq $form.Handle) -or ($process -and $desk::Pid($now) -eq [uint32]$process.Id)
    $record.foreground_restored = [bool]($originalForeground -ne [IntPtr]::Zero -and $mine -and $desk::Front($originalForeground))
    if ($form) { $form.Close(); $form.Dispose() }
    $record.remaining_bpr_processes = @(Get-Process better_pull_review -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
    $record.finished_utc = (Get-Date).ToUniversalTime().ToString('o')
    $record | ConvertTo-Json -Depth 20 | Set-Content -Encoding UTF8 $Output
}
if ($record.status -ne 'passed') { Write-Output "WINDOWS_SETTINGS_INVENTORY_FAILED: $($record.failure)"; exit 1 }
Write-Output 'WINDOWS_SETTINGS_INVENTORY_OK'
