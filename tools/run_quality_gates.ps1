param(
    [string]$BuildDirectory = "",
    [string]$Generator = "",
    [switch]$SkipFirmware,
    [string]$KeilPath = "",
    [int]$FirmwareTimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot "build\quality"
}
elseif (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot $BuildDirectory
}

if ([string]::IsNullOrWhiteSpace($Generator)) {
    $mingwMake = Get-Command "mingw32-make" -ErrorAction SilentlyContinue
    if ($null -ne $mingwMake) {
        $Generator = "MinGW Makefiles"
    }
}

Write-Host "Validating repository structure and documentation..."
& python "-B" (Join-Path $PSScriptRoot "validate_project.py")
if ($LASTEXITCODE -ne 0) {
    throw "Repository validation failed."
}

Write-Host "Configuring host tests..."
$configureArguments = @(
    "-S",
    $repositoryRoot,
    "-B",
    $BuildDirectory,
    "-DCMAKE_BUILD_TYPE=Release"
)
if (-not [string]::IsNullOrWhiteSpace($Generator)) {
    $configureArguments += @("-G", $Generator)
}
& cmake $configureArguments
if ($LASTEXITCODE -ne 0) {
    throw "Host test configuration failed."
}

Write-Host "Building host tests..."
& cmake "--build" $BuildDirectory "--config" "Release"
if ($LASTEXITCODE -ne 0) {
    throw "Host test build failed."
}

Write-Host "Running host tests..."
& ctest `
    "--test-dir" $BuildDirectory `
    "--build-config" "Release" `
    "--output-on-failure"
if ($LASTEXITCODE -ne 0) {
    throw "Host tests failed."
}

if (-not $SkipFirmware) {
    Write-Host "Building both firmware targets..."
    $firmwareBuild = Join-Path $PSScriptRoot "build_firmware.ps1"
    if ([string]::IsNullOrWhiteSpace($KeilPath)) {
        & $firmwareBuild -TimeoutSeconds $FirmwareTimeoutSeconds
    }
    else {
        & $firmwareBuild `
            -KeilPath $KeilPath `
            -TimeoutSeconds $FirmwareTimeoutSeconds
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Firmware build failed."
    }
}

Write-Host "All selected quality gates passed."
