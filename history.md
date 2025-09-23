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
