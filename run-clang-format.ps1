$files = git ls-files "*.c" "*.cc" "*.cpp" "*.cxx" "*.h" "*.hpp" ":(exclude)third_party/" ":(exclude)*.slang.h"

if (-not $files) {
    Write-Host "No C/C++ source files to analyze."
    exit 0
}

foreach ($f in $files) {
    Write-Host "Running clang-format on $f"
    clang-format -i "$f"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "clang-format failed on $f"
        exit 1
    }
}

git diff --exit-code
if ($LASTEXITCODE -ne 0) {
    Write-Error "clang-format check failed: some files are not properly formatted. Please run clang-format locally and commit the changes."
    exit 1
}
