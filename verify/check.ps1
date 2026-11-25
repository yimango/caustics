# Rebuild main.cpp and run the wave-surface check. Exit with the check's status.
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

function Invoke-Checked([string[]]$Command) {
    & $Command[0] $Command[1..($Command.Length - 1)]
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Invoke-Checked @(
    'g++', '-std=c++17', '-O2', '-ffunction-sections', '-fdata-sections',
    '-Dmain=caustics_main', '-DM_PI=3.14159265358979323846',
    '-I', 'verify/stubs', '-I', 'include', '-I', 'external/glfw-3.4.bin.WIN64/include',
    '-c', 'main.cpp', '-o', 'verify/main_wave.o'
)
Invoke-Checked @('g++', '-std=c++17', '-O2', '-c', 'verify/wave_stability.cpp', '-o', 'verify/wave_stability.o')
Invoke-Checked @('gcc', '-c', 'include/glad/glad.c', '-o', 'verify/glad.o', '-I', 'include')
Invoke-Checked @('gcc', '-c', 'verify/glfw_link_stubs.c', '-o', 'verify/glfw_link_stubs.o')
Invoke-Checked @(
    'g++', '-O2',
    'verify/wave_stability.o', 'verify/main_wave.o', 'verify/glad.o', 'verify/glfw_link_stubs.o',
    '-lopengl32', '-o', 'verify/wave_stability.exe'
)

& .\verify\wave_stability.exe
exit $LASTEXITCODE
