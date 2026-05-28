# Copyright 2026 Tomas Mikalauskas
# SPDX-License-Identifier: Apache-2.0

param(
    [ValidateSet("RenderDoc", "Nsight", "Print")]
    [string] $Tool = "RenderDoc",

    [int] $SampleIndex = 2,
    [int] $Frames = 2,
    [switch] $ResizeSmoke,
    [switch] $NoValidation,
    [switch] $WhatIfCommand,
    [string] $BuildDir = "build/windows-debug",
    [string] $Configuration = "Debug",
    [string] $Output = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$sampleExe = Join-Path $repoRoot "build/windows-debug/samples/$Configuration/kera_samples.exe"
if ($BuildDir -ne "build/windows-debug") {
    $sampleExe = Join-Path $repoRoot "$BuildDir/samples/$Configuration/kera_samples.exe"
}

if (-not (Test-Path $sampleExe)) {
    throw "Missing sample executable: $sampleExe. Build first with cmake --build --preset windows-debug."
}

$sampleArgs = @("--smoke-test", "--smoke-frames", "$Frames", "--sample-index", "$SampleIndex")
if ($ResizeSmoke) {
    $sampleArgs += "--resize-smoke"
}

if (-not $NoValidation) {
    $env:VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation"
}

if ([string]::IsNullOrWhiteSpace($Output)) {
    $suffix = if ($ResizeSmoke) { "resize" } else { "frame" }
    $Output = Join-Path (Split-Path $sampleExe -Parent) "kera_sample_${SampleIndex}_${suffix}"
}

function Find-CommandPath {
    param(
        [string[]] $Names,
        [string[]] $Fallbacks
    )

    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }
    }

    foreach ($path in $Fallbacks) {
        if (Test-Path $path) {
            return $path
        }
    }

    return $null
}

function Format-CommandLine {
    param(
        [string] $Exe,
        [string[]] $Arguments
    )

    $quotedArgs = $Arguments | ForEach-Object {
        if ($_ -match "\s") {
            "`"$_`""
        } else {
            $_
        }
    }

    return "`"$Exe`" $($quotedArgs -join ' ')"
}

if ($Tool -eq "Print") {
    Write-Host "Sample command:"
    Write-Host (Format-CommandLine -Exe $sampleExe -Arguments $sampleArgs)
    Write-Host "VK_INSTANCE_LAYERS=$env:VK_INSTANCE_LAYERS"
    exit 0
}

if ($Tool -eq "RenderDoc") {
    $renderDoc = Find-CommandPath `
        -Names @("renderdoccmd.exe", "renderdoccmd") `
        -Fallbacks @("C:/Program Files/RenderDoc/renderdoccmd.exe")

    if (-not $renderDoc) {
        throw "renderdoccmd was not found. Install RenderDoc or add renderdoccmd to PATH."
    }

    $captureArgs = @("capture", "--wait-for-exit", "--output", $Output, $sampleExe) + $sampleArgs
    Write-Host (Format-CommandLine -Exe $renderDoc -Arguments $captureArgs)
    if (-not $WhatIfCommand) {
        & $renderDoc @captureArgs
    }
    exit $LASTEXITCODE
}

$nsight = Find-CommandPath `
    -Names @("ngfx.exe", "nsight-graphics.exe") `
    -Fallbacks @()

$nsightCommandPath = "$Output.nsight-launch.txt"
$nsightCommand = @(
    "Nsight Graphics capture target",
    "Executable: $sampleExe",
    "Arguments: $($sampleArgs -join ' ')",
    "Working directory: $repoRoot",
    "VK_INSTANCE_LAYERS=$env:VK_INSTANCE_LAYERS"
)
Set-Content -Path $nsightCommandPath -Value $nsightCommand

Write-Host "Wrote Nsight launch sheet: $nsightCommandPath"
if ($nsight) {
    Write-Host "Opening Nsight Graphics. Use the launch sheet values for the frame debugger target."
    if (-not $WhatIfCommand) {
        Start-Process -FilePath $nsight -WorkingDirectory $repoRoot
    }
} else {
    Write-Host "Nsight Graphics was not found on PATH. Open Nsight manually and use the launch sheet values."
}
