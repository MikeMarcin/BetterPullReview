# External UI Automation course for a tests-disabled, signed-out native package.
# The caller supplies an already isolated process. No setting, sign-in, update,
# export or maintenance command is invoked; editable values are never captured.
param([int]$ApplicationPid, [string]$Executable, [string]$Revision, [string]$Output)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, WindowsBase
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SettingsNative {
 [StructLayout(LayoutKind.Sequential)] public struct Point { public int x, y; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr hwnd, ref Point point);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
}
'@
$ae = [Windows.Automation.AutomationElement]
$scope = [Windows.Automation.TreeScope]
$walker = [Windows.Automation.TreeWalker]::RawViewWalker
$trueCondition = [Windows.Automation.Condition]::TrueCondition
$process = Get-Process -Id $ApplicationPid
if ([IO.Path]::GetFullPath($process.Path) -ne [IO.Path]::GetFullPath($Executable)) { throw 'PID executable mismatch' }
if (Test-Path $Output) { throw 'Capture output already exists' }
$checks = [Collections.Generic.List[string]]::new()

function Require($condition, $message) { if (-not $condition) { throw $message } }
function Wait-For([string]$message, [scriptblock]$predicate) {
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $predicate) { return }
        Start-Sleep -Milliseconds 80
    }
    throw $message
}
function Windows {
    $condition = [Windows.Automation.PropertyCondition]::new($ae::ProcessIdProperty, $ApplicationPid)
    @($ae::RootElement.FindAll($scope::Children, $condition) | Where-Object { -not $_.Current.IsOffscreen })
}
function Workbenches { @(Windows | Where-Object { $_.Current.Name.StartsWith('BetterPullReview') -and $_.Current.Name.EndsWith('Default profile') }) }
function Settings { @(Windows | Where-Object { $_.Current.Name.StartsWith('Settings ') -and $_.Current.Name.EndsWith('Default profile') }) }
function Handle($element) { [IntPtr]$element.Current.NativeWindowHandle }
function Focus($element) {
    [SettingsNative]::SetForegroundWindow((Handle $element)) | Out-Null
    Wait-For 'Foreground focus refused' { [SettingsNative]::GetForegroundWindow() -eq (Handle $element) }
    Start-Sleep -Milliseconds 200
}
function Key([byte]$code, [bool]$control = $false) {
    $foreground = [SettingsNative]::GetForegroundWindow()
    Require (@(Windows | Where-Object { (Handle $_) -eq $foreground }).Count -gt 0) 'Refuse key outside owned process'
    if ($control) { [SettingsNative]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero) }
    [SettingsNative]::keybd_event($code, 0, 0, [UIntPtr]::Zero)
    [SettingsNative]::keybd_event($code, 0, 2, [UIntPtr]::Zero)
    if ($control) { [SettingsNative]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero) }
    Start-Sleep -Milliseconds 150
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
    $point = New-Object SettingsNative+Point
    $point.x = [int]($rect.X + $rect.Width / 2)
    $point.y = [int]($rect.Y + $rect.Height / 2)
    [SettingsNative]::ScreenToClient((Handle $window), [ref]$point) | Out-Null
    $position = [IntPtr](($point.y -shl 16) -bor ($point.x -band 65535))
    Require ([SettingsNative]::PostMessage((Handle $window), 0x201, [IntPtr]1, $position)) 'Mouse down refused'
    Require ([SettingsNative]::PostMessage((Handle $window), 0x202, [IntPtr]0, $position)) 'Mouse up refused'
    Start-Sleep -Milliseconds 150
}
function Menu-Item([string]$name) {
    $found = @()
    foreach ($window in (Windows)) {
        $found += @($window.FindAll($scope::Descendants, $trueCondition) | Where-Object {
            $_.Current.ControlType -eq [Windows.Automation.ControlType]::MenuItem -and
            ($_.Current.Name.Replace('&','') -split "`t")[0] -eq $name -and -not $_.Current.IsOffscreen
        })
    }
    if ($found.Count -ne 1) {
        $visible = @()
        foreach ($window in (Windows)) {
            $visible += @($window.FindAll($scope::Descendants, $trueCondition) | Where-Object { $_.Current.ControlType -eq [Windows.Automation.ControlType]::MenuItem -and -not $_.Current.IsOffscreen } | ForEach-Object { $_.Current.Name })
        }
        throw "Expected one visible menu item $name, got $($found.Count); observed=$($visible -join '|')"
    }
    $found[0]
}
function Menu-Command($window, [string]$menu, [string]$item) {
    Focus $window
    Click (Find $window 'Application menu') $window
    $parent = Menu-Item $menu
    $expand = $null
    if ($parent.TryGetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern, [ref]$expand)) {
        $expand.Expand()
    } else {
        $parent.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    }
    Start-Sleep -Milliseconds 150
    $command = Menu-Item $item
    Write-Output "MENU_ITEM:$item ACCELERATOR:$($command.Current.AcceleratorKey) ACCESS:$($command.Current.AccessKey)"
    $command.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
}
function Close($window) { $window.GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close() }
function Nodes($element, [int]$depth = 0) {
    Require ($depth -lt 32) 'Unbounded accessibility depth'
    $current = $element.Current
    if ($current.IsOffscreen) { return }
    $state = ''
    $toggle = $null
    if ($element.TryGetCurrentPattern([Windows.Automation.TogglePattern]::Pattern, [ref]$toggle)) { $state = $toggle.Current.ToggleState.ToString() }
    [ordered]@{depth=$depth; type=$current.ControlType.ProgrammaticName.Replace('ControlType.',''); name=$current.Name; enabled=$current.IsEnabled; state=$state}
    $child = $walker.GetFirstChild($element)
    while ($null -ne $child) { Nodes $child ($depth + 1); $child = $walker.GetNextSibling($child) }
}

