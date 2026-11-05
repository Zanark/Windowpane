param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Visual Studio Installer (vswhere) is required.' }
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Install Visual Studio 2022 C++ desktop tools and Windows SDK 10.0.26100.0.' }
$cmake = Join-Path $visualStudio 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
if (-not (Test-Path $cmake)) { throw 'The Visual Studio CMake component is required.' }

& $cmake -S $root -B (Join-Path $root 'build') -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_SYSTEM_VERSION=10.0.26100.0'
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build (Join-Path $root 'build') --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
if ($Test) {
    & $ctest --test-dir (Join-Path $root 'build') -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'WindowPane checks failed.' }
}
Write-Output (Join-Path $root "build\$Configuration\WindowPane.exe")
