Param(
    [ValidateSet('Debug','Release')]
    [string[]]$Configuration = @('Debug','Release'),
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    $vsCandidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise'
    )
    foreach ($root in $vsCandidates) {
        $candidate = Join-Path $root 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path $candidate) {
            $cmakeDir = Split-Path $candidate -Parent
            if (-not ($env:Path.Split([IO.Path]::PathSeparator) -contains $cmakeDir)) {
                $env:Path = "$cmakeDir" + [IO.Path]::PathSeparator + $env:Path
            }
            $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
            if ($cmakeCommand) {
                break
            }
        }
    }
}

if (-not $cmakeCommand) {
    throw "cmake が PATH にありません。Visual Studio の開発者コマンドプロンプト等から実行してください。"
}

$repoRoot = Convert-Path (Join-Path $PSScriptRoot '..')
$nlohmannVersion = '3.11.3'
$nlohmannExpectedSha256 = '9BEA4C8066EF4A1C206B2BE5A36302F8926F7FDC6087AF5D20B417D0CF103EA6'
$nlohmannPackageRoot = Join-Path $repoRoot 'packages\native\nlohmann_json'
$nlohmannIncludeRoot = Join-Path $nlohmannPackageRoot 'include'
$nlohmannHeaderDir = Join-Path $nlohmannIncludeRoot 'nlohmann'
$nlohmannHeaderPath = Join-Path $nlohmannHeaderDir 'json.hpp'
$nlohmannDownloadUri = "https://github.com/nlohmann/json/releases/download/v$nlohmannVersion/json.hpp"

