param (
    [Parameter(Mandatory=$true)][string]$ProjectDir,
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$OutputDir
)

Import-Module "$PSScriptRoot\..\..\shared.psm1"


$ExpandedDir = "$ProjectDir\DENetworkSDK"
if (Test-Path $ExpandedDir) {
    Remove-Item $ExpandedDir -Force -Recurse
}

if (!(Test-Path "$ProjectDir\DENetworkSDK-nightly.zip")) {
    DownloadArtifactGithub -SourceDir $ProjectDir -FilenameArtifact "DENetworkSDK-nightly.zip" `
        -UrlPath "denetwork/releases/download/nightly"
}

Expand-Archive -Path "$ProjectDir\DENetworkSDK-nightly.zip" -DestinationPath "$ProjectDir"
