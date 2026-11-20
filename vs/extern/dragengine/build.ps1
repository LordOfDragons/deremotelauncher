param (
    [Parameter(Mandatory=$true)][string]$ProjectDir,
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$OutputDir
)

Import-Module "$PSScriptRoot\..\..\shared.psm1"

$DragengineVersion = "1.34.3"

$ExpandedDir = "$ProjectDir\build"
if (Test-Path $ExpandedDir) {
    Remove-Item $ExpandedDir -Force -Recurse
}

$ArchiveFile = "$ProjectDir\dragengine-sdk-$DragengineVersion-windows64.zip"

if (!(Test-Path $ArchiveFile)) {
    DownloadArtifactGithub -SourceDir $ProjectDir -FilenameArtifact "dragengine-sdk-$DragengineVersion-windows64.zip" `
        -UrlPath "dragengine/releases/download/v$DragengineVersion"
}

Expand-Archive -Path $ArchiveFile -DestinationPath $ExpandedDir
