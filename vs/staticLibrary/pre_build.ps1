param (
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$ProjectDir
)

Import-Module "$PSScriptRoot\..\shared.psm1"

Copy-Item -Path "$ProjectDir\config.h" -Destination $SourceDir
