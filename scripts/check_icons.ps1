# Check that Windows files carry the app icon (used by the CI and release workflows). Run with pwsh or powershell.
#   check_icons.ps1 -Files "a.exe;b.vst3" [-Vst3Folder C:\...\JPurrr-8000.vst3]
# Files are separated by semicolons. Every file must have at least one icon resource; the VST3 folder must hold Plugin.ico and desktop.ini.
param(
    [Parameter(Mandatory = $true)][string]$Files,
    [string]$Vst3Folder = ''
)
$ErrorActionPreference = 'Stop'
# Failures are also written as GitHub annotations (the CI log itself cannot always be downloaded).
try {
Add-Type -Namespace JP -Name Native -MemberDefinition '[DllImport("shell32.dll", CharSet = CharSet.Unicode)] public static extern uint ExtractIconEx(string file, int index, IntPtr[] large, IntPtr[] small, uint count);'
foreach ($f in ($Files -split ';')) {
    $path = (Resolve-Path $f).Path
    $n = [JP.Native]::ExtractIconEx($path, -1, $null, $null, 0)
    if ($n -lt 1) { throw "no icon resource in $path" }
    Write-Host "OK: $path has $n icon resource(s)"
}
if ($Vst3Folder -ne '') {
    foreach ($name in 'Plugin.ico', 'desktop.ini') {
        $p = Join-Path $Vst3Folder $name
        if (-not (Test-Path -LiteralPath $p)) { throw "missing $p" }
    }
    $attr = (Get-Item -LiteralPath $Vst3Folder -Force).Attributes
    if (($attr -band [IO.FileAttributes]::System) -eq 0 -and ($attr -band [IO.FileAttributes]::ReadOnly) -eq 0) {
        Write-Host "::warning title=VST3 folder icon::$Vst3Folder has neither the System nor the Read-only attribute, Explorer will not show its icon"
    }
    Write-Host "OK: $Vst3Folder has Plugin.ico and desktop.ini (attributes: $attr)"
}
} catch {
    $msg = ($_ | Out-String).Trim() -replace "`r?`n", ' | '
    Write-Host "::error title=icon check::$msg"
    if ($Vst3Folder -ne '' -and (Test-Path -LiteralPath $Vst3Folder)) {
        $root = (Resolve-Path -LiteralPath $Vst3Folder).Path
        $list = (Get-ChildItem -LiteralPath $root -Force -Recurse | ForEach-Object { $_.FullName.Substring($root.Length) + ' [' + $_.Attributes + ']' }) -join ' | '
        Write-Host "::notice title=VST3 folder::$list"
    }
    exit 1
}
