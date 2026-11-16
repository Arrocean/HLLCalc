#requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory = 'build/cmake',
    [string]$Version = '',
    [string]$VersionSuffix = '',
    [switch]$DesktopTests,
    [switch]$SkipTests
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'This project requires Windows 10/11 and Visual Studio C++ Build Tools.'
}
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not [IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory = Join-Path $source $BuildDirectory }
$build = [IO.Path]::GetFullPath($BuildDirectory)
if ($build.TrimEnd('\') -eq $source.TrimEnd('\')) { throw 'In-source builds are not supported.' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = $null
$generator = $null
if (Test-Path -LiteralPath $vswhere) {
    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0) { throw 'Unable to query Visual Studio.' }
    if ($vsRoot) {
        $vsVersion = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
        $major = [int]($vsVersion.Split('.')[0])
        if ($major -eq 18) { $generator = 'Visual Studio 18 2026' }
        elseif ($major -eq 17) { $generator = 'Visual Studio 17 2022' }
        else { throw 'Install Visual Studio 2022/2026 C++ desktop tools and Windows SDK.' }
    }
}
if (-not $vsRoot) { throw 'Visual Studio 2022/2026 C++ Build Tools and Windows SDK were not found.' }
$bundled = Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $bundled) { $cmake = $bundled }
elseif ($cmakeCommand) { $cmake = $cmakeCommand.Source }
else { throw 'CMake was not found. Install the Visual Studio CMake tools component.' }
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) { throw 'ctest.exe was not found beside cmake.exe.' }

$ui = if ($DesktopTests) { 'ON' } else { 'OFF' }
$configure = @('-S',$source,'-B',$build,'-G',$generator,'-A','x64','-DBUILD_TESTING=ON',"-DHLLCALC_BUILD_UI_TESTS=$ui")
if ($Version) { $configure += "-DHLLCALC_VERSION=$Version" }
$configure += "-DHLLCALC_VERSION_SUFFIX=$VersionSuffix"
& $cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
if (-not $SkipTests) {
    if ($DesktopTests) { Write-Warning 'Desktop tests briefly control the keyboard, cursor and foreground window. Do not interact until they finish.' }
    & $ctest --test-dir $build -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed. Close any running HLLCalc instance or conflicting hotkey applications, then retry.' }
}
$binary = Join-Path $build "bin/$Configuration/HLLCalc.exe"
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Expected executable not found: $binary" }
$convenience = Join-Path $source 'build/HLLCalc.exe'
New-Item -ItemType Directory -Path (Split-Path -Parent $convenience) -Force | Out-Null
try { Copy-Item -LiteralPath $binary -Destination $convenience -Force }
catch { throw "Could not update build/HLLCalc.exe. Close the running program and retry. $($_.Exception.Message)" }
Write-Host "Build succeeded: $convenience"
