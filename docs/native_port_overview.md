# COM_Nostr_Native 移植サマリー (2025-09-23)

## 目的
- .NET 版 `COM_Nostr` で確立済みの COM 契約と振る舞いを C++/ATL 実装へ忠実に移植する。
- README.md と `COM_Nostr/Contracts/*.cs` に定義されたインターフェイス / HRESULT を `COM_Nostr_Native/COMNostrNative.idl` に同期し、双方で互換性を保つ。
- NIP-01 / 11 / 15 / 20 / 42 / 65 の最小要件を満たすランタイムを WinHTTP + libsecp256k1 を土台に再構築し、strfry リレーを用いた回帰テストを通す。

## COM インターフェイスとエラー契約
| インターフェイス | 主なメソッド / プロパティ | 条件付き HRESULT | 備考 |
| --- | --- | --- | --- |
| `INostrClient` | `Initialize`, `ConnectRelay`, `OpenSubscription`, `PublishEvent`, `RespondAuth`, `RefreshRelayInfo`, `ListRelays` | `E_NOSTR_NOT_INITIALIZED`, `E_NOSTR_ALREADY_INITIALIZED`, `E_NOSTR_SIGNER_MISSING`, `E_NOSTR_WEBSOCKET_ERROR`, `HRESULT_FROM_WIN32(ERROR_TIMEOUT)` | `Initialize` 時に `SynchronizationContext` を捕捉し、コールバックを直列化する必要あり。 |
| `INostrRelaySession` | `Url`, `State`, `SupportedNips`, `LastOkResult`, `Reconnect`, `Close`, `UpdatePolicy` | `E_NOSTR_RELAY_NOT_CONNECTED` | `RefreshRelayInfo` → `NIP-11` 取得で `RelayDescriptor.Metadata` を更新。 |
| `INostrSubscription` | `Id`, `Status`, `Filters`, `UpdateFilters`, `Close` | `E_INVALIDARG` (フィルタ異常), `E_NOSTR_WEBSOCKET_ERROR` | `SubscriptionOptions` に基づく `KeepAlive`/`AutoRequeryWindowSeconds` 制御を維持。 |
| `INostrAuthCallback` / `INostrEventCallback` | `OnAuthRequired`, `OnAuthSucceeded`, `OnAuthFailed`, `OnEvent`, `OnEndOfStoredEvents`, `OnNotice`, `OnClosed` | - | COM コールバックは `ComCallbackDispatcher` 相当で直列化し STA 境界を守る。 |
| `INostrSigner` | `Sign`, `GetPublicKey` | `E_POINTER`, `E_NOSTR_SIGNER_MISSING` | Schnorr 署名 (BIP-340) を外部 COM へ委譲。 |