Wait-For 'Initial signed-out workbench absent' { (Workbenches).Count -eq 1 }
$first = (Workbenches)[0]
Menu-Command $first 'View' ('Settings' + [char]0x2026)
Wait-For 'Native Settings menu did not open one labelled window' { (Settings).Count -eq 1 }
$shared = (Settings)[0]
$identity = (Handle $shared)
Focus $first
Require ((Settings).Count -eq 1) 'Settings is not modeless'
Key 0xBC $true
try { Wait-For 'Ctrl-comma did not reuse/focus Settings' { (Settings).Count -eq 1 -and (Handle (Settings)[0]) -eq $identity -and [SettingsNative]::GetForegroundWindow() -eq $identity } } catch {
    throw "Ctrl-comma: count=$((Settings).Count), identity=$identity, actual=$((Settings | ForEach-Object { Handle $_ }) -join ','), foreground=$([SettingsNative]::GetForegroundWindow())"
}
$checks.Add('native-menu-ctrl-comma-modeless-singleton-profile-label')
Key 0x1B
Wait-For 'Escape did not close Settings' { (Settings).Count -eq 0 }
Focus $first
Key 0xBC $true
Wait-For 'Ctrl-comma did not reopen Settings' { (Settings).Count -eq 1 }
$shared = (Settings)[0]
$identity = Handle $shared
$checks.Add('escape-and-shortcut-reopen')
Menu-Command $first 'Profiles' 'New Window'
Wait-For 'New Window did not create same-profile sibling' { (Workbenches).Count -eq 2 }
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
$checks.Add('same-profile-sibling-menu-shortcut-and-retirement')
$shared = (Settings)[0]
$categories = @('General','Accounts & hosts','Reviews & queue','Appearance','Notifications','Agents & context','Storage & privacy')
$pages = @()
for ($index = 0; $index -lt $categories.Count; $index++) {
    $category = $categories[$index]
    Focus $shared
    $sidebar = Find $shared 'Settings pages'
    Click (Find $sidebar $category) $shared
    $page = Find $shared 'Settings page'
    Find $page $category | Out-Null
    $nodes = @(Nodes $page)
    Require ($nodes.Count -lt 2000) 'Unbounded page nodes'
    $pages += [ordered]@{index=$index; category=$category; nodes=$nodes}
}
$checks.Add('seven-categories-with-current-page-heading')
Menu-Command $second 'File' 'Quit'
Require ($process.WaitForExit(15000)) 'Native Quit did not stop process'
Require ($process.ExitCode -eq 0) 'Native Quit returned failure'
$checks.Add('native-quit-exit-zero')
[ordered]@{schema='bpr.settings-inventory.uia.v1'; build='windows-current-qt6112-release'; revision=$Revision; executable_sha256=(Get-FileHash $Executable -Algorithm SHA256).Hash.ToLower(); platform='Windows 11 10.0.26200 x64'; source='native UI Automation raw tree and user32 gestures'; categories=$categories; native_checks=$checks; pages=$pages} | ConvertTo-Json -Depth 20 | Set-Content -Encoding UTF8 $Output
Write-Output 'WINDOWS_SETTINGS_INVENTORY_OK'
