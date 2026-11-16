#Requires -Version 7.0
# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')][string]$Configuration = 'Release',
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$SourceDirectory = (Split-Path -Parent $PSScriptRoot),
    [switch]$AllowUntrackedSource
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ValidatedVersion([string]$Text) {
    $pattern = '^(?<major>0|[1-9][0-9]*)\.(?<minor>0|[1-9][0-9]*)\.(?<patch>0|[1-9][0-9]*)(?:-(?<suffix>[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$'
    $match = [regex]::Match($Text, $pattern)
    if ($Text.Length -gt 159 -or -not $match.Success) {
        throw 'Version must be MAJOR.MINOR.PATCH or MAJOR.MINOR.PATCH-prerelease (without v).'
    }
    foreach ($name in @('major', 'minor', 'patch')) {
        $number = 0
        if (-not [int]::TryParse($match.Groups[$name].Value, [ref]$number) -or $number -gt 65535) {
            throw 'Version components must be in 0..65535.'
        }
    }
    $suffix = $match.Groups['suffix'].Value
    foreach ($identifier in $suffix.Split('.')) {
        if ($identifier -cmatch '^0[0-9]+$') { throw 'Numeric prerelease identifiers cannot contain leading zeroes.' }
    }
    return [pscustomobject]@{
        Core = "$($match.Groups['major'].Value).$($match.Groups['minor'].Value).$($match.Groups['patch'].Value)"
        Suffix = $suffix
    }
}

function Get-AbsoluteDirectory([string]$Value, [bool]$MustExist) {
    $absolute = [IO.Path]::GetFullPath($Value)
    if ($MustExist -and -not (Test-Path -LiteralPath $absolute -PathType Container)) {
        throw "Directory does not exist: $absolute"
    }
    return $absolute.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
}

