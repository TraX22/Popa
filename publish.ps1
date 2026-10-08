<#
.SYNOPSIS
    Prepares a release of Popa and, optionally, publishes it on GitHub.

.DESCRIPTION
    1. Reads the version from src\Popa\version.h.
    2. Builds Release and runs the unit tests (build.ps1).
    3. Checks the result: size under 5 MB and the version resource.
    4. Copies Popa.exe and THIRD-PARTY-NOTICES.txt to dist\ and writes the
       SHA-256 fingerprint of Popa.exe.
    With -Publish it also creates the tag v<version> and the GitHub release
    with both files attached, using the GitHub CLI (gh). The notices file
    must travel with the program: nlohmann/json's MIT license requires it. The release
    notes come from docs\release-notes\v<version>.md.

    The asset must be called exactly "Popa.exe": that is the name the
    updater looks for in the latest release.

.EXAMPLE
    .\publish.ps1            # build and check only; nothing leaves this PC
    .\publish.ps1 -Publish   # also tag, push the tag and create the release
#>
param([switch]$Publish)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

# 1. Version, from the single place where it is defined.
$header = Get-Content (Join-Path $root 'src\Popa\version.h') -Raw
$parts = foreach ($name in 'MAJOR', 'MINOR', 'PATCH') {
    if ($header -notmatch "#define POPA_VERSION_$name (\d+)") { throw "POPA_VERSION_$name not found in version.h" }
    $Matches[1]
}
$version = $parts -join '.'
$tag = "v$version"
Write-Host "Popa $version" -ForegroundColor Cyan

# 2. Build and test.
& (Join-Path $root 'build.ps1') -Configuration Release
if ($LASTEXITCODE -ne 0) { throw 'Build or tests failed.' }

# 3. Checks.
$exe = Join-Path $root 'bin\x64\Release\Popa.exe'
$size = (Get-Item $exe).Length
if ($size -ge 5MB) { throw "Popa.exe is $size bytes; the limit is 5 MB." }
$fileVersion = (Get-Item $exe).VersionInfo.FileVersion
if ($fileVersion -ne $version) { throw "The version resource says '$fileVersion', expected '$version'." }

# 4. dist\ with the executable and its fingerprint.
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item $exe (Join-Path $dist 'Popa.exe') -Force
Copy-Item (Join-Path $root 'THIRD-PARTY-NOTICES.txt') (Join-Path $dist 'THIRD-PARTY-NOTICES.txt') -Force
$hash = (Get-FileHash (Join-Path $dist 'Popa.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  Popa.exe" | Set-Content (Join-Path $dist 'Popa.exe.sha256') -Encoding ascii
Write-Host ("dist\Popa.exe: {0:N0} bytes, SHA-256 {1}" -f $size, $hash) -ForegroundColor Green

if (-not $Publish) {
    Write-Host 'Not published. Run again with -Publish to create the GitHub release.'
    return
}

# 5. Publish (only with -Publish).
$notes = Join-Path $root "docs\release-notes\$tag.md"
if (-not (Test-Path $notes)) { throw "Release notes not found: $notes" }
if (git -C $root status --porcelain) { throw 'There are uncommitted changes; commit them first.' }
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'GitHub CLI (gh) not found.' }

if (-not (git -C $root tag --list $tag)) {
    git -C $root tag -a $tag -m "Popa $version"
}
git -C $root push origin $tag
if ($LASTEXITCODE -ne 0) { throw 'Could not push the tag.' }

# GitHub computes and shows the SHA-256 digest of each uploaded file; the
# updater compares the downloaded Popa.exe against it.
gh release create $tag (Join-Path $dist 'Popa.exe') (Join-Path $dist 'THIRD-PARTY-NOTICES.txt') --title "Popa $version" --notes-file $notes --verify-tag
if ($LASTEXITCODE -ne 0) { throw 'Could not create the release.' }
Write-Host "Published $tag." -ForegroundColor Green
