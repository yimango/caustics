param(
    [switch]$SkipBoundary,
    [string]$GlmInclude = ""
)

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

function Invoke-Checked([string[]]$Command) {
    & $Command[0] $Command[1..($Command.Length - 1)]
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ([string]::IsNullOrWhiteSpace($GlmInclude)) {
    if (-not [string]::IsNullOrWhiteSpace($env:GLM_INCLUDE_DIR)) {
        $GlmInclude = $env:GLM_INCLUDE_DIR
    } elseif (Test-Path 'external/glm/glm/glm.hpp') {
        $GlmInclude = 'external/glm'
    } else {
        $GlmInclude = 'external/glm'
    }
}

Write-Host "Building verification executables..."
Invoke-Checked @('g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I', 'src', '-c', 'src/sim/wave_sim.cpp', '-o', 'verify/wave_sim.o')

Invoke-Checked @('g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I', 'src', '-c', 'verify/wave_stability.cpp', '-o', 'verify/wave_stability.o')
Invoke-Checked @('g++', '-O2', 'verify/wave_stability.o', 'verify/wave_sim.o', '-o', 'verify/wave_stability.exe')

Invoke-Checked @('g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I', 'src', '-c', 'verify/boundary_tests.cpp', '-o', 'verify/boundary_tests.o')
Invoke-Checked @('g++', '-O2', 'verify/boundary_tests.o', 'verify/wave_sim.o', '-o', 'verify/boundary_tests.exe')

Invoke-Checked @('g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I', 'src', '-I', $GlmInclude, '-c', 'verify/optics_tests.cpp', '-o', 'verify/optics_tests.o')
Invoke-Checked @('g++', '-O2', 'verify/optics_tests.o', '-o', 'verify/optics_tests.exe')

Write-Host "Running wave_stability..."
Invoke-Checked @('.\verify\wave_stability.exe')

Write-Host "Running optics_tests..."
Invoke-Checked @('.\verify\optics_tests.exe')

if ($SkipBoundary) {
    Write-Host "Skipping boundary_tests (-SkipBoundary specified; binary compiled cleanly)."
} else {
    Write-Host "Running boundary_tests..."
    Invoke-Checked @('.\verify\boundary_tests.exe')
}

exit 0