`HResults.cs` に定義されたカスタム値 (`0x88990001`〜`0x88990005`) と .NET 既定の HRESULT 変換 (`ArgumentException` → `E_INVALIDARG` 等) を Native 実装でも同一テーブルで提供すること。
## .NET 実装レイヤー整理
| レイヤー | .NET コンポーネント | 主な責務 | 主な依存 | Native 対応方針 |
| --- | --- | --- | --- | --- |
| エントリーポイント | `NostrClient` | Initialize/Dispose、リレー登録、サブスクリプション管理、署名者委譲、Publish/AUTH フロー | `NostrClientResources`, `ComCallbackDispatcher`, `NostrRelaySession`, `NostrFilterConverter`, `HResults` | `src/client/NostrClient.cpp` (ATL `CComObjectRootEx`) として実装し、クリティカルセクションで状態を保護。 |
| リレーセッション | `NostrRelaySession` | WinHTTP WebSocket 接続、NIP-11 取得、再接続、LastOkResult 更新、AuthChallenge キャッシュ | `IWebSocketConnection`, `NostrHttpClient`, `NostrJsonSerializer`, `BackoffPolicy`, `ComCallbackDispatcher` | `src/client/NostrRelaySession.cpp` で RAII ラッパー (`CHandle`, `CComPtr`) を用意。WinHTTP ハンドルと Threadpool Work Items で受信ループを構築。 |
| サブスクリプション | `NostrSubscription` | REQ 送信、Filter 更新、Queue overflow、EOSE/CLOSED 管理 | `ComCallbackDispatcher`, `NostrRelaySession`, `NostrJsonSerializer` | `src/client/NostrSubscription.cpp` にステートマシンを移植し、`AutoRequeryWindowSeconds` を C++ 側で再計算。 |
| DTO / シリアライザ | `NostrEvent` ほか DTO, `NostrJsonSerializer`, `NostrFilterConverter`, `NostrProtocolModels` | COM DTO ⇔ 内部 DTO ⇔ JSON の正規化と検証 | `nlohmann::json`, `RelayUriUtilities` | `include/dto/*.h` と `src/serialization/NostrJsonSerializer.cpp` を新設。VARIANT/SAFEARRAY 変換は共通ヘルパーに集約。 |
| ランタイム資源 | `NostrClientResources`, `ClientRuntimeOptions`, `IWebSocketConnection` ファミリ | HttpClient/WebSocket ファクトリ、タイムアウト、ユーザーエージェントの保持 | `System.Net.Http`, `ClientWebSocket` | `include/runtime/ClientRuntimeOptions.h` などで値オブジェクト化し、WinHTTP + カスタム WebSocket 実装 (`WinHttpWebSocket`) に置換。 |
| 補助ロジック | `BackoffPolicy`, `RelayUriUtilities`, `HResults`, `ComCallbackDispatcher` | 再接続ポリシー、URL 正規化、HRESULT マッピング、STA ディスパッチ | `System.Uri`, `SynchronizationContext` | `src/util/BackoffPolicy.cpp`, `src/util/RelayUriUtilities.cpp`, `include/NostrHResults.h`, `src/runtime/ComCallbackDispatcher.cpp` (STA ワーカースレッド) を実装。 |

内部処理で `ArrayPool<byte>`、`CancellationToken`、`SynchronizationContext` といった .NET 固有要素を使用している箇所は、C++ 側では `ATL::CComAutoCriticalSection`、`HANDLE` ベースのイベント、`Threadpool` API や `concurrency::concurrent_queue` を用いて代替する。
## DTO / JSON 契約の要点
- EVENT (`NostrEvent`): `id`, `pubkey`, `created_at`, `kind`, `tags`, `content`, `sig` を保持。`tags` は `SAFEARRAY` のジャグ配列 (タグラベル + 値群)。署名の無い入力は `PublishEvent` 内でドラフト作成→署名→`Id` 再計算を行う。
- FILTER (`NostrFilter`): `ids`, `authors`, `kinds`, `tags`, `since`, `until`, `limit`。`since`/`until`/`limit` は `VARIANT` → 正規化 (`double`/`int` 変換) を経て 0 以下の場合 null とする。
- RELAY メタ (`RelayDescriptor`): `Url` + 読み書き可否。`Metadata` には NIP-11 の JSON オブジェクトを格納し、Native 版では `IStream` 相当の JSON 文字列を `BSTR` で保持予定。
- AUTH (`AuthChallenge` + kind:22242): `challenge`/`relay` タグ必須。チャレンジの有効期限 (`expires_at`) は double (UNIX 秒) で保持。
- OK 結果 (`NostrOkResult`): `success`, `eventId`, `message`。`PublishEvent`/`RespondAuth` 後に `NostrRelaySession.LastOkResult` を更新。

