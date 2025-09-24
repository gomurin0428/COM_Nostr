# 作業履歴

## 2025-09-23
- ネイティブ WebSocket ハンドシェイクテストが docker デーモン未起動時に長時間無言になる原因を調査し、TROUBLESHOOTING.md に対処手順を追記。
- COM_Nostr_Native の IDL で `IDispatch` を既定インターフェイスに設定する際の構文エラーを修正し、x64 ビルドが通ることを確認。
- COM_Nostr_NativePS に最小の `Stub.cpp` を追加し、自動生成ファイルに依存せずに DLL をリンクできるように変更。
- ソリューション設定を更新して `COM_Nostr_NativePS` を Debug/Release | x64 ビルド対象に含め、全体ビルドを確認。
- TEXT_FILE_OVERVIEW.md と TROUBLESHOOTING.md に今回の変更点と対処手順を追記。
- `dotnet test` を試行したが 0 件実行のまま終了したため、詳細は `UnitTest_COM_Nostr\bin\Debug\net8.0-windows\TestResults` のログを参照。
- COM_Nostr_Native のフェーズ別実装計画を再策定し、ImplementationPlan.md と TEXT_FILE_OVERVIEW.md を更新。
- msbuild で `COM_Nostr_Native\COM_Nostr_Native.vcxproj` (Debug|x64) をビルドし、現状のネイティブ DLL が成功裏にリンクされることを確認。
- Phase0 の成果物として `docs/native_port_overview.md` と `docs/native_sequence_diagrams.md` を新規作成し、移植対象クラスの依存関係・NIP 要件・シーケンス図を整備。
- WinHTTP の圧縮未対応など Native 実装特有の留意点を TROUBLESHOOTING.md に追記し、TEXT_FILE_OVERVIEW.md も更新。
- `msbuild COM_Nostr.sln /p:Configuration=Debug /p:Platform=x64` を実行し、既存 .NET / C++ プロジェクトのビルド成功を確認。
- COM_Nostr_Native のビルド設定を C++20 固定・警告をエラー扱いに変更し、プリコンパイルヘッダーに nlohmann/json 3.11.3 を取り込むように更新。
- `packages/native/nlohmann_json` を追加し、サブモジュール化した libsecp256k1 と合わせてネイティブ依存の取得・配置フローを整備。
- `build/native-deps.ps1` を新設して libsecp256k1 を Debug/Release x64 で CMake ビルド・インストールするスクリプトを整備し、README と TEXT_FILE_OVERVIEW を更新。
- `external/libsecp256k1/README.port.md` を作成し、ビルドオプションとスクリプトの利用手順を記録。
- COM_Nostr_Native フェーズ2の成果物として `NostrJsonSerializer` の AUTH expiresAt パースを `expiresAt`/文字列形式に拡張し、`Variant` 安全配列や JSON 変換の単体テスト (`SerializerTests`) を強化。ATL 依存のない `Fake*Dispatch` スタブと `_AtlModule` スタブ (`AtlModuleStub.cpp`) を追加。
- `msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration=Debug /p:Platform=x64` および Release 構成でビルドを通し、`NostrNativeTests.vcxproj` も Debug|x64 でビルド。`vstest.console` を用いて `tests/native/NostrNativeTests/x64/Debug/NostrNativeTests.dll` の6件テストを成功確認。
- ImplementationPlan.md をチェックボックス付き進捗に更新し、TEXT_FILE_OVERVIEW.md にネイティブテストの新規ファイル概要を追記。
- WinHTTP ベースの `NativeHttpClient` と `WinHttpWebSocket` を実装し、`NativeClientResources` でファクトリを束ねた。プロジェクト設定に `winhttp.lib` と `src/runtime` ディレクトリを追加。
- docker の strfry を動的ポートで起動する `WebSocketHandshakeTests` を追加し、REQ→EOSE の往復を確認するネイティブ MSTest を作成。テスト実行時には専用ボリュームディレクトリを生成して後片付けするようにした。
- README/TROUBLESHOOTING/TEXT_FILE_OVERVIEW を更新し、WinHTTP ハンドシェイク要件 (`Sec-WebSocket-Protocol: nostr`) や新規ファイル概要を追記。
- 2025-09-23T21:49:09+09:00 GitHub Issue #1 (WinHTTPベースWebSocketをIXWebSocketへ置き換える) を作成し、IXWebSocket移行計画を記載。
- 2025-09-23T21:53:53+09:00 TROUBLESHOOTING.md に pwsh 実行時のパースエラー回避手順を追記し、TEXT_FILE_OVERVIEW.md を最新化。
- 2025-09-23T22:11:30+09:00 IXWebSocket をサブモジュールとして導入するための依存ビルド確認 (`build/native-deps.ps1`) 実行準備。
- 2025-09-23T22:13:36+09:00 `build/native-deps.ps1 -Configuration Debug` を実行したが、PATH に cmake が見つからず失敗したことを確認。
- 2025-09-23T22:16:26+09:00 Visual Studio 同梱 CMake (`CommonExtensions/Microsoft/CMake`) を PATH へ一時追加して再実行予定。
- 2025-09-23T22:17:37+09:00 Debug 構成の `build/native-deps.ps1` を再実行したが、libsecp256k1 が共有ライブラリとして構成され `secp256k1.lib` が出力されなかったためスクリプトが失敗。
- 2025-09-23T22:18:26+09:00 `build/native-deps.ps1 -Clean -Configuration Debug` で静的ライブラリ再生成を試行予定 (BUILD_SHARED_LIBS=OFF を追加済み)。

