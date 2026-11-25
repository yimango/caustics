$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

$slash = [string][char]47
$lineMark = $slash + $slash
$blockMark = $slash + '*'

$targets = @(
    'main.cpp',
    'shaders.h',
    'CMakeLists.txt',
    'verify\wave_stability.cpp',
    'verify\glfw_link_stubs.c',
    'verify\check.ps1',
    'include\glad\glad.h',
    'include\glad\glad.c'
) + @(Get-ChildItem -Recurse -File verify\stubs | ForEach-Object { $_.FullName.Substring((Get-Location).Path.Length + 1) })

foreach ($relative in $targets) {
    $lines = Get-Content -LiteralPath $relative
    for ($n = 0; $n -lt $lines.Count; $n++) {
        $text = $lines[$n]
        $hit = $false
        if ($relative -eq 'CMakeLists.txt') {
            if ($text -match '^\s*#') { $hit = $true }
        } elseif ($text.Contains($lineMark) -or $text.Contains($blockMark)) {
            $hit = $true
        }
        if ($hit) {
            Write-Error ("comment in {0}:{1}" -f $relative, ($n + 1))
            exit 1
        }
    }
}

function Invoke-Checked([string[]]$Command) {
    & $Command[0] $Command[1..($Command.Length - 1)]
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Invoke-Checked @(
    'g++', '-std=c++17', '-O2', '-ffunction-sections', '-fdata-sections',
    '-Dmain=caustics_main',
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
