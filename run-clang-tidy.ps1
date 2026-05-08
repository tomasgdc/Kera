$buildDir = "build/clang-tidy"
$runLocal = $false

foreach ($arg in $args) {
    if ($arg -eq "--local") {
        $runLocal = $true
    } elseif ($arg.StartsWith("--build-dir=")) {
        $buildDir = $arg.Substring("--build-dir=".Length)
    }
}

$files = git ls-files "*.c" "*.cc" "*.cpp" "*.cxx" "*.cu" ":(exclude)third_party/" ":(exclude)*.slang.h"

if (-not $files) {
    Write-Host "No C/C++ source files to analyze."
    exit 0
}

if ($runLocal) {
    Write-Host "Running in Local Mode: configuring $buildDir with Ninja."
    if ($IsWindows -or $env:OS -eq "Windows_NT") {
        $vsInstallPath = "C:\Program Files\Microsoft Visual Studio\2022\Professional"
        $devShellModule = Join-Path $vsInstallPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
        if (Test-Path $devShellModule) {
            Import-Module $devShellModule -Scope Local
            Enter-VsDevShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation -VsInstallPath $vsInstallPath >$null
        }
    }

    cmake -S . -B "$buildDir" -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DKERA_BUILD_SAMPLES=ON
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configure failed."
        exit 1
    }
}

if (-not (Test-Path (Join-Path $buildDir "compile_commands.json"))) {
    Write-Error "compile_commands.json not found in $buildDir. Run with --local or pass --build-dir=<dir>."
    exit 1
}

$headerFilter = "^(include|src|samples)/.*\.(h|hpp)$"

$failed = 0
foreach ($f in $files) {
    $abs = (Resolve-Path $f).Path

    Write-Host "Running clang-tidy on $f"
    clang-tidy "$abs" -p "$buildDir" -header-filter="$headerFilter"

    if ($LASTEXITCODE -ne 0) {
        $failed = 1
    }
}

if ($failed -ne 0) {
    Write-Error "clang-tidy reported issues."
    exit 1
} else {
    Write-Host "clang-tidy passed on all files."
}