function Assert-SafeSourcePath([string]$Relative) {
    $name = $Relative.Replace('\', '/')
    if ($name.StartsWith('/') -or $name -match '(^|/)\.\.(/|$)' -or $name.Contains(':')) {
        throw "Unsafe source archive path: $Relative"
    }
    if ($name -imatch '(^|/)(build|out|release|dist|\.git|\.vs|\.idea|\.vscode|node_modules|__pycache__|\.venv|venv|cmake-build-[^/]*)(/|$)' -or
        $name -imatch '(^|/)(\.env(?:\.[^/]*)?|id_rsa(?:\.[^/]*)?|id_ed25519(?:\.[^/]*)?|credentials(?:\.[^/]*)?|secrets?(?:\.[^/]*)?)(/|$)' -or
        $name -imatch '\.(exe|dll|obj|pdb|ilk|res|dmp|log|tmp|temp|pem|key|pfx|p12|keystore)$') {
        throw "Generated or potentially sensitive file must not enter source archive: $Relative"
    }
}

function Assert-ExpectedOutput([string]$Target, [string]$ExpectedName) {
    $absolute = [IO.Path]::GetFullPath($Target)
    if ([IO.Path]::GetDirectoryName($absolute) -ne $outputRoot -or [IO.Path]::GetFileName($absolute) -cne $ExpectedName) {
        throw "Unexpected output target: $absolute"
    }
    return $absolute
}

$parsed = Get-ValidatedVersion $Version
$sourceRoot = Get-AbsoluteDirectory $SourceDirectory $true
$buildRoot = Get-AbsoluteDirectory $BuildDirectory $true
$outputRoot = Get-AbsoluteDirectory $OutputDirectory $false
if ($sourceRoot -eq $buildRoot -or $sourceRoot -eq $outputRoot -or $buildRoot -eq $outputRoot) {
    throw 'SourceDirectory, BuildDirectory and OutputDirectory must be distinct.'
}
foreach ($required in @('CMakeLists.txt', 'LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.md')) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $required) -PathType Leaf)) { throw "Missing project file: $required" }
}
$licenseManifest = @(
    @{ Source = 'third_party/imgui/LICENSE.txt'; Installed = 'Dear-ImGui-MIT.txt' },
    @{ Source = 'third_party/B-LAUNCHER-LICENSE'; Installed = 'B-Launcher-MIT.txt' },
    @{ Source = 'third_party/licenses/stb-MIT.txt'; Installed = 'stb-MIT.txt' },
    @{ Source = 'third_party/licenses/ProggyClean-MIT.txt'; Installed = 'ProggyClean-MIT.txt' },
    @{ Source = 'third_party/licenses/csgo-sharecode-MIT.txt'; Installed = 'csgo-sharecode-MIT.txt' }
)
foreach ($item in $licenseManifest) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $item.Source) -PathType Leaf)) { throw "Missing third-party license: $($item.Source)" }
}
$cachePath = Join-Path $buildRoot 'CMakeCache.txt'
if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw 'BuildDirectory must contain a configured CMakeCache.txt.' }
$cache = [IO.File]::ReadAllText($cachePath)
$homeMatch = [regex]::Match($cache, '(?m)^CMAKE_HOME_DIRECTORY:[^=\r\n]+=(?<value>[^\r\n]*)\r?$')
if (-not $homeMatch.Success -or (Get-AbsoluteDirectory $homeMatch.Groups['value'].Value $true) -ne $sourceRoot) {
    throw 'SourceDirectory does not match the source tree configured in BuildDirectory.'
}
$cmakeMatch = [regex]::Match($cache, '(?m)^CMAKE_COMMAND:[^=\r\n]+=(?<value>[^\r\n]*)\r?$')
$cmakeExecutable = if ($cmakeMatch.Success -and (Test-Path -LiteralPath $cmakeMatch.Groups['value'].Value -PathType Leaf)) {
    $cmakeMatch.Groups['value'].Value
} else {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $command) { throw 'Cannot find CMake. Install CMake or use the recorded CMAKE_COMMAND from a valid build cache.' }
    $command.Source
}
$coreMatch = [regex]::Match($cache, '(?m)^HLLCALC_VERSION:[^=\r\n]+=(?<value>[^\r\n]*)\r?$')
$suffixMatch = [regex]::Match($cache, '(?m)^HLLCALC_VERSION_SUFFIX:[^=\r\n]+=(?<value>[^\r\n]*)\r?$')
if (-not $coreMatch.Success -or -not $suffixMatch.Success -or $coreMatch.Groups['value'].Value -cne $parsed.Core -or $suffixMatch.Groups['value'].Value -cne $parsed.Suffix) {
    throw 'Package version differs from CMake configuration. Configure HLLCALC_VERSION and HLLCALC_VERSION_SUFFIX to match -Version first.'
}

