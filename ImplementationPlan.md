# COM_Nostr_Native 実装計画 (2025-09-23)

## 目的
- 既存 .NET 実装の挙動を踏襲しつつ、`COM_Nostr_Native` の ATL/C++ 実装を完成させる。
- README.md に記載された COM インターフェイスと HRESULT 契約を C++ レイヤーでも完全再現し、クライアントからの移行をシームレスにする。
- docker + strfry を用いた統合テストを準備し、COM_Nostr_Native を単独でビルド・テスト・配布できる状態に仕上げる。

## 前提・制約
- COM インターフェイス定義は `COM_Nostr_Native/COMNostrNative.idl` を唯一のソースとし、IID/CLSID を .NET 版と共有する。
- 外部依存は Windows 標準 API (WinHTTP WebSocket / CNG) とヘッダオンリーライブラリ (`nlohmann::json`) を基本とし、暗号処理では `libsecp256k1` (module=schnorrsig 有効) をソースビルドでバンドルする。
- フォールバック実装は禁止。機能が未完成な段階では COM API をエクスポートしない、もしくは HRESULT を `E_NOTIMPL` で明示する。
- マルチスレッド COM 呼び出しに対しても determinism を保つため、.NET 版の `ComCallbackDispatcher` と同等のシリアライズ機構を C++ で実装する。
- 署名者 (`INostrSigner`) は外部 COM 実装に委譲し、COM_Nostr_Native 内では署名を生成しない。

## 実装方針の要点
- `.NET` 版の責務分割 (`COM_Nostr/Contracts/NostrClient.cs`, `Internal/*`) を 1:1 で C++ クラスに写経し、レイヤー構造 (リソース、セッション、サブスクリプション、ユーティリティ) を維持する。
- C++ 側では `std::shared_ptr` と `winrt::handle` ではなく、ATL の `CComAutoCriticalSection` / `CComPtr` と RAII ラッパークラスを用いて COM ライフサイクル管理を簡潔化する。
- HTTP (NIP-11 取得) は WinHTTP の同期 API、WebSocket は WinHTTP WebSocket ハンドル + バックグラウンドワーカーで実装し、受信ループは `Threadpool work` + `concurrency::concurrent_queue` により .NET 版の Task 並列実行を模倣する。
- JSON パース/生成は `nlohmann::json` を利用し、DTO クラスから JSON 文字列を構築する際に COM 文字列を UTF-8 に変換するヘルパーを共通化する。
- HRESULT テーブルは .NET 版の `COM_Nostr/Internal/HResults.cs` に合わせて `include/NostrHResults.h` を新設し、共通エラー処理を保持する。

## フェーズ別計画
### フェーズ0: 既存資産の棚卸しと仕様確定 (2日)
- `COM_Nostr/Contracts` 配下の主要クラスを洗い出し、移植対象のメソッドと依存関係をクラス図にまとめる。
- README.md と `Nostrプロトコルの現行仕様まとめ.docx` を読み合わせ、Native 実装で必要な NIP (01/11/15/20/42/65) の最小要件を明文化する。
- Nostr イベント／フィルタの JSON 期待値を .NET 実装のユニットテストから抽出し、C++ で再利用できるサンプルデータを作成する。->これはユニットテストが消えているので無理。
- 成果物: `docs/native_port_overview.md` (移植方針)、`docs/native_sequence_diagrams.md` (必要なら.Mermaidで書く。) 。

### フェーズ1: ビルド環境と依存ライブラリ整備 (3日)
- `COM_Nostr_Native.vcxproj` に C++20 / マルチバイト禁止 (Unicode 固定) / Treat warnings as errors を設定。
- `external/` ディレクトリを作成し `libsecp256k1` をサブモジュールとして追加、x64 Release/Debug の静的ライブラリをビルドする PowerShell スクリプトを用意。
- `packages/native` に `nlohmann::json` を追加し、プリコンパイルヘッダで include。
- CI 手順として `msbuild COM_Nostr_Native.vcxproj /p:Configuration=Debug;Platform=x64` と `Release` の両方が通ることを確認する。
- 成果物: `build/native-deps.ps1`, `external/libsecp256k1/README.port.md`。

### フェーズ2: 共通ユーティリティと DTO 層の実装 (4日)
- COM 文字列⇔UTF-8 変換、`SAFEARRAY` ハンドリング、`VARIANT` ラッパーをまとめた `include/ComValueHelpers.h` を実装。
- DTO (`NostrEvent`, `NostrFilter`, `RelayDescriptor` など) を ATL の `IDispatchImpl` で実装し、.NET のプロパティと同名のプロパティを expose。
- DTO ↔ JSON の変換ヘルパー (`NostrJsonSerializer`) を C++ で実装し、.NET の `COM_Nostr/Internal/NostrJsonSerializer.cs` と同等のフィールド名／バリデーションを再現。
- 成果物: DTO クラス、`tests/native/NostrJsonSerializerTests.cpp`。

