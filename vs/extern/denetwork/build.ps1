param (
    [Parameter(Mandatory=$true)][string]$ProjectDir,
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$OutputDir
)

Import-Module "$PSScriptRoot\..\..\shared.psm1"

$DENetworkVersion = "1.2"

$ExpandedDir = "$ProjectDir\build"
if (Test-Path $ExpandedDir) {
    Remove-Item $ExpandedDir -Force -Recurse
}

$ArchiveFile = "$ProjectDir\DENetworkSDK-$DENetworkVersion.zip"

if (!(Test-Path $ArchiveFile)) {
    DownloadArtifactGithub -SourceDir $ProjectDir -FilenameArtifact "DENetworkSDK-$DENetworkVersion.zip" `
        -UrlPath "denetwork/releases/download/v$DENetworkVersion"
}

Expand-Archive -Path $ArchiveFile -DestinationPath $ExpandedDir
