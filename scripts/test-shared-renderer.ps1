param([string]$VisualStudio = 'C:\Program Files\Microsoft Visual Studio\18\Community')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$output = Join-Path $repo 'artifacts\renderer-tests'
$angle = Join-Path $repo 'third_party\angle-uwp'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$buildScript = Join-Path $output 'build.cmd'
@"
@echo off
call "$VisualStudio\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /MD /W4 /I"$angle\include" "$repo\tests\ViewerRendererSmoke.cpp" "$repo\native\shared\ViewerRenderer.cpp" /Fe:"$output\ViewerRendererSmoke.exe" /link /LIBPATH:"$angle\lib\x64" libEGL.lib libGLESv2.lib
exit /b %errorlevel%
"@ | Set-Content -LiteralPath $buildScript -Encoding ascii
Push-Location $output
try {
    & $env:ComSpec /d /c $buildScript
    if ($LASTEXITCODE -ne 0) { throw 'Shared renderer test build failed' }
    Copy-Item -Path "$angle\bin\x64\*.dll" -Destination $output -Force
    # The ANGLE package imports the UWP CRT, even in this desktop test host.
    $vclibs = Get-AppxPackage -Name 'Microsoft.VCLibs.140.00' |
        Where-Object { $_.Architecture -eq 'X64' } | Select-Object -First 1
    if (!$vclibs) { throw 'Install x64 Microsoft.VCLibs.140.00 before running the test' }
    Copy-Item -Path (Join-Path $vclibs.InstallLocation '*_APP.dll') -Destination $output -Force
    & "$output\ViewerRendererSmoke.exe" $repo | Tee-Object -FilePath (Join-Path $output 'smoke.log')
    if ($LASTEXITCODE -ne 0) { throw "Shared renderer smoke test failed (exit $LASTEXITCODE)" }
} finally { Pop-Location }
