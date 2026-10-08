<#
.SYNOPSIS
    Builds the Popa solution and runs the unit tests from the command line.

.EXAMPLE
    .\build.ps1                  # Debug and Release, then tests on both
    .\build.ps1 -Configuration Release
    .\build.ps1 -NoTests
#>
param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',
    [switch]$NoTests
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

# vswhere ships with the Visual Studio Installer at a fixed location and tells
# where Visual Studio is installed, so the script does not hard-code a path.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found. Is Visual Studio installed?' }

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
    -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild.exe not found.' }

$vstest = & $vswhere -latest -products * `
    -find 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe' | Select-Object -First 1
if (-not $vstest -and -not $NoTests) { throw 'vstest.console.exe not found.' }

$configurations = if ($Configuration -eq 'All') { 'Debug', 'Release' } else { $Configuration }

foreach ($config in $configurations) {
    Write-Host "=== Building $config ===" -ForegroundColor Cyan
    & $msbuild (Join-Path $root 'Popa.slnx') -nologo -verbosity:minimal -maxCpuCount `
        "-property:Configuration=$config" '-property:Platform=x64'
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($config)." }

    $exe = Join-Path $root "bin\x64\$config\Popa.exe"
    $size = (Get-Item $exe).Length
    Write-Host ("Popa.exe ($config): {0:N0} bytes ({1:N2} MB)" -f $size, ($size / 1MB)) -ForegroundColor Green

    if (-not $NoTests) {
        Write-Host "=== Testing $config ===" -ForegroundColor Cyan
        & $vstest (Join-Path $root "bin\x64\$config\PopaTests.dll") /Platform:x64
        if ($LASTEXITCODE -ne 0) { throw "Tests failed ($config)." }
    }
}
