# PitchShifter installer for Windows (Linux/macOS: install.sh).
# Installs the release build from GitHub. The Lua/IUP front panel is not shipped for Windows.
#
#   irm https://raw.githubusercontent.com/bacteriafield/pitchshifter/main/install.ps1 | iex
#   .\install.ps1 -Version v0.0.1 -Prefix C:\PitchShifter
#
param(
    [string]$Version = 'latest',
    [string]$Prefix = "$env:LOCALAPPDATA\PitchShifter"
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # the progress bar makes Invoke-WebRequest very slow

$repo = 'bacteriafield/pitchshifter'
$name = 'pitchshifter-windows-x86_64'
if ($Version -eq 'latest') {
    $url = "https://github.com/$repo/releases/latest/download/$name.zip"
} else {
    $url = "https://github.com/$repo/releases/download/$Version/$name.zip"
}

Write-Host "==> Downloading $url"
$tmp = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
New-Item -ItemType Directory $tmp | Out-Null
try {
    Invoke-WebRequest $url -OutFile "$tmp\$name.zip"
    Expand-Archive "$tmp\$name.zip" $tmp

    Write-Host "==> Installing to $Prefix"
    New-Item -ItemType Directory -Force $Prefix | Out-Null
    Copy-Item "$tmp\$name\*" $Prefix -Recurse -Force
} finally {
    Remove-Item $tmp -Recurse -Force
}

$bin = "$Prefix\bin"
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if (($userPath -split ';') -notcontains $bin) {
    [Environment]::SetEnvironmentVariable('Path', "$userPath;$bin".TrimStart(';'), 'User')
    Write-Host "Added $bin to your PATH (open a new terminal)."
}
Write-Host 'Done. Run: PitchShifter'