### JSON サンプル
```
// EVENT publish payload
["EVENT", {
  "id": "9cf...8e",
  "pubkey": "8d4...98",
  "created_at": 1690000000,
  "kind": 1,
  "tags": [],
  "content": "Hello, Nostr!",
  "sig": "c1f...abc"
}]

// REQ (購読)
["REQ", "sub-01f8cfe6b8ec4c07", {
  "authors": ["8d4...98"],
  "kinds": [1],
  "since": 1690000000
}]

// AUTH 応答 (NIP-42)
["AUTH", {
  "id": "a1b...", "pubkey": "8d4...98", "created_at": 1690001234,
  "kind": 22242,
  "tags": [["challenge", "c016..."], ["relay", "wss://relay.example.com"]],
  "content": "", "sig": "5be..."
}]
```
## 対応が必要な NIP (現行仕様まとめより)
- **NIP-01**: EVENT/REQ/CLOSE/NOTICE フロー、購読 ID (最大 64 文字) の管理、フィルタ構文 (ids/authors/kinds/#tags/since/until/limit)。
- **NIP-11**: `https://<relay>/.well-known/nostr.json` or `GET <relay>` のメタ JSON を取得し、`supported_nips`, `limitation`, `software`, `pubkey` 等を `RelayDescriptor.Metadata` にキャッシュ。
- **NIP-15**: `EOSE` メッセージで保存済みイベントの送信完了を通知。`NostrSubscription` が `Pending→Active` に遷移する基準となる。
- **NIP-20**: `OK [eventId, success, message]` の解析と `LastOkResult` 更新、`success=false` の NOTICE 化。
- **NIP-42**: `AUTH` チャレンジ処理、`kind:22242` イベント署名、`auth-required`/`restricted` プレフィックスの NOTICE/OK/CLOSED に応じたコールバック。
- **NIP-65**: `RelayDescriptor.Metadata` から推奨リレーリスト (kind:10002) を抽出する余地を残し、Preferred リレーの優先制御を維持。

Native 実装では WinHTTP WebSocket の制約 (圧縮未サポート等) を README.md の制限事項に明記し、接続失敗を `HResults.WebSocketFailure` で通知する。
## Native 実装モジュール案
- `include/NostrHResults.h` : `HResults` の値と `inline HRESULT MakeComError(...)` を提供。
- `include/ComValueHelpers.h` : BSTR/UTF-8、SAFEARRAY、VARIANT の相互変換ユーティリティ。
- `include/dto/*.h` : DTO (`NostrEventDto`, `RelayDescriptorDto`, `SubscriptionOptionsDto` など) を `ATL::CComCoClass` ベースで定義。
- `src/runtime/ClientRuntimeOptions.cpp` : .NET 版 `ClientRuntimeOptions` の移植。タイムアウト正規化とユーザーエージェント保持を実装。
- `src/runtime/NativeHttpClient.cpp` : WinHTTP を利用した NIP-11 取得と JSON パース。
- `src/runtime/WinHttpWebSocket.cpp` : WinHTTP WebSocket ラッパー。送受信はバックグラウンド Threadpool ワーカーで非同期処理。
- `src/client/NostrRelaySession.cpp` : セッション状態管理、再接続、AUTH チャレンジ保持、LastOkResult 更新。
- `src/client/NostrSubscription.cpp` : REQ/CLOSE フロー、キューと溢れ制御、AutoRequery。
- `src/client/NostrEventPublisher.cpp` : EVENT/AUTH の署名・送信ロジック、`PublishRetryAttempts` 処理。
- `src/client/CallbackDispatcher.cpp` : STA スレッド or SynchronizationContext 代替を提供。
- `tests/native/*` : GoogleTest ベースのユニットテスト。JSON シリアライザ、Backoff、WebSocket ハンドシェイク (モック) などをカバー。

### 依存ライブラリ・ビルドフロー
1. `external/libsecp256k1` をサブモジュール化し、`build/native-deps.ps1` で x64 Debug/Release 静的ライブラリを生成 (module=schnorrsig 有効)。
2. `packages/native/nlohmann_json` (ヘッダオンリー) を取得し、`pch.h` で include。
3. `COM_Nostr_Native.vcxproj` を C++20 / Unicode 固定 (`/utf-8`) / Treat Warnings As Errors へ更新。
4. `cmake` (テスト) + `msbuild` (COM DLL) が通るパイプラインを README の CI 手順に反映。
## リスクと未決事項
- `.NET` 版ユニットテストの一部が未整備なため、C++ 向け JSON サンプルデータは README の仕様と `doc_tmp/Nostr_spec_extracted.txt` を基に整理した。実装段階で既存 MSTest を再確認し、欠損があれば C++ 版テストに取り込む必要がある。
- WinHTTP WebSocket では permessage-deflate が利用できないため、圧縮要求を出すリレーでは接続エラーとなる。Native 版 README 更新時に制限事項を記載し、`NostrRelaySession` で NOTICE へ通知する。
- `ComCallbackDispatcher` の STA スレッド版を C++ で再現する際、Windows Threadpool で STA は直接利用できない。`CreateThread` + `CoInitializeEx(APARTMENTTHREADED)` で埋め込みメッセージポンプを実装する案を採用。
- docker + strfry をテストごとに起動する MSTest を Native 版へ移植する際、テスト実行時間増加が予想されるため、CI のタイムアウト値を事前調整する。