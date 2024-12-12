param (
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [Parameter(Mandatory=$true)][string]$SolutionDir
)

Import-Module "$PSScriptRoot\..\shared.psm1"

# application
$TargetDir = "$OutputDir\DERemoteLauncherDesktop\DERemoteLauncherDesktop"
#if (Test-Path $TargetDir) {
#    Remove-Item $TargetDir -Force -Recurse
#}

Write-Host "DERemoteLauncher: Copy application to '$TargetDir'"
Install-Files -Path "$OutputDir\DERemoteLauncher.exe" -Destination $TargetDir
Install-Files -Path "$SolutionDir\extern\fox\fox-1.7.81-vc64\lib\fox-1.7.dll" -Destination $TargetDir
