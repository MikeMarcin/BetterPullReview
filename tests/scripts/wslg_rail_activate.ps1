# Native WSLg window activation for the Linux Settings observers (BPR-0975).
#
# WSLg's Weston compositor gives keyboard focus to the surface whose RAIL
# window Windows made foreground, and its XWayland window manager puts X11
# input focus back on that surface whenever an X client moves it elsewhere.
# So an X11 observer cannot focus one of two mapped windows by itself. This
# helper performs the platform's own activation instead: it asks Windows to
# make the exact RAIL window foreground, as the taskbar or Alt-Tab would.
#
# It runs in the interactive Windows session through WSL interop. It never
# injects keyboard or pointer input, attaches thread input, or changes any
# foreground-lock, display or security setting. A caption is matched exactly
# and only on RAIL_WINDOW top-level windows owned by msrdc.exe; the caller
# must still prove that X11 focus reached the exact owned target.
#
# Output is one JSON object: the exact candidates, the current foreground
# window and, with -Activate, the activated handle and the foreground window
# before and after. The `foreground` field names a window's title only for an
# msrdc RAIL_WINDOW, so a capture never records another application's title
# (BPR-0975's activation probe reads it to see whether Windows followed WSLg).
param(
	[Parameter(Mandatory = $true)][string]$Title,
	[switch]$Activate,
	[long]$Handle = 0
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class BprRail {
	delegate bool Visitor(IntPtr hwnd, IntPtr parameter);
	[DllImport("user32.dll")] static extern bool EnumWindows(Visitor callback, IntPtr parameter);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, StringBuilder text, int length);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int length);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
	[DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hwnd);
	[DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr hwnd, int command);
	[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
	[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
	[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint process);
	public static string ClassOf(IntPtr hwnd) { var text = new StringBuilder(256); GetClassName(hwnd, text, text.Capacity); return text.ToString(); }
	public static string TitleOf(IntPtr hwnd) { var text = new StringBuilder(1024); GetWindowText(hwnd, text, text.Capacity); return text.ToString(); }
	public static uint ProcessOf(IntPtr hwnd) { uint process; GetWindowThreadProcessId(hwnd, out process); return process; }
	public static List<IntPtr> Rail(string title) {
		var found = new List<IntPtr>();
		EnumWindows((hwnd, unused) => {
			if (IsWindowVisible(hwnd) && ClassOf(hwnd) == "RAIL_WINDOW" && TitleOf(hwnd) == title) { found.Add(hwnd); }
			return true;
		}, IntPtr.Zero);
		return found;
	}
}
'@
function Describe([IntPtr]$Window) {
	$process = [BprRail]::ProcessOf($Window)
	$name = (Get-Process -Id $process -ErrorAction SilentlyContinue).ProcessName
	[ordered]@{ handle = $Window.ToInt64(); class = [BprRail]::ClassOf($Window); title = [BprRail]::TitleOf($Window); process = $name }
}
function Foreground {
	$window = [BprRail]::GetForegroundWindow()
	if ($window -eq [IntPtr]::Zero) { return $null }
	$description = Describe $window
	if (-not ($description.class -eq 'RAIL_WINDOW' -and $description.process -eq 'msrdc')) { $description.title = $null }
	$description
}
$result = [ordered]@{ title = $Title; candidates = @(); foreground = $null; activated = $null; before = $null; after = $null; error = $null }
try {
	$candidates = @([BprRail]::Rail($Title) | Where-Object { (Describe $_).process -eq 'msrdc' })
	$result.candidates = @($candidates | ForEach-Object { $_.ToInt64() })
	$result.foreground = Foreground
	if ($Activate) {
		$target = if ($Handle -ne 0) { @($candidates | Where-Object { $_.ToInt64() -eq $Handle }) } else { $candidates }
		if ($target.Count -ne 1) { throw "Expected one exact msrdc RAIL window to activate, found $($target.Count)" }
		$window = $target[0]
		$result.before = Describe ([BprRail]::GetForegroundWindow())
		if ([BprRail]::IsIconic($window)) { [BprRail]::ShowWindowAsync($window, 9) | Out-Null }
		if (-not [BprRail]::SetForegroundWindow($window)) { throw 'Windows refused the RAIL foreground request' }
		$deadline = [DateTime]::UtcNow.AddSeconds(2)
		while ([BprRail]::GetForegroundWindow() -ne $window -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 50 }
		$result.after = Describe ([BprRail]::GetForegroundWindow())
		if ([BprRail]::GetForegroundWindow() -ne $window) { throw 'The RAIL window did not become foreground' }
		$result.activated = $window.ToInt64()
	}
} catch {
	$result.error = $_.Exception.Message
}
$result | ConvertTo-Json -Compress -Depth 4
