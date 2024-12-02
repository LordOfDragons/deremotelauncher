# Shared Functions Module
###########################


# Install files to directory
##############################

function Install-Files {
    param (
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Destination,
        [Parameter(Mandatory=$false)][string]$Name
    )

    $Path = Resolve-Path $Path

    if (!(Test-Path $Destination)) {
        New-Item -ItemType Directory $Destination | Out-Null
    }

    if (!$Name) {
        $Name = Split-Path -Path $Path -Leaf
    }

    Copy-Item -Path $Path -Destination (Join-Path -Path $Destination -ChildPath $Name) -Force
}


# Get version from SConscript
###############################

function Get-Version {
    param (
        [Parameter(Mandatory=$true)][string]$Path
    )

    (Select-String -Path $Path -Pattern "versionString = '([0-9.]+)'").Matches.Groups[1].Value
}


# Copy files to directory
function Copy-Files {
    param (
        [Parameter(Mandatory=$true)][string]$SourceDir,
        [Parameter(Mandatory=$true)][string]$TargetDir,
        [Parameter(Mandatory=$true)][string]$Pattern,
        [Parameter(Mandatory=$false)][string]$Replace1Key,
        [Parameter(Mandatory=$false)][string]$Replace1Value
    )

    $SourceDir = Resolve-Path $SourceDir

    if (!(Test-Path $TargetDir)) {
        New-Item -ItemType Directory $TargetDir | Out-Null
    }

    $CutLength = $SourceDir.Length + 1

    Get-ChildItem -Path (Join-Path -Path $SourceDir -ChildPath $Pattern) -Recurse | ForEach-Object {
        $RelativePath = $_.FullName.Substring($CutLength)
        $TargetPath = Join-Path -Path $TargetDir -ChildPath $RelativePath
        $ParentPath = Split-Path -Path $TargetPath -Parent
        # Write-Host $RelativePath
        if (!(Test-Path $ParentPath)) {
            New-Item -ItemType Directory $ParentPath | Out-Null
        }
        
        if ($Replace1Key) {
            $Content = Get-Content -Raw -Path $_.FullName
            $Content = $Content -creplace "$Replace1Key","$Replace1Value"
            Set-Content -Path "$TargetDir\$RelativePath" -Value $Content

        }else{
            Copy-Item -Path $_.FullName -Destination "$TargetDir\$RelativePath" -Force
        }
    }
}


# Sanitize script input path
# --------------------------
# Visual Studio has the tendency to forget strip double quote from the end
# of path send to script files causing various failures. This function
# strips trailing double quote.
# 
# Furthermore such path can also contain a trailing backslash where there
# should be none. This function also strips those

function SanitizeScriptInputPath {
    param (
        [Parameter(Mandatory=$true)][string]$Path
    )

    if($Path.EndsWith('"')){
        $Path = $Path.Substring(0, $Path.Length - 1)
    }

    if($Path.EndsWith('\\')){
        $Path = $Path.Substring(0, $Path.Length - 1)
    }

    return $Path
}


# Download artifact if not present
# --------------------------------

function DownloadArtifact {
    param (
        [Parameter(Mandatory=$true)][string]$SourceDir,
        [Parameter(Mandatory=$true)][string]$FilenameArtifact,
        [Parameter(Mandatory=$true)][string]$UrlPath
    )

    if (!(Test-Path "$SourceDir\$FilenameArtifact")) {
        Invoke-WebRequest "$UrlExternArtifacts/$UrlPath/$FilenameArtifact" -OutFile "$SourceDir\$FilenameArtifact"
    }
}

function DownloadArtifactGithub {
    param (
        [Parameter(Mandatory=$true)][string]$SourceDir,
        [Parameter(Mandatory=$true)][string]$FilenameArtifact,
        [Parameter(Mandatory=$true)][string]$UrlPath
    )

    if (!(Test-Path "$SourceDir\$FilenameArtifact")) {
        Invoke-WebRequest "$UrlGithubArtifacts/$UrlPath/$FilenameArtifact" -OutFile "$SourceDir\$FilenameArtifact"
    }
}


# Various path constants
##########################

New-Variable -Name UrlExternArtifacts -Scope Global -Option ReadOnly -Force `
    -Value "https://dragondreams.s3.eu-central-1.amazonaws.com/deremotelauncher/extern"

New-Variable -Name UrlGithubArtifacts -Scope Global -Option ReadOnly -Force `
    -Value "https://github.com/LordOfDragons"
