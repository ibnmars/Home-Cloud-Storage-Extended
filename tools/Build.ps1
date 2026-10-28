# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
param(
    [ValidateSet('debug', 'release', 'tests')][string]$Preset = 'release',
    [string]$VcpkgRoot,
    [switch]$RunTests,
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$selectedVcpkgRoot = $VcpkgRoot
if (!$selectedVcpkgRoot -and (Test-Path -LiteralPath (Join-Path $projectRoot 'build-tools/vcpkg'))) {
    $selectedVcpkgRoot = Join-Path $projectRoot 'build-tools/vcpkg'
}
if (!$selectedVcpkgRoot) { $selectedVcpkgRoot = $env:VCPKG_ROOT }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = if (Test-Path -LiteralPath $vswhere) {
    & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    if (!$vsRoot) { throw 'Run in an x64 Native Tools prompt or install Visual Studio C++ tools.' }
    & (Join-Path $vsRoot 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
}
$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } elseif ($vsRoot) {
    Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
} else { throw 'CMake is required.' }
if (!(Get-Command ninja.exe -ErrorAction SilentlyContinue) -and $vsRoot) {
    $env:PATH = (Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja') + ';' + $env:PATH
}
$env:VSLANG = '1033'
if ($Preset -ne 'tests') {
    if (!$selectedVcpkgRoot) { throw 'Set VCPKG_ROOT to a bootstrapped vcpkg checkout.' }
    $env:VCPKG_ROOT = (Resolve-Path -LiteralPath $selectedVcpkgRoot).Path
    $downloadDirectory = if (Test-Path -LiteralPath (Join-Path $projectRoot 'dependencies/sources')) {
        'dependencies/sources'
    } else { 'build/vcpkg-downloads' }
    foreach ($entry in @{
        VCPKG_DOWNLOADS = $downloadDirectory
        VCPKG_DEFAULT_BINARY_CACHE = 'build/vcpkg-cache'
        VCPKG_REGISTRIES_CACHE = 'build/vcpkg-registries'
    }.GetEnumerator()) {
        if (![Environment]::GetEnvironmentVariable($entry.Key)) {
            $cache = Join-Path $projectRoot $entry.Value
            New-Item -ItemType Directory -Path $cache -Force | Out-Null
            [Environment]::SetEnvironmentVariable($entry.Key, $cache, 'Process')
        }
    }
}
Push-Location -LiteralPath $projectRoot
try {
    & $cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $buildArguments = @('--build', '--preset', $Preset, '--parallel', '2')
    if ($Clean) { $buildArguments += '--clean-first' }
    & $cmake @buildArguments 2>&1 | Where-Object { $_ -notmatch '^(Note: including file:|Примечание: включение файла:)' }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($RunTests -and $Preset -ne 'release') {
        & (Join-Path (Split-Path $cmake) 'ctest.exe') --preset $Preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
} finally {
    Pop-Location
}