function Ensure-NlohmannJsonHeader {
    if (-not (Test-Path $nlohmannPackageRoot)) {
        New-Item -ItemType Directory -Path $nlohmannPackageRoot -Force | Out-Null
    }
    if (-not (Test-Path $nlohmannIncludeRoot)) {
        New-Item -ItemType Directory -Path $nlohmannIncludeRoot -Force | Out-Null
    }
    if (-not (Test-Path $nlohmannHeaderDir)) {
        New-Item -ItemType Directory -Path $nlohmannHeaderDir -Force | Out-Null
    }

    $needsDownload = $true
    if (Test-Path $nlohmannHeaderPath) {
        $existingHash = (Get-FileHash -Path $nlohmannHeaderPath -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($existingHash -eq $nlohmannExpectedSha256) {
            $needsDownload = $false
        }
        else {
            Remove-Item $nlohmannHeaderPath -Force
        }
    }

    if ($needsDownload) {
        $tempFile = [System.IO.Path]::GetTempFileName()
        try {
            Write-Host "[native-deps] Download nlohmann/json $nlohmannVersion"
            Invoke-WebRequest -Uri $nlohmannDownloadUri -OutFile $tempFile -UseBasicParsing | Out-Null
            $downloadedHash = (Get-FileHash -Path $tempFile -Algorithm SHA256).Hash.ToUpperInvariant()
            if ($downloadedHash -ne $nlohmannExpectedSha256) {
                throw "nlohmann/json $nlohmannVersion のダウンロード結果のハッシュが一致しません。期待: $nlohmannExpectedSha256 実測: $downloadedHash"
            }
            Move-Item -Path $tempFile -Destination $nlohmannHeaderPath -Force
        }
        catch {
            if (Test-Path $tempFile) {
                Remove-Item $tempFile -Force
            }
            throw "nlohmann/json $nlohmannVersion の json.hpp ダウンロードに失敗しました: $($_.Exception.Message)"
        }
    }
}

if ($Clean -and (Test-Path $nlohmannHeaderDir)) {
    Remove-Item $nlohmannHeaderDir -Recurse -Force
}

Ensure-NlohmannJsonHeader

$secpRoot = Join-Path $repoRoot 'external\libsecp256k1'
if (-not (Test-Path (Join-Path $secpRoot '.git'))) {
    throw "libsecp256k1 submodule が存在しません。`git submodule update --init --recursive` を実行してください。"
}

$ixRoot = Join-Path $repoRoot 'external\IXWebSocket'
if (-not (Test-Path (Join-Path $ixRoot '.git'))) {
    throw "IXWebSocket submodule が存在しません。`git submodule update --init --recursive` を実行してください。"
}

$secpBuildRoot = Join-Path $secpRoot 'build'
$secpPackageRoot = Join-Path $repoRoot 'packages\native\libsecp256k1\x64'
$ixBuildRoot = Join-Path $ixRoot 'build'
$ixPackageRoot = Join-Path $repoRoot 'packages\native\ixwebsocket\x64'

if ($Clean) {
    if (Test-Path $secpBuildRoot) {
        Remove-Item $secpBuildRoot -Recurse -Force
    }
    if (Test-Path $secpPackageRoot) {
        Remove-Item $secpPackageRoot -Recurse -Force
    }
    if (Test-Path $ixBuildRoot) {
        Remove-Item $ixBuildRoot -Recurse -Force
    }
    if (Test-Path $ixPackageRoot) {
        Remove-Item $ixPackageRoot -Recurse -Force
    }
}

foreach ($config in ($Configuration | Sort-Object -Unique)) {
    $configDir = Join-Path $secpBuildRoot $config
    $installDir = Join-Path $secpPackageRoot $config
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
    $installDirNormalized = ($installDir -replace '\\','/')
    $cmakeArgs = @(
        '-S', $secpRoot,
        '-B', $configDir,
        '-G', 'Visual Studio 17 2022',
        '-A', 'x64',
        '-DBUILD_SHARED_LIBS=OFF',
        '-DSECP256K1_ENABLE_MODULE_ECDH=ON',
        '-DSECP256K1_ENABLE_MODULE_EXTRAKEYS=ON',
        '-DSECP256K1_ENABLE_MODULE_SCHNORRSIG=ON',
        '-DSECP256K1_BUILD_BENCHMARK=OFF',
        '-DSECP256K1_BUILD_TESTS=OFF',
        '-DSECP256K1_BUILD_EXHAUSTIVE_TESTS=OFF',
        '-DSECP256K1_BUILD_CTIME_TESTS=OFF',
        "-DCMAKE_INSTALL_PREFIX=$installDirNormalized"
    )

    Write-Host "[native-deps] Configure libsecp256k1 $config"
    cmake @cmakeArgs | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed for libsecp256k1 ($config)"
    }

    Write-Host "[native-deps] Build libsecp256k1 $config"
    cmake --build $configDir --config $config | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed for libsecp256k1 ($config)"
    }

    Write-Host "[native-deps] Install libsecp256k1 $config"
    cmake --install $configDir --config $config | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "cmake install failed for libsecp256k1 ($config)"
    }

    $secpLibCandidates = @(
        (Join-Path -Path $installDir -ChildPath 'lib\libsecp256k1.lib'),
        (Join-Path -Path $installDir -ChildPath 'lib\secp256k1.lib')
    )
    $secpLibPath = $secpLibCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $secpLibPath) {
        throw "secp256k1 static library が $installDir に見つかりません。"
    }

    $ixConfigDir = Join-Path $ixBuildRoot $config
    $ixInstallDir = Join-Path $ixPackageRoot $config
    New-Item -ItemType Directory -Path $ixConfigDir -Force | Out-Null
    New-Item -ItemType Directory -Path $ixInstallDir -Force | Out-Null
    $ixInstallDirNormalized = ($ixInstallDir -replace '\\','/')
    $ixCmakeArgs = @(
        '-S', $ixRoot,
        '-B', $ixConfigDir,
        '-G', 'Visual Studio 17 2022',
        '-A', 'x64',
        '-DUSE_TLS=OFF',
        '-DUSE_ZLIB=OFF',
        '-DBUILD_DEMO=OFF',
        '-DIXWEBSOCKET_INSTALL=ON',
        "-DCMAKE_INSTALL_PREFIX=$ixInstallDirNormalized"
    )

    Write-Host "[native-deps] Configure IXWebSocket $config"
    cmake @ixCmakeArgs | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed for IXWebSocket ($config)"
    }

    Write-Host "[native-deps] Build IXWebSocket $config"
    cmake --build $ixConfigDir --config $config | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed for IXWebSocket ($config)"
    }

    Write-Host "[native-deps] Install IXWebSocket $config"
    cmake --install $ixConfigDir --config $config | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "cmake install failed for IXWebSocket ($config)"
    }

    $ixLibPath = Join-Path $ixInstallDir 'lib\ixwebsocket.lib'
    if (-not (Test-Path $ixLibPath)) {
        throw "ixwebsocket.lib が $ixLibPath に見つかりません。"
    }
}
