param(
    [ValidateSet('x64','ARM64')][string]$Architecture = 'x64',
    [string]$SDK = '10.0.26100.0',
    [switch]$VerifyRendering
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& "$PSScriptRoot\verify-uwp-package.ps1" -Architecture $Architecture
$package = Get-ChildItem "$repo\artifacts\packages\$Architecture" -File -Recurse |
    Where-Object { $_.Name -like 'OpenGLLab_*' -and $_.Extension -in @('.appx','.msix') } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$layout = Join-Path $repo "artifacts\deploy\$Architecture"
$makeappx = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\$SDK\x64\makeappx.exe"
if (!(Test-Path -LiteralPath $makeappx)) { throw "Missing SDK packaging tool: $makeappx" }
& $makeappx unpack /p $package.FullName /d $layout /o | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Package extraction failed; close OpenGLLab before retrying.' }
[xml]$manifest = Get-Content "$layout\AppxManifest.xml"
$installed = Get-AppxPackage -Name $manifest.Package.Identity.Name
# An already registered development layout loads updated binaries/bundles directly.
# Re-registering the same identity/version can be rejected by Windows (0x80073CFB).
if (!$installed -or !$installed.IsDevelopmentMode -or
    $installed.InstallLocation -ne $layout -or
    $installed.Version.ToString() -ne $manifest.Package.Identity.Version) {
    Add-AppxPackage -Register "$layout\AppxManifest.xml"
    $installed = Get-AppxPackage -Name $manifest.Package.Identity.Name
}
$runtimeLog = "$env:LOCALAPPDATA\Packages\$($installed.PackageFamilyName)\LocalState\renderer.log"
$previousLineCount = @(Get-Content -LiteralPath $runtimeLog -ErrorAction SilentlyContinue).Count
Start-Process explorer.exe -ArgumentList "shell:AppsFolder\$($installed.PackageFamilyName)!App" -WindowStyle Hidden
Write-Host "Requested OpenGLLab launch. Runtime log: $runtimeLog"
if ($VerifyRendering) {
    $deadline = (Get-Date).AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 250
        $newLines = @(Get-Content -LiteralPath $runtimeLog -ErrorAction SilentlyContinue | Select-Object -Skip $previousLineCount)
        if (($newLines -match '^React bundle loaded') -and
            ($newLines -match '^React settings:') -and
            ($newLines -match '^First frame presented:.*GL_NO_ERROR')) {
            Write-Host 'Verified React bundle loading, React-to-C++ settings, and ANGLE frame presentation.'
            return
        }
    } while ((Get-Date) -lt $deadline)
    throw "No successful React/ANGLE frame within 30 seconds. Inspect $runtimeLog"
}
