param(
    [string]$KeilPath = "E:\STM32_Keil_Free\Keil_v5\UV4\UV4.exe",
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$targets = @("Master_MCU", "Slave_MCU")

if (-not (Test-Path -LiteralPath $KeilPath -PathType Leaf)) {
    throw "Keil executable not found: $KeilPath"
}

function Invoke-FirmwareBuild {
    param([string]$TargetDirectory)

    $workingDirectory = Join-Path $repositoryRoot $TargetDirectory
    $projectPath = Join-Path $workingDirectory "Project.uvprojx"
    $objectsDirectory = Join-Path $workingDirectory "Objects"
    $logPath = Join-Path $objectsDirectory "engineering_build.log"

    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        throw "Keil project not found: $projectPath"
    }
    if (-not (Test-Path -LiteralPath $objectsDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $objectsDirectory | Out-Null
    }
    if (Test-Path -LiteralPath $logPath) {
        Remove-Item -LiteralPath $logPath -Force
    }

    Write-Host "Building $TargetDirectory..."
    Start-Process `
        -FilePath $KeilPath `
        -ArgumentList @(
            "-b",
            "Project.uvprojx",
            "-j0",
            "-o",
            "Objects\engineering_build.log"
        ) `
        -WorkingDirectory $workingDirectory `
        -WindowStyle Hidden | Out-Null

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $buildOutput = ""
    do {
        if (Test-Path -LiteralPath $logPath) {
            $buildOutput = Get-Content -LiteralPath $logPath -Raw
            if ($buildOutput -match "\d+ Error\(s\), \d+ Warning\(s\)") {
                break
            }
        }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)

    if ($buildOutput -notmatch "0 Error\(s\), 0 Warning\(s\)") {
        if ($buildOutput) {
            Write-Host $buildOutput
        }
        throw "$TargetDirectory build failed or timed out."
    }

    $summary = $buildOutput -split "`r?`n" |
        Where-Object {
            $_ -match "Program Size:" -or
            $_ -match "0 Error\(s\), 0 Warning\(s\)"
        }
    $summary | ForEach-Object { Write-Host "  $_" }
}

foreach ($target in $targets) {
    Invoke-FirmwareBuild -TargetDirectory $target
}

Write-Host "Both firmware targets passed with zero errors and zero warnings."
