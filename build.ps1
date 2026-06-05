# Build libtama for every requested platform/target combination.
#
# Usage:
#   .\build.ps1 [OPTIONS]
#
# Options:
#   -Platform  Comma-separated platforms to build (windows,linux,web). Default: all three.
#   -Target    Comma-separated targets to build (debug,release). Default: both.
#   -Jobs      Parallel compile jobs passed to scons. Default: CPU count.
#
# Web builds require the Emscripten SDK to be activated first:
#   emsdk activate latest
#
# Examples:
#   .\build.ps1                               # build everything
#   .\build.ps1 -Platform windows             # windows debug + release only
#   .\build.ps1 -Platform windows,web -Target release
param(
    [string]$Platform = "windows,linux,web",
    [string]$Target   = "debug,release",
    [int]   $Jobs     = [Environment]::ProcessorCount
)

$ErrorActionPreference = "Stop"
$NativeDir = Join-Path $PSScriptRoot "addons\tama\native"

function Scons-Target($t) {
    switch ($t) {
        "debug"   { "template_debug" }
        "release" { "template_release" }
        default   { $t }
    }
}

$platforms = $Platform -split "," | ForEach-Object { $_.Trim() }
$targets   = $Target   -split "," | ForEach-Object { $_.Trim() }

$pass = @()
$fail = @()

Push-Location $NativeDir
try {
    foreach ($p in $platforms) {
        foreach ($t in $targets) {
            $st    = Scons-Target $t
            $label = "$p/$t"

            Write-Host ""
            Write-Host ("━" * 50)
            Write-Host "  Building: $label"
            Write-Host ("━" * 50)

            scons platform=$p target=$st -j$Jobs
            if ($LASTEXITCODE -eq 0) {
                $pass += $label
            } else {
                $fail += $label
            }
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host ("━" * 50)
Write-Host "  Build summary"
Write-Host ("━" * 50)
foreach ($l in $pass) { Write-Host "  OK  $l" }
foreach ($l in $fail) { Write-Host "  !!  $l" }

if ($fail.Count -gt 0) { exit 1 }
