# The owner's native GitHub sign-in through `bpr tour connect` on Windows
# ([[BPR-1068]], AC44-12). This is the Windows counterpart of the Linux and
# macOS owner steps recorded on the card. The owner runs it in a visible
# console and enters the device code at https://github.com/login/device. Only
# the owner may enter that code.
#
# It runs from a frozen root, never from a checkout:
#
#   ROOT\commit.txt   the commit that ROOT\src was exported from
#   ROOT\src          a `git archive` of that commit, which contains this script
#   ROOT\install      the TourCLI install (made by `prepare`)
#   ROOT\result.txt   what `signin` observed (made by `signin`)
#
# prepare  Enters the x64 MSVC environment, configures the `tour-cli` preset
#          (tests off, Release) in ROOT\src, builds it, and installs the
#          TourCLI component into ROOT\install. The TourCLI install carries no
#          Qt on Windows, so windeployqt puts QtCore, QtNetwork and their
#          plugins beside bpr.exe. That is a test arrangement for this run and
#          not a package. It records bpr.exe's SHA-256 and checks that
#          `tour capabilities` starts. It makes no GitHub request. The build log
#          is ROOT\prepare.log.
#
# signin   Checks bpr.exe against the recorded digest. It then uses a fresh
#          data root under %TEMP% that does not exist beforehand, so `bpr`
#          creates it privately. In that root it runs:
#            1. `tour capabilities --json`;
#            2. `tour connect --host github.com --transport native`, in this
#               console, unpiped. The device code appears only here;
#            3. a 120 s bounded `tour open` of MikeMarcin/BPR-Test#1 (a read;
#               nothing is written to GitHub), in a following process.
#          Then it removes the data root.
#
# Credential Manager. Only the target names of BetterPullReview's own generic
# credentials (`org.betterpullreview.github-native-*`) are read, never a
# secret. A fresh data root signs in as its Default profile, and a Default
# profile's targets carry no profile component. So if a BetterPullReview
# desktop on this machine is signed in on its Default profile, the new grant
# would overwrite the desktop's. If any production Default-profile target
# already exists, `signin` stops before it asks GitHub for anything and prints
# SIGNIN_STOPPED. Otherwise, on every exit, it deletes exactly the targets that
# were not there before it started, and checks that the set left is the set it
# found. Deleting the local grant does not revoke it on GitHub. The grant stays
# listed under the account's authorized OAuth apps, as it did after the Linux
# and macOS runs. This is an owner-attended live sign-in, not a test
# fixture: it uses the production service on purpose, because that is the
# vault being qualified. tests/AGENTS.md keeps fixtures off it, and the exact
# before/after deletion is the narrow cleanup that rule asks for.
#
# result.txt holds exit codes, durations, counts and named non-secret fields.
# It never holds the device code, a token or a credential.
param(
    [Parameter(Mandatory, Position = 0)] [ValidateSet('prepare', 'signin')] [string]$Phase,
    # The shared dependency cache of the station's checkout, so Qt and the
    # pinned dependencies are not downloaded again. With no such directory the
    # root keeps its own cache.
    [string]$DependencyCache = 'C:\code\git\BetterPullReview\build\.cpm-cache'
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$Source = Join-Path $Root 'src'
$Install = Join-Path $Root 'install'
$Bpr = Join-Path $Install 'bin\bpr.exe'
$DigestFile = Join-Path $Root 'install.sha256'
$PrepareLog = Join-Path $Root 'prepare.log'
$ResultFile = Join-Path $Root 'result.txt'
$PullRequest = 'https://github.com/MikeMarcin/BPR-Test/pull/1'
$OpenBoundSeconds = 120

if (-not (Test-Path -LiteralPath (Join-Path $Source 'CMakePresets.json')) -or (Test-Path -LiteralPath (Join-Path $Source '.git'))) {
    Write-Host "NOT_A_FROZEN_ROOT: $Source must be a git archive export (no .git) with CMakePresets.json."
    exit 2
}
$Commit = 'unknown'
if (Test-Path -LiteralPath (Join-Path $Root 'commit.txt')) {
    $Commit = (Get-Content -LiteralPath (Join-Path $Root 'commit.txt') -TotalCount 1).Trim()
}

function Write-Logged([string]$Text) {
    Add-Content -LiteralPath $PrepareLog -Value $Text -Encoding UTF8
    Write-Host $Text
}

# Runs a native command, copying its output to the console and the log, and
# returns its exit code. Native standard error is merged as text: with 'Stop',
# Windows PowerShell would turn its first line into a terminating error.
function Invoke-Logged([string]$Name, [string]$Executable, [string[]]$Arguments) {
    Write-Logged "== $Name"
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $Executable @Arguments 2>&1 | ForEach-Object {
            $line = "$_"
            Add-Content -LiteralPath $PrepareLog -Value $line -Encoding UTF8
            Write-Host $line
        }
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    Write-Logged "== $Name exit $code"
    return $code
}

function Get-Field($Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Invoke-Prepare {
    if (Test-Path -LiteralPath $Install) {
        Write-Host "PREPARE_REFUSED: $Install already exists. Remove the whole root and export it again."
        exit 2
    }
    Set-Content -LiteralPath $PrepareLog -Value "bpr1068 windows owner sign-in prepare, commit $Commit, $((Get-Date).ToUniversalTime().ToString('u'))" -Encoding UTF8

    # Keep a CMake already on PATH: the developer shell puts Visual Studio's
    # bundled CMake first, which may be older than the presets require.
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($null -eq (Get-Command cl -ErrorAction SilentlyContinue)) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            Write-Logged 'PREPARE_FAILED: vswhere.exe was not found, so Visual Studio cannot be located.'
            exit 1
        }
        $visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $visualStudio) {
            Write-Logged 'PREPARE_FAILED: no Visual Studio with the x64 C++ tools.'
            exit 1
        }
        Import-Module (Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
        Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
        Write-Logged "Visual Studio: $visualStudio (x64)"
    }
    if ($null -eq $cmakeCommand) { $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue }
    if ($null -eq $cmakeCommand -or $null -eq (Get-Command ninja -ErrorAction SilentlyContinue)) {
        Write-Logged 'PREPARE_FAILED: cmake or ninja is not on PATH.'
        exit 1
    }
    $cmake = $cmakeCommand.Source
    Write-Logged "CMake: $cmake"

    $cache = Join-Path $Root 'cpm-cache'
    if ($DependencyCache -and (Test-Path -LiteralPath $DependencyCache)) { $cache = $DependencyCache }
    Write-Logged "Dependency cache: $cache"

    Push-Location -LiteralPath $Source
    try {
        if ((Invoke-Logged 'configure' $cmake @('--preset', 'tour-cli', "-DCPM_SOURCE_CACHE=$($cache.Replace('\', '/'))")) -ne 0) {
            Write-Logged "PREPARE_FAILED: configure. See $PrepareLog"; exit 1
        }
        if ((Invoke-Logged 'build' $cmake @('--build', '--preset', 'tour-cli')) -ne 0) {
            Write-Logged "PREPARE_FAILED: build. See $PrepareLog"; exit 1
        }
        if ((Invoke-Logged 'install' $cmake @('--install', 'build/tour-cli', '--component', 'TourCLI', '--prefix', $Install)) -ne 0) {
            Write-Logged "PREPARE_FAILED: install. See $PrepareLog"; exit 1
        }
    } finally {
        Pop-Location
    }
    if (-not (Test-Path -LiteralPath $Bpr)) {
        Write-Logged "PREPARE_FAILED: the install has no $Bpr"; exit 1
    }

    $qtLine = Select-String -LiteralPath (Join-Path $Source 'build\tour-cli\CMakeCache.txt') -Pattern '^Qt6_DIR:[A-Z]+=(.+)$' | Select-Object -First 1
    if ($null -eq $qtLine) {
        Write-Logged 'PREPARE_FAILED: Qt6_DIR is not in the build cache.'; exit 1
    }
    $qtPrefix = (Resolve-Path -LiteralPath (Join-Path $qtLine.Matches[0].Groups[1].Value.Trim() '..\..\..')).Path
    $windeployqt = Join-Path $qtPrefix 'bin\windeployqt.exe'
    Write-Logged "Qt: $qtPrefix"
    if ((Invoke-Logged 'windeployqt' $windeployqt @('--release', '--no-translations', '--no-compiler-runtime', $Bpr)) -ne 0) {
        Write-Logged "PREPARE_FAILED: windeployqt. See $PrepareLog"; exit 1
    }

    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $capabilities = & $Bpr tour capabilities --json 2>$null
    $code = $LASTEXITCODE
    $ErrorActionPreference = $previous
    $platform = $null
    try { $platform = Get-Field (Get-Field ($capabilities -join "`n" | ConvertFrom-Json) 'data') 'platform' } catch { $platform = $null }
    if ($code -ne 0 -or (Get-Field $platform 'os') -ne 'windows') {
        Write-Logged "PREPARE_FAILED: bpr.exe tour capabilities exited $code. See $PrepareLog"; exit 1
    }
    $digest = (Get-FileHash -LiteralPath $Bpr -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath $DigestFile -Value $digest -Encoding ASCII
    Write-Logged "PREPARED $digest commit $Commit"
}

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace BprOwnerSignin {
public static class CredentialTargets {
    const int CredTypeGeneric = 1;
    const int ErrorNotFound = 1168;
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool CredEnumerateW(string filter, int flags, out int count, out IntPtr credentials);
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool CredDeleteW(string target, int type, int flags);
    [DllImport("advapi32.dll")]
    static extern void CredFree(IntPtr buffer);
    // Reads TargetName and Type only: CREDENTIALW begins DWORD Flags, DWORD
    // Type, LPWSTR TargetName. The blob is never read and is freed at once.
    public static string[] List(string filter) {
        int count; IntPtr credentials;
        if (!CredEnumerateW(filter, 0, out count, out credentials)) {
            int error = Marshal.GetLastWin32Error();
            if (error == ErrorNotFound) return new string[0];
            throw new Win32Exception(error);
        }
        try {
            var names = new List<string>();
            for (int index = 0; index < count; ++index) {
                IntPtr credential = Marshal.ReadIntPtr(credentials, index * IntPtr.Size);
                if (Marshal.ReadInt32(credential, 4) != CredTypeGeneric) continue;
                names.Add(Marshal.PtrToStringUni(Marshal.ReadIntPtr(credential, 8)));
            }
            names.Sort(StringComparer.Ordinal);
            return names.ToArray();
        } finally {
            CredFree(credentials);
        }
    }
    public static bool Delete(string target) { return CredDeleteW(target, CredTypeGeneric, 0); }
}
}
'@

function Invoke-Signin {
    $TargetFilter = 'org.betterpullreview.github-native-*'
    $DefaultPrefixes = @(
        'org.betterpullreview.github-native-credential.windows-credential-manager.v2/',
        'org.betterpullreview.github-native-refresh.windows-credential-manager.v1/'
    )
    $result = New-Object System.Collections.Generic.List[string]
    $result.Add('bpr1068 windows owner sign-in, result v1')
    $result.Add("started_utc=$((Get-Date).ToUniversalTime().ToString('u'))")
    $result.Add("commit=$Commit")
    $os = Get-CimInstance Win32_OperatingSystem
    $result.Add("windows=$($os.Caption) $($os.Version) build $($os.BuildNumber)")

    if (-not (Test-Path -LiteralPath $Bpr) -or -not (Test-Path -LiteralPath $DigestFile)) {
        Write-Host "SIGNIN_NOT_READY: run prepare first ($Bpr)."; exit 2
    }
    $expected = (Get-Content -LiteralPath $DigestFile -TotalCount 1).Trim()
    $actual = (Get-FileHash -LiteralPath $Bpr -Algorithm SHA256).Hash.ToLowerInvariant()
    $result.Add("bpr_sha256=$actual")
    if ($actual -ne $expected) {
        Write-Host "SIGNIN_NOT_READY: bpr.exe is $actual, prepare recorded $expected."; exit 2
    }

    $session = Join-Path $env:TEMP ('bpr1068-signin-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $session | Out-Null
    $data = Join-Path $session 'data'
    $before = @([BprOwnerSignin.CredentialTargets]::List($TargetFilter))
    $outcome = 'SIGNIN_DONE'
    try {
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $capabilities = & $Bpr tour capabilities --json 2>$null
        $code = $LASTEXITCODE
        $ErrorActionPreference = $previous
        $result.Add("capabilities_exit=$code")
        try {
            $platform = Get-Field (Get-Field ($capabilities -join "`n" | ConvertFrom-Json) 'data') 'platform'
            foreach ($name in 'os', 'native_vault', 'gh_transport', 'package', 'qualification') {
                $result.Add("platform_$name=$(Get-Field $platform $name)")
            }
        } catch {
            $result.Add('platform=unparsed')
        }

        $result.Add("vault_targets_before=$($before.Count)")
        $defaults = @($before | Where-Object { $target = $_; @($DefaultPrefixes | Where-Object { $target.StartsWith($_, [StringComparison]::Ordinal) }).Count -gt 0 })
        $result.Add("vault_default_profile_targets_before=$($defaults.Count)")
        if ($defaults.Count -gt 0) {
            $result.Add('stopped=a BetterPullReview Default-profile sign-in already exists here; nothing was requested from GitHub')
            $outcome = 'SIGNIN_STOPPED'
            return
        }

        Write-Host ''
        Write-Host 'Open https://github.com/login/device in a browser and enter the code that bpr prints below.'
        Write-Host 'Enter it only there. Do not paste it anywhere else.'
        Write-Host ''
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        & $Bpr tour connect --host github.com --transport native --data-root $data
        $connectExit = $LASTEXITCODE
        $ErrorActionPreference = $previous
        $result.Add("connect_exit=$connectExit")
        $result.Add(('connect_seconds={0:N1}' -f $clock.Elapsed.TotalSeconds))
        $connected = @([BprOwnerSignin.CredentialTargets]::List($TargetFilter))
        $result.Add("vault_targets_added_by_connect=$(@($connected | Where-Object { $before -cnotcontains $_ }).Count)")
        if ($connectExit -ne 0) {
            $outcome = 'SIGNIN_INCOMPLETE'
            return
        }

        Write-Host ''
        Write-Host "Reading $PullRequest in a new process (at most $OpenBoundSeconds s)..."
        $openOut = Join-Path $session 'open.json'
        $openErr = Join-Path $session 'open.err'
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $Bpr -ArgumentList @('tour', 'open', $PullRequest, '--json', '--data-root', "`"$data`"") -NoNewWindow -PassThru -RedirectStandardOutput $openOut -RedirectStandardError $openErr
        $null = $process.Handle
        if (-not $process.WaitForExit($OpenBoundSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
            $result.Add("open_exit=timeout after $OpenBoundSeconds s (stopped)")
            $outcome = 'SIGNIN_INCOMPLETE'
        } else {
            $process.WaitForExit()
            $result.Add("open_exit=$($process.ExitCode)")
            if ($process.ExitCode -ne 0) { $outcome = 'SIGNIN_INCOMPLETE' }
        }
        $result.Add(('open_seconds={0:N1}' -f $clock.Elapsed.TotalSeconds))
        try {
            $opened = Get-Content -LiteralPath $openOut -Raw | ConvertFrom-Json
            $result.Add("open_status=$(Get-Field $opened 'status')")
            $error_ = Get-Field $opened 'error'
            if ($null -ne $error_) { $result.Add("open_error_code=$(Get-Field $error_ 'code')") }
            $openData = Get-Field $opened 'data'
            foreach ($name in 'complete', 'remote_stale', 'read_only', 'published', 'profile_id', 'account_id', 'host', 'repository', 'number', 'revision', 'local_state') {
                $value = Get-Field $openData $name
                if ($null -ne $value) { $result.Add("open_$name=$value") }
            }
        } catch {
            $result.Add('open_json=unparsed')
        }
        if ($outcome -ne 'SIGNIN_DONE' -and (Test-Path -LiteralPath $openErr)) {
            $text = (Get-Content -LiteralPath $openErr -Raw)
            if ($text) {
                $text = $text.Trim()
                if ($text.Length -gt 400) { $text = $text.Substring($text.Length - 400) }
                $result.Add("open_stderr_tail=$($text -replace '\s+', ' ')")
            }
        }
    } finally {
        $after = @([BprOwnerSignin.CredentialTargets]::List($TargetFilter))
        $added = @($after | Where-Object { $before -cnotcontains $_ })
        $deleted = 0
        foreach ($target in $added) {
            if ([BprOwnerSignin.CredentialTargets]::Delete($target)) { ++$deleted }
        }
        $final = @([BprOwnerSignin.CredentialTargets]::List($TargetFilter))
        $same = ($final.Count -eq $before.Count) -and (@($final | Where-Object { $before -cnotcontains $_ }).Count -eq 0)
        $result.Add("cleanup_vault_targets_added=$($added.Count)")
        $result.Add("cleanup_vault_targets_deleted=$deleted")
        $result.Add("cleanup_vault_targets_after=$($final.Count)")
        $result.Add("cleanup_vault_matches_before=$same")
        try {
            Remove-Item -LiteralPath $session -Recurse -Force
        } catch {
        }
        $result.Add("cleanup_session_removed=$(-not (Test-Path -LiteralPath $session))")
        $result.Add("finished_utc=$((Get-Date).ToUniversalTime().ToString('u'))")
        $result.Add("outcome=$outcome")
        Set-Content -LiteralPath $ResultFile -Value $result -Encoding UTF8
        Write-Host ''
        $result | ForEach-Object { Write-Host $_ }
        Write-Host "$outcome result: $ResultFile"
    }
}

if ($Phase -eq 'prepare') { Invoke-Prepare } else { Invoke-Signin }
