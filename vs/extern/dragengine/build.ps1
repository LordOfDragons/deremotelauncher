param (
    [Parameter(Mandatory=$true)][string]$ProjectDir,
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$OutputDir
)

Import-Module "$PSScriptRoot\..\..\shared.psm1"


$ExpandedDir = "$ProjectDir\DragengineSDK"
if (Test-Path $ExpandedDir) {
    Remove-Item $ExpandedDir -Force -Recurse
}

DownloadArtifactGithub -SourceDir $ProjectDir -FilenameArtifact "dragengine-sdk-nightly-windows64.zip" `
    -UrlPath "dragengine/releases/download/nightly"

DownloadArtifactGithub -SourceDir $ProjectDir -FilenameArtifact "dragengine-sdk-nightly-windows64.zip" `
    -UrlPath "dragengine/releases/download/nightly"

Expand-Archive -Path "$ProjectDir\dragengine-sdk-nightly-windows64.zip" `
    -DestinationPath "$ProjectDir"
