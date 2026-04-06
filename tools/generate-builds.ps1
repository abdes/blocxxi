<#
.SYNOPSIS
Generate both Ninja and Visual Studio build trees.
#>
param(
    [Parameter(Position = 0)]
    [Alias("ProfileHost")]
    [string]$BuildProfile,
    [string]$Build = "missing",
    [string]$DeployerFolder = "out/install",
    [string]$DeployerPackage = "blocxxi/0.1.0",
    [switch]$NoClean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter()][string[]]$Arguments = @()
    )

    Write-Host "> $Command $($Arguments -join ' ')" -ForegroundColor DarkGray
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command $($Arguments -join ' ')"
    }
}

function Resolve-RepoPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
}

function Test-ProfileIsAsan {
    param([Parameter(Mandatory = $true)][string]$ProfilePath)

    return [bool](Select-String -Path $ProfilePath -Pattern "^\s*sanitizer\s*=\s*asan\s*$" -Quiet)
}

function Get-ProfileSettingValue {
    param(
        [Parameter(Mandatory = $true)][string]$ProfilePath,
        [Parameter(Mandatory = $true)][string]$SettingName
    )

    $match = Select-String -Path $ProfilePath -Pattern "^\s*$([regex]::Escape($SettingName))\s*=\s*(.+?)\s*$" | Select-Object -First 1
    if ($null -eq $match) {
        return $null
    }

    return $match.Matches[0].Groups[1].Value.Trim()
}

function Get-HostPlatformName {
    if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)) {
        return "Windows"
    }

    if ([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Linux)) {
        return "Linux"
    }

    return $null
}

function Get-DefaultNormalProfilePath {
    $hostPlatform = Get-HostPlatformName

    if ($hostPlatform -eq "Windows") {
        return Resolve-RepoPath "profiles/windows-msvc.ini"
    }

    if ($hostPlatform -eq "Linux") {
        return Resolve-RepoPath "profiles/linux-clang.ini"
    }

    throw "Unsupported host OS. This script currently supports Windows and Linux only."
}

function Get-ProfilePair {
    param([string]$RequestedProfile)

    $normalProfilePath = if ([string]::IsNullOrWhiteSpace($RequestedProfile)) {
        Get-DefaultNormalProfilePath
    } else {
        Resolve-RepoPath $RequestedProfile
    }

    if (-not (Test-Path $normalProfilePath)) {
        throw "Profile not found: $normalProfilePath"
    }

    $requestedIsAsan = Test-ProfileIsAsan $normalProfilePath
    if ($requestedIsAsan) {
        $asanProfilePath = $normalProfilePath
        $normalProfilePath = $normalProfilePath -replace '-asan\.ini$', '.ini'
    } else {
        $asanProfilePath = $normalProfilePath -replace '\.ini$', '-asan.ini'
    }

    if (-not (Test-Path $normalProfilePath)) {
        throw "Normal profile not found: $normalProfilePath"
    }

    if (-not (Test-Path $asanProfilePath)) {
        throw "ASan profile not found: $asanProfilePath"
    }

    if (Test-ProfileIsAsan $normalProfilePath) {
        throw "Normal profile must not enable sanitizer=asan: $normalProfilePath"
    }

    if (-not (Test-ProfileIsAsan $asanProfilePath)) {
        throw "ASan profile must enable sanitizer=asan: $asanProfilePath"
    }

    $hostPlatform = Get-HostPlatformName
    if ($null -eq $hostPlatform) {
        throw "Unsupported host OS. This script currently supports Windows and Linux only."
    }

    foreach ($profilePath in @($normalProfilePath, $asanProfilePath)) {
        $profileOs = Get-ProfileSettingValue -ProfilePath $profilePath -SettingName "os"
        if ([string]::IsNullOrWhiteSpace($profileOs)) {
            throw "Profile does not declare an 'os' setting: $profilePath"
        }

        if ($profileOs -ne $hostPlatform) {
            throw "Profile OS '$profileOs' does not match host OS '$hostPlatform': $profilePath"
        }
    }

    return @{
        Normal = $normalProfilePath
        Asan = $asanProfilePath
    }
}

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
$profilePair = Get-ProfilePair $BuildProfile
$normalProfilePath = $profilePair.Normal
$asanProfilePath = $profilePair.Asan

