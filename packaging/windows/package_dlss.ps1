# Package a complete source build; PR #69's bridge cannot overlay an official binary.
[CmdletBinding()]
param(
    [string]$BaseArchive,
    [string]$Msys2Root = $env:MSYS2_ROOT,
    [string]$WindowsPython = $env:WINPYTHON,
    [string]$BashExe = 'C:/msys64/usr/bin/bash.exe'
)
$ErrorActionPreference = 'Stop'
if ($BaseArchive) {
    throw 'Archive overlays are incompatible with the PR #69 renderer. Build the complete renderer first, then run this script without BaseArchive.'
}
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$Msys2Root) {
    $localToolchain = Join-Path $repo 'out/msys2'
    $Msys2Root = if (Test-Path -LiteralPath "$localToolchain/clang64/bin/clang.exe") { $localToolchain } else { 'C:/msys64' }
}
$Msys2Root = (Resolve-Path -LiteralPath $Msys2Root).Path
if (!(Test-Path -LiteralPath $BashExe)) { throw 'Set BashExe to the MSYS2 bash.exe (not WSL bash).' }
foreach ($file in @('bb-probe.exe','bb-gpu-capabilities.exe','bb-play.exe','bbport_dlss.dll','bbport_nr_nvngx.dll')) {
    if (!(Test-Path -LiteralPath "$repo/out/$file")) { throw "Build the renderer and adapters first: out/$file missing." }
}
# Check the recursive staging cleanup targets before invoking the shell packager.
foreach ($relative in @('out/stage','out/pyi-work','out/pyi-dist','dist/bbport-windows')) {
    $target = [IO.Path]::GetFullPath((Join-Path $repo $relative))
    if (!$target.StartsWith($repo + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package path escapes the workspace: $target"
    }
    if ((Test-Path -LiteralPath $target) -and ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Package cleanup target is a link: $target"
    }
}
$previousRoot = $env:BB_PACKAGE_MSYS_ROOT
$previousPython = $env:WINPYTHON
try {
    $env:BB_PACKAGE_MSYS_ROOT = $Msys2Root
    if ($WindowsPython) { $env:WINPYTHON = $WindowsPython }
    Push-Location $repo
    try {
        $command = [string]::Join("`n", @('export PATH="/usr/bin:$PATH"', 'export MSYS2_ROOT="$(cygpath -u "$BB_PACKAGE_MSYS_ROOT")"', 'exec bash packaging/windows/package.sh'))
        & $BashExe --noprofile --norc -c $command
        if ($LASTEXITCODE -ne 0) { throw 'Full source package failed.' }
    } finally { Pop-Location }
} finally {
    $env:BB_PACKAGE_MSYS_ROOT = $previousRoot
    $env:WINPYTHON = $previousPython
}