$gitMode = Test-Path -LiteralPath (Join-Path $sourceRoot '.git')
$sourcePaths = @()
if ($gitMode) {
    $top = & git -C $sourceRoot rev-parse --show-toplevel
    if ($LASTEXITCODE -ne 0 -or [IO.Path]::GetFullPath([string]$top).TrimEnd('\', '/') -ne $sourceRoot) { throw 'SourceDirectory must be the root of a valid Git checkout.' }
    $dirty = @(& git -C $sourceRoot status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect Git working tree.' }
    if ($dirty.Count -gt 0) { throw 'Commit or remove working-tree changes before packaging: the executable and git archive HEAD must have matching source.' }
    $sourcePaths = @(& git -C $sourceRoot ls-tree -r --name-only HEAD)
    if ($LASTEXITCODE -ne 0 -or $sourcePaths.Count -eq 0) { throw 'Cannot enumerate Git source tree.' }
    foreach ($relative in $sourcePaths) { Assert-SafeSourcePath ([string]$relative) }
} elseif (-not $AllowUntrackedSource) {
    throw 'No .git checkout found. Official releases require git archive HEAD. For local verification only, explicitly add -AllowUntrackedSource to create a marked, allowlisted untracked snapshot.'
} else {
    Write-Warning 'No Git checkout: creating an explicitly marked untracked source snapshot, not git archive HEAD.'
    $rootFiles = @('CMakeLists.txt', 'CMakePresets.json', 'LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.md', 'build.cmd', '.gitignore', '.gitattributes', '.editorconfig', '.clang-format')
    $directories = @('src', 'tests', 'resources', 'cmake', 'scripts', 'docs', 'third_party', 'licenses', '.github')
    $extensions = @('.cpp', '.cc', '.c', '.h', '.hpp', '.inl', '.in', '.rc', '.ico', '.png', '.json', '.md', '.txt', '.cmake', '.ps1', '.py', '.cmd', '.bat', '.yml', '.yaml', '.csv', '.editorconfig')
    foreach ($name in $rootFiles) {
        $file = Join-Path $sourceRoot $name
        if (Test-Path -LiteralPath $file -PathType Leaf) { $sourcePaths += $name }
    }
    foreach ($directory in $directories) {
        $folder = Join-Path $sourceRoot $directory
        if (-not (Test-Path -LiteralPath $folder -PathType Container)) { continue }
        $entries = @(Get-ChildItem -LiteralPath $folder -Recurse -Force)
        foreach ($entry in $entries) {
            if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Untracked source snapshot refuses links/junctions: $($entry.FullName)" }
            if ($entry.PSIsContainer) { continue }
            $relative = [IO.Path]::GetRelativePath($sourceRoot, $entry.FullName)
            Assert-SafeSourcePath $relative
            if ($extensions -contains $entry.Extension.ToLowerInvariant() -or $entry.Name -imatch '^(LICENSE|COPYING|NOTICE|AUTHORS|B-LAUNCHER-LICENSE)(\..*)?$') {
                $sourcePaths += $relative
            } else { throw "Unrecognized file in allowlisted source directory; review it before packaging: $relative" }
        }
    }
    foreach ($relative in $sourcePaths) {
        $entry = Get-Item -LiteralPath (Join-Path $sourceRoot $relative) -Force
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Source snapshot refuses links: $relative" }
    }
}
foreach ($required in @('CMakeLists.txt', 'LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.md')) {
    if ($sourcePaths -notcontains $required) { throw "Required corresponding source is absent from archive: $required" }
}
foreach ($directory in @('src/', 'tests/', 'resources/', 'cmake/', 'scripts/', 'docs/', 'third_party/', '.github/')) {
    if (-not ($sourcePaths | Where-Object { $_.Replace('\', '/').StartsWith($directory, [StringComparison]::Ordinal) })) {
        throw "Required corresponding source directory is absent from archive: $directory"
    }
}

foreach ($item in $licenseManifest) {
    if (-not ($sourcePaths | Where-Object { $_.Replace('\', '/') -ceq $item.Source })) { throw "Source archive is missing third-party license: $($item.Source)" }
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$nonce = [guid]::NewGuid().ToString('N')
$workRoot = Join-Path $outputRoot ".package-$nonce"
New-Item -ItemType Directory -Path $workRoot -ErrorAction Stop | Out-Null
$marker = Join-Path $workRoot '.hllcalc-package-owned'
[IO.File]::WriteAllText($marker, $nonce)
try {
    $runtime = Join-Path $workRoot 'runtime'
    New-Item -ItemType Directory -Path $runtime | Out-Null
    & $cmakeExecutable --install $buildRoot --config $Configuration --prefix $runtime
    if ($LASTEXITCODE -ne 0) { throw 'cmake --install failed.' }
    foreach ($required in @('HLLCalc.exe', 'LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.md')) {
        $installed = Join-Path $runtime $required
        if (-not (Test-Path -LiteralPath $installed -PathType Leaf) -or (Get-Item -LiteralPath $installed).Length -eq 0) { throw "Installed package is missing $required" }
    }
    $reader = [IO.BinaryReader]::new([IO.File]::OpenRead((Join-Path $runtime 'HLLCalc.exe')))
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'Installed executable has no DOS header.' }
        $reader.BaseStream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 64 -or $peOffset -gt $reader.BaseStream.Length - 6) { throw 'Installed executable has an invalid PE offset.' }
        $reader.BaseStream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'This package requires a Windows x64 PE executable.' }
    } finally { $reader.Dispose() }
    $productVersion = (Get-Item -LiteralPath (Join-Path $runtime 'HLLCalc.exe')).VersionInfo.ProductVersion
    if ($productVersion -cne $Version) { throw "Executable version $productVersion differs from package version $Version; rebuild after configuring." }
    foreach ($required in @('LICENSE', 'THIRD_PARTY_NOTICES.md')) {
        if ((Get-FileHash -LiteralPath (Join-Path $runtime $required)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $sourceRoot $required)).Hash) { throw "Installed legal file differs from source: $required" }
    }
    foreach ($item in $licenseManifest) {
        $original = Join-Path $sourceRoot $item.Source
        $installed = Join-Path (Join-Path $runtime 'licenses') $item.Installed
        if (-not (Test-Path -LiteralPath $installed -PathType Leaf) -or (Get-FileHash -LiteralPath $original).Hash -ne (Get-FileHash -LiteralPath $installed).Hash) { throw "Missing or modified installed third-party license: $($item.Installed)" }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $runtime 'docs') -PathType Container)) { throw 'Installed package is missing docs.' }
    $binaryName = "HLLCalc-$Version-windows-x64.zip"
    $sourceName = "HLLCalc-$Version-source.zip"
    $binaryZip = Join-Path $workRoot $binaryName
    $sourceZip = Join-Path $workRoot $sourceName
    [IO.Compression.ZipFile]::CreateFromDirectory($runtime, $binaryZip, [IO.Compression.CompressionLevel]::Optimal, $false)
    if ($gitMode) {
        & git -C $sourceRoot archive --format=zip "--prefix=HLLCalc-$Version/" "--output=$sourceZip" HEAD
        if ($LASTEXITCODE -ne 0) { throw 'git archive HEAD failed.' }
    } else {
        $sourceStage = Join-Path $workRoot 'source'
        $sourceFolder = Join-Path $sourceStage "HLLCalc-$Version"
        New-Item -ItemType Directory -Path $sourceFolder -Force | Out-Null
        foreach ($relative in $sourcePaths) {
            $target = Join-Path $sourceFolder $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $sourceRoot $relative) -Destination $target
        }
        [IO.File]::WriteAllText((Join-Path $sourceFolder 'UNTRACKED-SNAPSHOT.txt'), "LOCAL VERIFICATION SNAPSHOT`nThis archive was built from an explicit source allowlist without a Git checkout.`nIt is not git archive HEAD and is not a verified tagged release.`n")
        [IO.Compression.ZipFile]::CreateFromDirectory($sourceStage, $sourceZip, [IO.Compression.CompressionLevel]::Optimal, $false)
    }
    $sums = @()
    foreach ($archive in @($binaryZip, $sourceZip)) { $sums += "$((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($archive))" }
    $checksum = Join-Path $workRoot 'SHA256SUMS.txt'
    [IO.File]::WriteAllText($checksum, (($sums -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
    foreach ($name in @($binaryName, $sourceName, 'SHA256SUMS.txt')) {
        $target = Assert-ExpectedOutput (Join-Path $outputRoot $name) $name
        # Only the three exact versioned output names are replaced on rerun.
        Move-Item -LiteralPath (Join-Path $workRoot $name) -Destination $target -Force
        Write-Host "Created: $target"
    }
} finally {
    $absoluteWork = [IO.Path]::GetFullPath($workRoot)
    $prefix = $outputRoot + [IO.Path]::DirectorySeparatorChar
    if (-not $absoluteWork.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($absoluteWork) -cne ".package-$nonce" -or
        -not (Test-Path -LiteralPath $marker -PathType Leaf) -or
        [IO.File]::ReadAllText($marker) -cne $nonce -or
        ((Get-Item -LiteralPath $absoluteWork).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing to remove an unverified packaging directory: $absoluteWork"
    }
    Remove-Item -LiteralPath $absoluteWork -Recurse -Force
}
