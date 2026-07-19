# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

$files = git ls-files "*.c" "*.cc" "*.cpp" "*.cxx" "*.h" "*.hpp" ":(exclude)third_party/" ":(exclude)*.slang.h"

if (-not $files) {
    Write-Host "No C/C++ source files to analyze."
    exit 0
}

$formattedFiles = @()
foreach ($f in $files) {
    Write-Host "Running clang-format on $f"
    $contentBeforeFormatting = Get-Content -Raw -LiteralPath $f
    clang-format -i "$f"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "clang-format failed on $f"
        exit 1
    }

    if((Get-Conntent -Raw -LiteralPath $f) -cne $contentBeforeFormatting) {
        $formattedFiles += $f
    }
}

if ($formattedFiles.count -gt 0) {
    Write-Error "clang-format updated the following files. Commit the formatting chages: " + ($formattedFiles -join ", ")
    exit 1
}
