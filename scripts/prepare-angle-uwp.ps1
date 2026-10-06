param([string]$Package = 'C:\Users\ASUS\source\repos\angle\artifacts\angle-uwp-release')
$ErrorActionPreference = 'Stop'
$destination = Join-Path $PSScriptRoot '..\third_party\angle-uwp'
if (!(Test-Path "$Package\SHA256SUMS.txt")) { throw "Missing ANGLE package: $Package" }
foreach ($line in Get-Content "$Package\SHA256SUMS.txt") {
    if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') { throw "Invalid checksum entry: $line" }
    $expected = $Matches[1]
    $file = Join-Path $Package $Matches[2]
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $expected) { throw "Checksum mismatch: $file" }
}
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item "$Package\*" $destination -Recurse -Force
Write-Host "Verified and copied ANGLE UWP package to $destination"