### 2025-09-23 Codex 調査メモ

- ConnectsAndReceivesEndOfStoredEvents テストで発生しているハング再現のために、テストコードと WinHttpWebSocket::Receive ループの実装を確認。docker relay 起動処理およびメッセージキューのイベント待ちの挙動に注目。
- 2025-09-23T10:15Z WinHttpWebSocket::Receiveのキュー処理とConnectsAndReceivesEndOfStoredEventsテストのログ出力を確認。pendingType処理やResetEventの挙動を調査中。
- 2025-09-23T10:25Z ConnectsAndReceivesEndOfStoredEventsテストを20秒タイムアウトで別プロセス実行開始準備。docker状態はユーザー申告により正常稼働と判断。
- 2025-09-23T10:35Z ConnectsAndReceivesEndOfStoredEventsテストの再実行を準備。前回は成功したが再現性確認のため2回目を実施。
- 2025-09-23T12:09:30Z WinHttpWebSocket::ReceiveLoopのイベント制御とstrfryからのEOSE未受信時の挙動を再点検し、テストハング再現のための単独ケース実行(20秒タイムアウト/別プロセス)をこれから実施予定。
- 2025-09-23T12:12:23Z vstest.console 20秒タイムアウト実行でConnectsAndReceivesEndOfStoredEventsが完了せず、プロセスをKillしてTIMEOUT扱いになったことを確認。strfryコンテナは起動済み。
- 2025-09-23T12:20:13Z WinHttpWebSocket::ReceiveLoopでWinHTTPタイムアウトを非致命扱いに修正し、ConnectsAndReceivesEndOfStoredEventsテストを20秒タイムアウトで再実行予定。
- 2025-09-23T12:35:48Z ConnectsAndReceivesEndOfStoredEvents単体実行を再試行し、約6秒で成功することを確認。残置していたstrfryコンテナを手動停止し整理。
- 2025-09-23T12:37:51Z デバッグログ挿入を撤去した最終版でのConnectsAndReceivesEndOfStoredEvents再実行を開始予定。
- 2025-09-23T12:38:49Z デバッグ用ログを除去した最終版でもConnectsAndReceivesEndOfStoredEventsが約6秒で成功することを確認。