$buildPlans = @(
    @{
        Name = "normal"
        HostProfilePath = $normalProfilePath
        IsAsan = $false
        Sanitizer = "none"
        Suffix = ""
        Configurations = @("Debug", "Release")
    },
    @{
        Name = "asan"
        HostProfilePath = $asanProfilePath
        IsAsan = $true
        Sanitizer = "asan"
        Suffix = "asan-"
        Configurations = @("Debug")
    }
)

$hostPlatform = Get-HostPlatformName
$isWindowsHost = $hostPlatform -eq "Windows"

Write-Host "Detected host OS: $hostPlatform" -ForegroundColor Cyan
Write-Host "Normal Conan profile: $normalProfilePath" -ForegroundColor Cyan
Write-Host "ASan Conan profile:   $asanProfilePath" -ForegroundColor Cyan

$pushedLocation = $false
try {
    Push-Location $repoRoot
    $pushedLocation = $true

    if (-not $NoClean) {
        foreach ($plan in $buildPlans) {
            foreach ($buildDir in @("out/build-$($plan.Suffix)vs", "out/build-$($plan.Suffix)ninja")) {
                $buildPath = Join-Path $repoRoot $buildDir
                if (Test-Path $buildPath) {
                    Remove-Item $buildPath -Recurse -Force
                }
            }
        }
    }

    $conanCommonArgs = @("--build=$Build", "--deployer-folder=$DeployerFolder", "--deployer-package=$DeployerPackage", "-c", "tools.cmake.cmakedeps:new=will_break_next", "-c", "tools.info.package_id:confs=['user.nova:sanitizer']")

    Write-Host "=== Phase 1: Conan installs for Ninja Multi-Config ===" -ForegroundColor Cyan
    foreach ($plan in $buildPlans) {
        $conanBaseArgs = @("install", ".", "--profile:host=$($plan.HostProfilePath)", "--profile:build=$($plan.HostProfilePath)") + $conanCommonArgs + @("-c", "user.nova:sanitizer=$($plan.Sanitizer)")
        foreach ($config in $plan.Configurations) {
            Invoke-External "conan" ($conanBaseArgs + @("-c", "tools.cmake.cmaketoolchain:generator=Ninja Multi-Config", "-s", "build_type=$config"))
        }
    }

    if ($isWindowsHost) {
        Write-Host "=== Phase 2: Conan installs for Visual Studio 18 ===" -ForegroundColor Cyan
        foreach ($plan in $buildPlans) {
            $conanBaseArgs = @("install", ".", "--profile:host=$($plan.HostProfilePath)", "--profile:build=$($plan.HostProfilePath)") + $conanCommonArgs + @("-c", "user.nova:sanitizer=$($plan.Sanitizer)")
            foreach ($config in $plan.Configurations) {
                Invoke-External "conan" ($conanBaseArgs + @("-c", "tools.cmake.cmaketoolchain:generator=Visual Studio 18 2026", "-s", "build_type=$config"))
            }
        }
    } else {
        Write-Host "=== Phase 2: Visual Studio Conan installs skipped on non-Windows host ===" -ForegroundColor Yellow
    }

    Write-Host "=== Phase 3: Configure Ninja trees ===" -ForegroundColor Cyan
    $ninjaPresets = if ($isWindowsHost) { @("windows-default", "windows-asan") } else { @("linux", "linux-asan") }
    foreach ($preset in $ninjaPresets) {
        Invoke-External "cmake" @("--preset", $preset)
    }

    if ($isWindowsHost) {
        Write-Host "=== Phase 4: Configure Visual Studio trees ===" -ForegroundColor Cyan
        foreach ($plan in $buildPlans) {
            $vsBuildDir = "out/build-$($plan.Suffix)vs"
            $vsToolchain = Join-Path $repoRoot "$vsBuildDir/generators/conan_toolchain.cmake"
            Invoke-External "cmake" @("-G", "Visual Studio 18 2026", "-A", "x64", "-S", $repoRoot, "-B", (Join-Path $repoRoot $vsBuildDir), "-D", "CMAKE_TOOLCHAIN_FILE=$vsToolchain")
        }
    } else {
        Write-Host "=== Phase 4: Visual Studio configure skipped on non-Windows host ===" -ForegroundColor Yellow
    }
}
finally {
    if ($pushedLocation) {
        Pop-Location
    }
}
