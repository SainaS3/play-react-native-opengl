param(
    [ValidateSet('x64','ARM64')][string[]]$Architectures = @('x64','ARM64'),
    [string]$MSBuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe',
    [string]$Toolset = 'v145',
    [string]$SDK = '10.0.26100.0',
    [string]$AnglePackage = 'C:\Users\ASUS\source\repos\angle\artifacts\angle-uwp-release',
    [string]$CertificateThumbprint = ''
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (!(Test-Path -LiteralPath $MSBuild)) { throw "MSBuild not found: $MSBuild" }
& "$PSScriptRoot\prepare-angle-uwp.ps1" -Package $AnglePackage
Push-Location $repo
try {
    New-Item -ItemType Directory -Force artifacts\logs | Out-Null
    & node scripts/generate-models.mjs
    if ($LASTEXITCODE -ne 0) { throw 'Model list generation failed' }
    & node node_modules/@react-native-community/cli/build/bin.js autolink-windows
    if ($LASTEXITCODE -ne 0) { throw 'React Native Windows autolinking failed' }
    foreach ($arch in $Architectures) {
        $arguments = @('windows\OpenGLLab.sln','/restore','/nologo','/m:1','/v:minimal',
            '/p:PreferredToolArchitecture=x64','/p:CL_MPCount=2',
            '/p:Configuration=Release',"/p:Platform=$arch","/p:PlatformToolset=$Toolset",
            "/p:WindowsTargetPlatformVersion=$SDK", '/p:AppxBundle=Never',
            "/p:AppxPackageDir=$repo\artifacts\packages\$arch\",
            "/flp:logfile=artifacts\logs\uwp-release-$arch.log;verbosity=normal",
            "/bl:artifacts\logs\uwp-release-$arch.binlog")
        if ($CertificateThumbprint) {
            $arguments += '/p:AppxPackageSigningEnabled=true',"/p:PackageCertificateThumbprint=$CertificateThumbprint"
        }
        & $MSBuild @arguments
        if ($LASTEXITCODE -ne 0) { throw "Release $arch build failed" }
        & "$PSScriptRoot\verify-uwp-package.ps1" -Architecture $arch
    }
} finally { Pop-Location }

