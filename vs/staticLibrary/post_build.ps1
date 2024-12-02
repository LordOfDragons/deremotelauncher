param (
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [Parameter(Mandatory=$true)][string]$ProjectDir
)

Import-Module "$PSScriptRoot\..\shared.psm1"

# headers
$TargetDir = "$OutputDir\SDK\Include\deremotelauncher"
if (Test-Path $TargetDir) {
    Remove-Item $TargetDir -Force -Recurse
}

Write-Host "DERemoteLauncher: Copy Headers to '$TargetDir'"
Copy-Files -SourceDir $SourceDir -TargetDir $TargetDir -Pattern "*.h"
Copy-Item -Path "$ProjectDir\config.h" -Destination $TargetDir

# library
$TargetDir = "$OutputDir\SDK\Library\x64"
Write-Host "DERemoteLauncher: Copy Library to '$TargetDir'"
Install-Files -Path "$OutputDir\DERemoteLauncher.lib" -Destination $TargetDir

# pdb
$TargetDir = "$OutputDir\SDK\Library\x64"
Write-Host "DERemoteLauncher: Copy PDB to '$TargetDir'"
Install-Files -Path "$OutputDir\DERemoteLauncher.pdb" -Destination $TargetDir

# info
$TargetDir = "$OutputDir\SDK"
Write-Host "DERemoteLauncher: Copy Info to '$TargetDir'"
Copy-Item -Path "$SourceDir\..\..\LICENSE" -Destination $TargetDir
Copy-Item -Path "$ProjectDir\..\..\README.md" -Destination $TargetDir
