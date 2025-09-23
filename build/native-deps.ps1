Param(
    [ValidateSet('Debug','Release')]
    [string[]]$Configuration = @('Debug','Release'),
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake が PATH にありません。Visual Studio の開発者コマンドプロンプト等から実行してください。"
}

$repoRoot = Convert-Path (Join-Path $PSScriptRoot '..')
$secpRoot = Join-Path $repoRoot 'external\libsecp256k1'
if (-not (Test-Path (Join-Path $secpRoot '.git'))) {
    throw "libsecp256k1 submodule が存在しません。`git submodule update --init --recursive` を実行してください。"
}

$buildRoot = Join-Path $secpRoot 'build'
$packageRoot = Join-Path $repoRoot 'packages\native\libsecp256k1\x64'

if ($Clean) {
    if (Test-Path $buildRoot) {
        Remove-Item $buildRoot -Recurse -Force
    }
    if (Test-Path $packageRoot) {
        Remove-Item $packageRoot -Recurse -Force
    }
}

foreach ($config in ($Configuration | Sort-Object -Unique)) {
    $configDir = Join-Path $buildRoot $config
    $installDir = Join-Path $packageRoot $config
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
    $cmakeArgs = @(
        '-S', $secpRoot,
        '-B', $configDir,
        '-G', 'Visual Studio 17 2022',
        '-A', 'x64',
        '-DSECP256K1_ENABLE_MODULE_ECDH=ON',
        '-DSECP256K1_ENABLE_MODULE_SCHNORRSIG=ON',
        '-DSECP256K1_ENABLE_SHARED=OFF',
        '-DSECP256K1_BUILD_BENCHMARK=OFF',
        '-DSECP256K1_BUILD_TESTS=OFF',
        '-DCMAKE_INSTALL_PREFIX=' + $installDir
    )

    Write-Host "[native-deps] Configure $config"
    cmake @cmakeArgs | Out-Null

    Write-Host "[native-deps] Build $config"
    cmake --build $configDir --config $config | Out-Null

    Write-Host "[native-deps] Install $config"
    cmake --install $configDir --config $config | Out-Null

    $libPath = Join-Path $installDir 'lib\secp256k1.lib'
    if (-not (Test-Path $libPath)) {
        throw "secp256k1.lib が $libPath に見つかりません。"
    }
}
