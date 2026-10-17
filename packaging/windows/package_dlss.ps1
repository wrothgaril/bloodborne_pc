# Repackage an existing Windows v1.8.2 release with the locally rebuilt DLSS bridge.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BaseArchive,
    [string]$PackageName = 'Bloodborne-Windows-DLSS-4.5'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if ($PackageName -notmatch '^[A-Za-z0-9][A-Za-z0-9_.-]*$') { throw 'PackageName must be a plain folder name.' }
foreach ($file in @('bbport_dlss.dll', 'nvngx_dlss.dll', 'NVIDIA-DLSS-LICENSE.txt')) {
    if (!(Test-Path -LiteralPath "$repo/out/$file")) { throw "Build DLSS first: out/$file missing." }
}
$stage = Join-Path $repo "out/packages/$PackageName"
$archive = Join-Path $repo "dist/$PackageName.zip"
$destination = Join-Path $repo "dist/$PackageName"
# Never refresh a playable folder containing saves; use a new staging directory.
if (Test-Path -LiteralPath $stage) { throw "Staging directory exists; choose a new PackageName: $stage" }
if ((Test-Path -LiteralPath $archive) -or (Test-Path -LiteralPath $destination)) {
    throw 'Output already exists; choose a new PackageName.'
}
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Expand-Archive -LiteralPath $BaseArchive -DestinationPath $stage
$play = Join-Path $stage 'bbport-windows'
if (!(Test-Path -LiteralPath "$play/bin/bb-probe.exe")) { throw 'Expected bbport-windows/bin/bb-probe.exe in BaseArchive.' }
Copy-Item -LiteralPath "$repo/out/bbport_dlss.dll","$repo/out/nvngx_dlss.dll" -Destination "$play/bin"
Copy-Item -LiteralPath "$repo/out/NVIDIA-DLSS-LICENSE.txt" -Destination "$play/licenses"
Copy-Item -LiteralPath "$repo/docs/DLSS_BUILD.md" -Destination "$play/README-DLSS.md"
Set-Content -LiteralPath "$play/bbport.ini" -Value 'upscaler=dlss' -Encoding ascii
# Include the bridge source and build instructions alongside its binary.
New-Item -ItemType Directory -Force -Path "$play/dlss-source/packaging/windows","$play/dlss-source/gpu" | Out-Null
Copy-Item -LiteralPath "$repo/gpu/dlss_bridge" -Destination "$play/dlss-source/gpu" -Recurse
Copy-Item -LiteralPath "$repo/packaging/windows/build_dlss.ps1","$repo/packaging/windows/package_dlss.ps1" -Destination "$play/dlss-source/packaging/windows"
New-Item -ItemType Directory -Force -Path "$play/dlss-source/docs" | Out-Null
Copy-Item -LiteralPath "$repo/docs/DLSS_BUILD.md" -Destination "$play/dlss-source/docs"
$version = (Get-Item -LiteralPath "$play/bin/nvngx_dlss.dll").VersionInfo
$files = @('bin/bb-probe.exe','bin/bbport_dlss.dll','bin/nvngx_dlss.dll') | ForEach-Object {
    @{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $play $_)).Hash }
}
$manifest = [ordered]@{
    base_release = 'windows-v1.8.2'
    base_archive_sha256 = (Get-FileHash -LiteralPath $BaseArchive).Hash
    dlss_runtime = '{0}.{1}.{2}.{3}' -f $version.FileMajorPart,$version.FileMinorPart,$version.FileBuildPart,$version.FilePrivatePart
    super_resolution = 'DLSS 4.5; M preset, L for Ultra Performance'
    dlss5_neural_rendering = $false
    dlss_frame_generation = $false
    files = $files
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$play/DLSS-build.json" -Encoding utf8
New-Item -ItemType Directory -Force -Path "$repo/dist" | Out-Null
Compress-Archive -LiteralPath $play -DestinationPath $archive -CompressionLevel Optimal
Copy-Item -LiteralPath $play -Destination $destination -Recurse
Write-Host "Package ready: $archive"

