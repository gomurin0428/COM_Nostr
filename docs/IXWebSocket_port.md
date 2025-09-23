# IXWebSocket ポートメモ

- 上流リポジトリ: https://github.com/machinezone/IXWebSocket
- 追跡コミット: 150e3d83b5f6a2a47f456b79330a9afe87cd379c (v11.4.6 からの 3 コミット先)
- ライセンス: BSD-3-Clause (`external/IXWebSocket/LICENSE.txt`)。配布物にはライセンス文の同梱が必要。
- ビルド手順: `build/native-deps.ps1` が CMake ジェネレータ `Visual Studio 17 2022` / `x64` を利用して静的ライブラリ `packages/native/ixwebsocket/x64/<Config>/lib/ixwebsocket.lib` を生成する。TLS (`USE_TLS`) と Zlib (`USE_ZLIB`) は既定で無効化。
- TLS や permessage-deflate を利用する場合は OpenSSL/MbedTLS/Zlib を個別に用意し、`build/native-deps.ps1` の `ixCmakeArgs` に対応する CMake オプションを追加して再ビルドすること。
- サブモジュール更新時は `git submodule update --remote external/IXWebSocket` で最新コミットを確認し、本ファイルのコミットハッシュと `build/native-deps.ps1` のオプション差分を再確認する。
