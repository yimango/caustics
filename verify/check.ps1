$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

function Invoke-Checked([string[]]$Command) {
    & $Command[0] $Command[1..($Command.Length - 1)]
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Invoke-Checked @('g++', '-std=c++17', '-O2', '-I', 'src', '-c', 'src/sim/wave_sim.cpp', '-o', 'verify/wave_sim.o')
Invoke-Checked @('g++', '-std=c++17', '-O2', '-I', 'src', '-c', 'verify/wave_stability.cpp', '-o', 'verify/wave_stability.o')
Invoke-Checked @('g++', '-O2', 'verify/wave_stability.o', 'verify/wave_sim.o', '-o', 'verify/wave_stability.exe')

& .\verify\wave_stability.exe
exit $LASTEXITCODE