### フェーズ3: ランタイム基盤 (HTTP/WebSocket/リソース管理) (5日)
- WinHTTP を用いた `NativeHttpClient` を実装し、NIP-11 ドキュメント取得と JSON パースをサポート。
- WebSocket 接続を抽象化する `INativeWebSocket` インターフェイスと `WinHttpWebSocket` 実装を作成。受信スレッドは `Threadpool` + `OVERLAPPED` を使用し、メッセージを安全にキューイングする。
- `.NET` 版 `NostrClientResources` に合わせて `NativeClientResources` を作成し、HTTP/WebSocket/シリアライザ/タイマーのファクトリを保持。
- 成果物: `src/runtime/NativeHttpClient.cpp`, `src/runtime/WinHttpWebSocket.cpp`, `tests/native/WebSocketHandshakeTests.cpp` (モックサーバーで接続確認)。

### フェーズ4: リレーセッション管理 (`INostrRelaySession`) (4日)
- `NostrRelaySession` クラスを実装し、状態遷移 (`Disconnected`→`Connecting`→`Connected`/`Faulted`) を `std::atomic` と `CComAutoCriticalSection` で管理。
- NIP-11 メタデータ取得後に `RelayDescriptor.Metadata` を更新し、Supported NIPs を SAFEARRAY で公開。
- セッションを `NostrClient` 内のマップで管理し、再接続 (`Reconnect`) と `Close` 操作を .NET 実装に合わせて制御。
- 成果物: `src/client/NostrRelaySession.cpp`, `tests/native/NostrRelaySessionTests.cpp` (WinHTTP をモックした単体テスト)。

### フェーズ5: サブスクリプション (`INostrSubscription`) (5日)
- サブスクリプション ID 生成器 (64 桁 hex) を実装し、REQ メッセージ構築ロジックを C++ へ移植。
- 受信スレッドからの EVENT/EOSE/CLOSED を `ComCallbackDispatcher` 相当のキューにポストし、COM コールバックを STA スレッド上で直列化。
- `SubscriptionOptions` (KeepAlive, AutoRequeryWindowSeconds, MaxQueueLength) を実装し、QueueOverflowStrategy の挙動を .NET 版に揃える。
- 成果物: `src/client/NostrSubscription.cpp`, `tests/native/NostrSubscriptionStateTests.cpp`。

### フェーズ6: PublishEvent / LastOkResult / サインフロー (4日)
- EVENT メッセージ構築と `INostrSigner` 呼び出しラッパーを実装。署名が空の場合は COM 経由で補完し、Schnorr 署名検証用の `libsecp256k1` を用いて `Id` 計算を行う。
- `LastOkResult` の更新、`OK.success=false` や `NOTICE` の COM コールバック伝播を実装。
- タイムアウト・再試行 (`PublishRetryAttempts = 3`) を C++ 側で再現し、HRESULT マッピング (`HResults::WebSocketFailure` など) を統一。
- 成果物: `src/client/NostrEventPublisher.cpp`, `tests/native/NostrPublishEventTests.cpp`。

### フェーズ7: AUTH/NIP-42 フロー (3日)
- `AuthChallenge` DTO を実装し、`auth-required` 通知を受け取った際のチャレンジキャッシュを保持。
- `RespondAuth` で `INostrSigner` を利用し、kind:22242 EVENT を組み立てて送信。OK/NOTICE 応答に応じて `OnAuthSucceeded`/`OnAuthFailed` を呼び分ける。
- 成果物: `src/client/NostrAuthManager.cpp`, `tests/native/NostrAuthFlowTests.cpp`。

### フェーズ8: 信頼性・リソース管理 (3日)
- 再接続時の指数バックオフ (`BackoffPolicy`) を C++ で実装し、サブスクリプション再送 (`AutoRequeryWindowSeconds`) を統合。
- Dispose/Shutdown 処理を `NostrClient::Dispose` に相当する `FinalRelease` で実装し、WebSocket/HTTP のクリーンアップを保証。
- メモリリーク検出用に `VLD` などのツールと `tests/native/leak` を組み合わせて検証。
- 成果物: `src/client/BackoffPolicy.cpp`, `tests/native/ResourceCleanupTests.cpp`。

### フェーズ9: COM 公開 & レジストリ統合 (2日)
- `NostrClient` ATL クラス (CoClass) を実装し、`ClassInterfaceType::None` 相当の `IDispatchImpl` ベースでメソッドを公開。
- `DllRegisterServer` 時に TypeLib (TLB) を登録し、`COM_Nostr_NativePS` の Proxy/Stub と一緒に regsvr32 が通ることを確認。
- MSI インストーラから Native DLL を登録する手順を README.md に追記し、.NET 版との差異を明記。
- 成果物: `src/com/NostrClient.cpp`, 更新済み README.md。

