param (
    [Parameter(Mandatory=$true)][string]$ProjectDir,
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$OutputDir
)

Import-Module "$PSScriptRoot\..\..\shared.psm1"


$ExpandedDir = Join-Path -Path $ProjectDir -ChildPath "fox-1.7.81-vc64"
if (Test-Path $ExpandedDir) {
    Remove-Item $ExpandedDir -Force -Recurse
}

DownloadArtifact -SourceDir $ProjectDir -FilenameArtifact "fox-1.7.81-vc64.zip" -UrlPath "fox"

$ArchiveFile = "$ProjectDir\fox-1.7.81-vc64.zip"

# workaround for powershell bug under github
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory($ArchiveFile, $ExpandedDir)
