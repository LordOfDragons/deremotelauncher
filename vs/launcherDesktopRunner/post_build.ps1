param (
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [Parameter(Mandatory=$true)][string]$SolutionDir
)

Import-Module "$PSScriptRoot\..\shared.psm1"

# application
$TargetDir = "$OutputDir\DERemoteLauncher"
#if (Test-Path $TargetDir) {
#    Remove-Item $TargetDir -Force -Recurse
#}

Write-Host "DERemoteLauncher-Runner: Copy application to '$TargetDir'"
Install-Files -Path "$OutputDir\DERemoteLauncherRunner.exe" -Destination $TargetDir
