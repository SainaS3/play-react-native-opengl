param([Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Architecture)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$package = Get-ChildItem "$repo\artifacts\packages\$Architecture" -File -Recurse |
    Where-Object { $_.Name -like 'OpenGLLab_*' -and $_.Extension -in @('.appx','.msix') } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$package) { throw "No $Architecture app package found" }
$archive = [IO.Compression.ZipFile]::OpenRead($package.FullName)
try {
    $machine = if ($Architecture -eq 'x64') { 0x8664 } else { 0xAA64 }
    if (!$archive.GetEntry('ANGLE-LICENSE.txt')) { throw 'Missing ANGLE redistribution license' }
    foreach ($name in @('OpenGLLab.exe','libEGL.dll','libGLESv2.dll','d3dcompiler_47.dll','Microsoft.ReactNative.dll','hermes.dll')) {
        $entry = $archive.GetEntry($name)
        if (!$entry) { throw "Missing package-root payload: $name" }
        $memory = [IO.MemoryStream]::new()
        $stream = $entry.Open()
        try { $stream.CopyTo($memory) } finally { $stream.Dispose() }
        $bytes = $memory.ToArray(); $memory.Dispose()
        $pe = [BitConverter]::ToInt32($bytes,0x3c)
        $actual = [BitConverter]::ToUInt16($bytes,$pe+4)
        # The ARM64 SDK shader compiler may use an ARM64X hybrid PE.
        if ($actual -ne $machine -and !($name -eq 'd3dcompiler_47.dll' -and $Architecture -eq 'ARM64' -and $actual -eq 0xA64E)) {
            throw ('Wrong architecture for {0}: {1:X4}' -f $name,$actual)
        }
        $dllFlags = [BitConverter]::ToUInt16($bytes,$pe+24+70)
        if ($name -in @('OpenGLLab.exe','libEGL.dll','libGLESv2.dll') -and ($dllFlags -band 0x1000) -eq 0) { throw "Missing AppContainer flag: $name" }
    }
    $bundle = $archive.GetEntry('Bundle/index.windows.bundle')
    if (!$bundle -or $bundle.Length -lt 10000) { throw 'Missing or empty packaged React Native JavaScript bundle' }
    foreach ($file in Get-ChildItem "$repo\shared\resources\models" -Filter *.obj) {
        if (!$archive.GetEntry('resources/models/'+$file.Name)) { throw "Missing model: $($file.Name)" }
    }
    foreach ($file in Get-ChildItem "$repo\shared\resources\shaders" -File) {
        if (!$archive.GetEntry('resources/shaders/'+$file.Name)) { throw "Missing shader: $($file.Name)" }
    }
    $reader = [IO.StreamReader]::new($archive.GetEntry('AppxManifest.xml').Open())
    try { [xml]$manifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
    if ($manifest.Package.Identity.ProcessorArchitecture -ne $Architecture.ToLowerInvariant()) { throw 'Manifest architecture mismatch' }
    if (!($manifest.Package.Dependencies.PackageDependency | Where-Object Name -eq 'Microsoft.VCLibs.140.00')) { throw 'Missing UWP VCLibs dependency' }
    Write-Host "Verified $Architecture package: $($package.FullName)"
    Write-Host 'Architecture, AppContainer, ANGLE DLLs, shader compiler, models, shaders and VCLibs dependency passed.'
} finally { $archive.Dispose() }
