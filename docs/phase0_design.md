# フェーズ0 詳細設計メモ

## 1. 要件整理
- 公開 COM インターフェイス (`INostrClient`, `INostrRelaySession`, `INostrSubscription`, `INostrEventCallback`, `INostrAuthCallback`, `INostrSigner`) は README.md の仕様とインターフェイス定義 (Contracts/*.cs) に一致させる。
- サポートすべきプロトコル機能: イベント送信/受信 (NIP-01), EOSE (NIP-15), OK/NOTICE (NIP-20), リレー認証 (NIP-42), リレーリスト取得 (NIP-65), メタデータ取得 (NIP-11)。これらの流れは「Nostrプロトコルの現行仕様まとめ.docx」の要約を参照する。
- `LastOkResult` は直近の EVENT/NOTICE 応答を保持し、`RelayDescriptor.Metadata` には NIP-11 応答を格納する。
- 署名者未設定で EVENT/AUTH を送信した場合は `E_NOSTR_SIGNER_MISSING` を返却し、`SetSigner` で注入済みの `INostrSigner` を常に利用する。

## 2. クラス構成
### 2.1 コアコンポーネント
- `NostrClient` : COM からのエントリーポイント。初期化・署名者管理・リレーセッション管理 (`RelayRegistry`) を担う。
- `RelaySession` : 単一リレーとの接続状態、WebSocket 送受信、NIP-11 キャッシュ、`LastOkResult` の更新を扱う。
- `Subscription` : サブスクリプション ID 発行、REQ/EOSE/CLOSED のライフサイクル管理、購読状態 (`SubscriptionStatus`) を担当。
- `NostrProtocolSerializer` : EVENT/REQ/NOTICE/OK/AUTH/EOSE/CLOSED を JSON <-> DTO 変換するヘルパー。
- `NostrHttpClient` : NIP-11 エンドポイント取得用の `HttpClient` ラッパー。User-Agent やタイムアウトは `ClientOptions` を反映。
- `IWebSocketConnection` + `ClientWebSocketConnection` : WebSocket の抽象化と既定実装。接続・送信・受信・閉鎖・キャンセルを安全に扱う。
- `ComCallbackDispatcher` : コールバック実行順序を直列化し、指定のスレッド (後述) にマルシャリングする。
- `INostrLogger` + `NullLogger` : 内部ログ用の軽量インターフェイス。COM 公開はせず、将来的な依存性注入に備える。

### 2.2 データ保持
- `RelayRegistry` : URL 正規化済みキーで `RelaySession` を保持する辞書。並行アクセスを `ReaderWriterLockSlim` で保護。
- `SubscriptionBook` : セッション毎のサブスクリプション集合。購読更新やクリーンアップを一元管理。
- `BackoffPolicy` : 再接続時の指数バックオフ計算を提供する値オブジェクト。

## 3. WebSocket 実装検討
### 3.1 候補比較
- **System.Net.WebSockets.ClientWebSocket**: .NET 標準、TLS サポート完備、CancellationToken による中断が容易。WinHTTP ベースで Windows の証明書ストアを利用可能。
- **外部 COM WebSocket ファクトリ**: `ClientOptions.WebSocketFactoryProgId` で指定された ProgId から生成。互換性確保が目的だが、標準実装がない場合のメンテコストが高い。

### 3.2 採用案
- 既定は `ClientWebSocket` を `ClientWebSocketConnection` でラップする。受信ループ用に `ReceiveAsync` + `CancellationToken` を利用し、切断・タイムアウト時に `RelaySession` へ通知する。
- `ClientOptions.WebSocketFactoryProgId` が指定された場合のみ COM からファクトリを生成し、`IWebSocketConnection` を提供する。生成失敗時は `COMException` (`REGDB_E_CLASSNOTREG`) で呼び出し元に通知し、標準実装へフォールバックしない。
- 送受信バッファは 64 KiB チャンクを既定とし、NOTICE/OK など小型メッセージ向けに `ArrayPool<byte>` を利用して GC 負荷を軽減する。

## 4. COM コールバック マルシャリング
### 4.1 選択肢の比較
- **SynchronizationContext ベース**: `Initialize` 呼び出しスレッドの `SynchronizationContext` へ `Post` する。GUI クライアント (WPF/WinForms/VBA) では既存コンテキストを利用でき、順序保証もしやすい。
- **IConnectionPointContainer**: COM イベント機構を再実装する必要があり、すでにコールバック インターフェイスが明示的に渡される本コンポーネントには過剰。イベント購読解除も複雑化する。

### 4.2 採用案と実装方針
- `NostrClient.Initialize` 時に現在の `SynchronizationContext` を捕捉し、以後の `INostrEventCallback` / `INostrAuthCallback` 呼び出しを `ComCallbackDispatcher` 経由で順次 `Post` する。
- `SynchronizationContext` が `null` の場合は内部で専用 STA スレッドを生成し (`Thread` + `SetApartmentState(ApartmentState.STA)`)、単純なメッセージポンプ (BlockingCollection<Action>) を介して同一スレッド上でコールバックを実行する。これにより COM オブジェクトのスレッド境界を明確化し、呼び出し順を保証する。
- `ComCallbackDispatcher` は停止処理 (`Dispose`) を提供し、リレー切断や `DisconnectRelay` 時に保留コールバックをドレインしてからスレッドを終了する。

## 5. プロトコル処理と状態管理
- `RelaySession` は `State` を `Disconnected -> Connecting -> Connected` の順で遷移させ、エラー発生時に `Faulted` を設定する。状態遷移は内部 `StateMachine` ヘルパーで集中管理。
- 受信ループは JSON メッセージを解析し、種別ごとに `RelayMessageDispatcher` へ振り分ける。`EVENT` は対象 `Subscription` へ転送し、`EOSE` で `Subscription.Status` を `Active` へ更新、`CLOSED` は `Closed` へ変更してコールバックする。
- `OK` メッセージは `RelaySession.LastOkResult` を更新すると同時に、該当購読または公開要求に紐付いている `TaskCompletionSource` を完了させる。
- `OpenSubscription` では 32 バイト乱数を hex に変換して購読 ID を生成し、REQ メッセージと共に `Subscription` オブジェクトを初期化。`SubscriptionOptions.AutoRequeryWindowSeconds` は最後に受信した `CreatedAt` を基準に `since` を再計算する。
- `RespondAuth` では `auth-required` で取得した challenge と公開鍵を kind:22242 EVENT に変換し、`IWebSocketConnection.SendAsync` を通じて送信する。

## 6. 例外・HRESULT ポリシー
| 事象 | 対応 | HRESULT |
| --- | --- | --- |
| 署名者未設定で EVENT/AUTH 呼び出し | `throw new COMException("Signer is not set", E_NOSTR_SIGNER_MISSING);` | `0x88990001` |
| 引数不備 (null/空文字/不正 URL 等) | `ArgumentException` を捕捉し `E_INVALIDARG` にマップ | `0x80070057` |
| リレー未接続 (`ConnectRelay` 未呼び出し) | `COMException` を送出 | `0x88990002` (カスタム) |
| WebSocket 接続エラー | 発生例外を `COMException` (`0x80200010` 相当) として再構成し、`RelaySession.State=Faulted` に設定 | `0x80200010` |
| JSON シリアライズ/パース失敗 | `COMException` (`COR_E_FORMAT`) | `0x80131537` |
| タイムアウト (`PublishEvent` 応答待ち) | `COMException` (`HRESULT_FROM_WIN32(ERROR_TIMEOUT)`) | `0x800705B4` |

- すべての公開メソッドで例外を捕捉し、内部ログへ書き出した後に適切な HRESULT へ変換するヘルパー (`ComErrorMapper`) を用意する。
- カスタム HRESULT (`0x88990001`/`0x88990002`) は `public const int` として `HResults` クラスに定義し、README.md にも記載する。

## 7. ログと診断
- `INostrLogger` (メソッド: `LogDebug`, `LogInfo`, `LogWarn`, `LogError`) を内部向けに定義し、既定は `NullLogger`。必要に応じ `Initialize` で DI 可能なフックを追加予定 (フェーズ1以降)。
- 重要イベント (接続開始/終了、認証要求、OK/NOTICE、サブスクリプション状態変化) をログする。
- 例外発生時は HRESULT と併せてログし、デバッグ容易性を確保する。

## 8. 今後の実装タスクと依存関係
1. `ComCallbackDispatcher` と STA ワーカーの実装 (完了: 2025-09-21)。
2. `IWebSocketConnection` 抽象化と `ClientWebSocketConnection` 実装。
3. `NostrProtocolSerializer` によるメッセージ変換ロジックと単体テスト。
4. `RelaySession` の状態遷移実装、および NIP-11 取得 (`HttpClient` + JSON 解析)。
5. `Subscription` 管理 (`AutoRequeryWindowSeconds`、`KeepAlive` 等) とテスト基盤 (docker strfry) の準備。
6. 例外マッピングと `HResults` 定義、README.md の更新。

---
- 本メモはフェーズ0終了時点の設計方針であり、後続フェーズで詳細設計が変わる場合は本ファイルと README.md を同時に更新する。