### フェーズ10: 統合・回帰テストとリリース準備 (5日)
- Win32 自動テスト (GoogleTest/CppUnitTest) と C# MSTest (COM 経由) を CI で実行するよう `azure-pipelines.yml` もしくは GitHub Actions を整備。
- strfry をテストごとに起動する MSTest プロジェクト `UnitTest_COM_Nostr_Native` を新設し、既存 `UnitTest_COM_Nostr` のテストケースを共有ライブラリ化して再利用。
- `dotnet test` で C# 経由の回帰、`ctest` または `ctest --output-on-failure` で C++ 単体テストを実行。
- 署名済みバイナリと TypeLib を含むリリース手順書を更新し、`CHANGELOG.md` に Native 移植フェーズのエントリを追加。

## ユニットテスト / 統合テスト計画
- **C++ 単体テスト**: `tests/native` に GoogleTest を導入し、JSON 変換、バックオフ、署名検証、WebSocket ラッパーのモックテストを実装する。`ctest` 実行前に `cmake --build` で静的ライブラリとテストバイナリを生成する。
- **MSTest (COM 経由統合)**: 既存の `UnitTest_COM_Nostr` をベースに共通ヘルパー (`StrfryRelayHost`, `SubscriptionAssertions`) を共有ライブラリ化し、Native 版向けに `UnitTest_COM_Nostr_Native` プロジェクトを作成。テストごとに `docker run --rm -it -p <動的ポート>:7777 -v "${hostPath}:/app/strfry-db" --name strfry_<test>` でリレーを起動し、テスト完了時に必ず `docker stop` で後始末する。
- **シナリオカバレッジ**:
  - Initialize/Dispose の境界テスト (二重初期化、Dispose 後の呼び出しで `E_NOSTR_OBJECT_DISPOSED` を返すか)。
  - NIP-11 メタデータ取得と JSON パース。
  - REQ 発行→EOSE→CLOSE のライフサイクル。
  - EVENT 送信の OK/NOTICE/timeout 分岐と LastOkResult 更新。
  - AUTH フロー (challenge 受領→RespondAuth→成功/失敗通知)。
  - QueueOverflowStrategy (DropOldest/Throw) の挙動確認。
  - 再接続時のサブスクリプション再送と AutoRequeryWindowSeconds。
- **CI コマンド**:
  - `msbuild COM_Nostr_Native.vcxproj /p:Configuration=Release;Platform=x64`
  - `ctest --build-config Release`
  - `dotnet test UnitTest_COM_Nostr_Native/UnitTest_COM_Nostr_Native.csproj -c Release`

## ドキュメント更新計画
- README.md の「公開 COM インターフェイス」節に Native 実装の制限事項 (依存 DLL, サポート OS, WebSocket API) を追記し、インストール手順を Native 版に合わせて更新する。
- `docs/` 配下に `native_runtime_architecture.md` を追加し、スレッドモデルとリソース管理の概要を整理する。
- `TEXT_FILE_OVERVIEW.md` をフェーズ完了ごとに更新し、新設した C++ ファイルやテストの概要を追記する。
- `TROUBLESHOOTING.md` へ WinHTTP WebSocket の既定タイムアウトや `libsecp256k1` ビルド時の VS toolset 要件など、想定されるハマりどころを記録する。

## マイルストーンと完了条件
- **M1 (フェーズ3完了)**: HTTP + WebSocket が接続・メッセージ送受信でき、NIP-11 情報が取得できる。
- **M2 (フェーズ6完了)**: 単体テストで EVENT/REQ の核心ロジックが成立し、COM コールバックディスパッチャが動作する。
- **M3 (フェーズ10完了)**: `msbuild` + `ctest` + `dotnet test` がローカルで成功し、strfry 統合テストが OK。README と TEXT_FILE_OVERVIEW が更新済みで、`CHANGELOG.md` に Native 版追加が記録されている。

## リスクと対策
- **WinHTTP WebSocket の制約**: フラグメントメッセージや圧縮が未サポートのため、Nostr リレーが permessage-deflate を要求した場合は接続失敗となる。接続エラーを NOTICE に流し、README に制限を明記する。
- **libsecp256k1 ビルド難易度**: Windows でのビルドが失敗した場合は `external/libsecp256k1/build.ps1` にツールセット自動検出と dlopen fallback を追加し、CI で検証。
- **COM コールバックのスレッド境界**: STA スレッドを常駐させる `CallbackDispatcher` が停止した場合にイベントが失われるリスクがあるため、心拍イベントを 30 秒ごとに送信してヘルスチェックする。
- **docker + strfry の起動時間**: テストごとに新しいコンテナを起動するため、タイムアウト (初回 5 秒) を明示し、起動待機ループをテストヘルパーに実装する。

## 参照ファイル
- `COM_Nostr/Contracts/NostrClient.cs` : .NET 版の中心ロジック。メソッド毎の例外処理と HRESULT マッピングをトレースする。
- `COM_Nostr/Internal/*` : JSON シリアライズやバックオフなどの詳細実装。
- `UnitTest_COM_Nostr/*.cs` : Native 版テストケースのテンプレートとして流用。
- `Nostrプロトコルの現行仕様まとめ.docx` : NIP 要件確認。
