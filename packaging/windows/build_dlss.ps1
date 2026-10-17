# Native Windows build of the ABI 1 DLSS 4.5 bridge; no MSYS2 installation needed.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SdkRoot,
    [string]$VulkanRoot = $env:VULKAN_SDK,
    [switch]$SmokeTest
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$sdk = (Resolve-Path -LiteralPath $SdkRoot).Path
$runtime = Join-Path $sdk 'lib/Windows_x86_64/rel/nvngx_dlss.dll'
if (!(Test-Path -LiteralPath $runtime)) { throw "Release DLSS runtime missing: $runtime" }
$version = (Get-Item -LiteralPath $runtime).VersionInfo
if ([version]::new($version.FileMajorPart, $version.FileMinorPart, $version.FileBuildPart) -lt [version]'310.5.0') {
    throw 'DLSS 4.5 needs the NVIDIA release runtime 310.5.0 or newer.'
}
if (!$VulkanRoot -or !(Test-Path -LiteralPath "$VulkanRoot/Include/vulkan/vulkan.h")) {
    throw 'Set VulkanRoot or VULKAN_SDK to an installed Vulkan SDK.'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
if (!$vs) { throw 'Install Visual Studio C++ x64 build tools.' }
$cmake = Join-Path $vs.installationPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (!(Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
$major = ([version]$vs.installationVersion).Major
switch ($major) {
    17 { $generator = 'Visual Studio 17 2022' }
    18 { $generator = 'Visual Studio 18 2026' }
    default { throw "Unsupported Visual Studio major version: $major" }
}
$build = Join-Path $repo 'out/dlss-bridge-native'
& $cmake -S "$repo/gpu/dlss_bridge" -B $build -G $generator -A x64 `
    "-DDLSS_SDK_ROOT=$sdk" "-DVULKAN_INCLUDE=$VulkanRoot/Include" `
    "-DBB_DLSS_SMOKE_TEST=$($SmokeTest.IsPresent.ToString().ToUpperInvariant())"
if ($LASTEXITCODE -ne 0) { throw 'DLSS bridge configuration failed.' }
& $cmake --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'DLSS bridge compilation failed.' }
Copy-Item -LiteralPath "$build/Release/bbport_dlss.dll",$runtime -Destination "$repo/out"
Copy-Item -LiteralPath "$sdk/LICENSE.txt" -Destination "$repo/out/NVIDIA-DLSS-LICENSE.txt"
if ($SmokeTest) {
    # The test loads the DLL and NGX from its own executable directory.
    Copy-Item -LiteralPath $runtime -Destination "$build/Release"
    & "$build/Release/bbport_dlss_smoke.exe"
    if ($LASTEXITCODE -ne 0) { throw 'DLSS GPU smoke test failed.' }
}
Write-Host "DLSS bridge ready: $repo/out/bbport_dlss.dll"

