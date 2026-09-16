$ErrorActionPreference = 'Stop'
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

# Chocolatey shims bin\PitchShifter.exe; its DLLs stay next to it
Install-ChocolateyZipPackage -PackageName $env:ChocolateyPackageName `
    -Url64bit 'https://github.com/bacteriafield/pitchshifter/releases/download/v@VERSION@/pitchshifter-windows-x86_64.zip' `
    -Checksum64 '@SHA256_WINDOWS@' -ChecksumType64 'sha256' `
    -UnzipLocation $toolsDir