## 2025-09-24
- 2025-09-24T05:37:14+09:00 Serena MCPツールの動作確認としてプロジェクト COM_Nostr をアクティブ化し、serena__list_dir でルート構造を取得できることを確認。
- 2025-09-24T05:41:02+09:00 build/native-deps.ps1 の Debug/Release ビルドを再実行する前に CMake/PATH 状況を確認開始。
- 2025-09-24T05:41:55+09:00 Visual Studio 付属 cmake を PATH に追加して build/native-deps.ps1 を Debug/Release で実行予定。
- 2025-09-24T05:42:26+09:00 build/native-deps.ps1 実行で libsecp256k1 の extrakeys 無効化エラーが発生したため、CMake オプション修正が必要と判断。
- 2025-09-24T05:43:06+09:00 build/native-deps.ps1 に cmake パス自動検出および extrakeys 有効化を追加後、再実行を行う。
- 2025-09-24T05:45:10+09:00 cmake --install の出力を確認するため Debug 構成で手動インストールを試行。
- 2025-09-24T05:46:08+09:00 build/native-deps.ps1 -Clean -Configuration Debug,Release を再実行して自動化を検証。
- 2025-09-24T05:48:18+09:00 CMake install prefix の正規化と終了コード検査を build/native-deps.ps1 に追加。
- 2025-09-24T05:48:36+09:00 修正後の build/native-deps.ps1 を -Clean 付きで再実行し、libsecp256k1/IXWebSocket のビルド・配置確認。
- 2025-09-24T05:50:25+09:00 build/native-deps.ps1 の lib ファイル検証を libsecp256k1.lib / secp256k1.lib 両対応に修正。
- 2025-09-24T05:50:37+09:00 修正後の build/native-deps.ps1 を -Clean で再実行して成果物配置を確認。
- 2025-09-24T05:52:49+09:00 build/native-deps.ps1 のライブラリ検証修正後、再度 -Clean でビルドを実行。
- 2025-09-24T05:54:26+09:00 COM_Nostr_Native.vcxproj を Debug/Release|x64 で msbuild 実行し、IXWebSocket サブモジュール導入後のリンク確認を行う。
- 2025-09-24T05:55:17+09:00 COM_Nostr_Native.vcxproj (Debug/Release|x64) の msbuild が成功し、ixwebsocket.lib 連携後もビルドが通ることを確認。
- 2025-09-24T05:58:46+09:00 README.md/TROUBLESHOOTING.md を更新し、IXWebSocket サブモジュールのビルド手順と CMake 自動検出仕様を反映。
- 2025-09-24T05:59:04+09:00 external/IXWebSocket/README.port.md を新規作成し、追跡コミット・ビルドオプション・ライセンス確認結果を記録。
- 2025-09-24T06:05:43+09:00 IXWebSocket ポートメモを docs/IXWebSocket_port.md に移動し、README の参照先を更新。
- 2025-09-24T18:43:13+09:00 `dotnet test UnitTest_COM_Nostr/UnitTest_COM_Nostr.csproj -c Debug` をタイムアウト20秒で別プロセス実行開始予定。
- 2025-09-24T19:03:36+09:00 COM 参照ベースの統合テスト(NostrClient)を追加し、TROUBLESHOOTING.md に Type Library 未登録時の対処を追記。ローカル環境では TLB 未登録のため `msbuild`/`dotnet test` は失敗することを確認。
- 2025-09-24T19:16:11+09:00 build/native-deps.ps1 で nlohmann/json 3.11.3 の json.hpp を再取得する準備を開始。
- 2025-09-24T19:16:44+09:00 build/native-deps.ps1 -Configuration Debug を実行し、nlohmann/json 3.11.3 の json.hpp とネイティブ依存を取得済みであることを確認。
- 2025-09-24T19:17:01+09:00 COM_Nostr_Native.vcxproj (Debug|x64) の msbuild を実行し、json.hpp 取得後のビルドを確認予定。
- 2025-09-24T19:17:27+09:00 msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration=Debug /p:Platform=x64 が成功し、json.hpp 取得後もビルドエラーが発生しないことを確認。
- 2025-09-24T19:17:39+09:00 COM_Nostr_Native.vcxproj (Release|x64) の msbuild を追加で実行し、両構成でのビルド完了を確認予定。
- 2025-09-24T19:18:44+09:00 Release|x64 ビルドは依存ライブラリ (ixwebsocket/libsecp256k1) の Release アーティファクト未生成により失敗したため、native-deps.ps1 を Release 構成で再実行する必要があると判断。
- 2025-09-24T19:18:56+09:00 build/native-deps.ps1 -Configuration Release を実行し、Release 用 ixwebsocket/libsecp256k1 を整備予定。
- 2025-09-24T19:19:16+09:00 build/native-deps.ps1 -Configuration Release が成功し、Release 向け依存ライブラリを配置完了。
- 2025-09-24T19:19:26+09:00 依存生成後に COM_Nostr_Native.vcxproj (Release|x64) の msbuild を再実行予定。
- 2025-09-24T19:19:43+09:00 msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration=Release /p:Platform=x64 が成功し、Debug/Release の両構成でビルド可能になったことを確認。
- 2025-09-24T20:20:39+09:00 COM_Nostr_Native.rgs 更新後の確認として、`msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration=Debug /p:Platform=x64` をタイムアウト20秒で別プロセス実行開始予定。
- 2025-09-24T20:21:21+09:00 `desktop-commander__start_process` 経由の PowerShell 起動が 0x8009001d で失敗したため、`shell` コマンドで同一ビルドを再実行予定。
- 2025-09-24T20:21:33+09:00 `shell` で `msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration=Debug /p:Platform=x64` をタイムアウト20秒で実行開始。
- 2025-09-24T20:21:56+09:00 `msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration=Debug /p:Platform=x64` が 1.63 秒で成功し、rgs 更新後もビルドが通ることを確認。
