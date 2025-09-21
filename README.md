# COM_Nostr

## コンポーネント概要
- Windows クライアントから Nostr リレーと対話する COM オートメーション対応コンポーネント。
- イベント送信・受信、フィルタ指定、認証 (NIP-42)、複数リレー接続管理を最小構成で提供する。
- 参照仕様: 「Nostrプロトコルの現行仕様まとめ.docx」に整理された NIP-01 / NIP-15 / NIP-20 / NIP-42 / NIP-65 / NIP-11 の要点。

## 公開 COM インターフェイス一覧
| インターフェイス | 役割 |
| --- | --- |
| `INostrClient` | コンポーネント全体のエントリーポイント。署名者やリレー設定を束ねる。 |
| `INostrRelaySession` | 単一リレーとの接続状態とメタ情報 (NIP-11) を扱う。 |
| `INostrSubscription` | REQ/EOSE/CLOSE のライフサイクルを表現する購読ハンドル。 |
| `INostrEventCallback` | EVENT/EOSE/NOTICE/CLOSED を受け取るクライアント実装向けコールバック。 |
| `INostrAuthCallback` | `auth-required` 応答や AUTH チャレンジを通知する。 |
| `INostrSigner` | Schnorr 署名 (secp256k1, BIP-340) を委譲するためのインターフェイス。 |

## データモデル
### NostrEvent (EVENT メッセージ用ペイロード)
| プロパティ | 型 | 説明 |
| --- | --- | --- |
| `Id` | `BSTR` | `[0, pubkey, created_at, kind, tags, content]` をシリアライズし SHA-256 した値。 |
| `PublicKey` | `BSTR` | 32 バイトの公開鍵 (hex)。 |
| `CreatedAt` | `DOUBLE` | UNIX タイムスタンプ (秒)。 |
| `Kind` | `LONG` | イベント種別。kind=1 は通常テキストノート。 |
| `Tags` | `SAFEARRAY(BSTR[])` | NIP-01/NIP-12 で定義される二次元タグ配列。 |
| `Content` | `BSTR` | 任意テキスト。 |
| `Signature` | `BSTR` | Schnorr 署名 (64 バイト hex)。 |

### NostrFilter (REQ フィルタ)
| フィールド | 型 | 説明 |
| --- | --- | --- |
| `Ids` | `SAFEARRAY(BSTR)` | 既知のイベント ID 群。 |
| `Authors` | `SAFEARRAY(BSTR)` | 公開鍵 (hex)。 |
| `Kinds` | `SAFEARRAY(LONG)` | 取得対象 kind。 |
| `Tags` | `SAFEARRAY(NostrTagQuery)` | `#e` `#p` などタグ検索。 |
| `Since` | `DOUBLE?` | 指定秒以降のイベント。 |
| `Until` | `DOUBLE?` | 指定秒以前のイベント。 |
| `Limit` | `LONG?` | 最大取得件数。 |

### その他 DTO
- `NostrTagQuery` : `Label` (`BSTR`), `Values` (`SAFEARRAY(BSTR)`).
- `RelayDescriptor` : `Url`, `ReadEnabled`, `WriteEnabled`, `Preferred`, `Metadata` (NIP-65)。
- `SubscriptionOptions` : `KeepAlive` (既定 true), `AutoRequeryWindowSeconds` (欠損補完用), `MaxQueueLength`。
- `AuthChallenge` : `RelayUrl`, `Challenge`, `ExpiresAt`。

## インターフェイス詳細
### INostrClient
- `void Initialize([in] ClientOptions options)` : WebSocket 実装やデフォルトタイムアウトを一括設定。
- `void SetSigner([in] INostrSigner* signer)` : Schnorr 署名実装を注入。署名者が未設定で `PublishEvent`/`RespondAuth` を呼ぶと `E_NOSTR_SIGNER_MISSING` を返す。
- `INostrRelaySession* ConnectRelay([in] RelayDescriptor descriptor, [in] INostrAuthCallback* authCallback)` : NIP-11 メタ取得→WebSocket 接続→`connected` 状態遷移。
- `void DisconnectRelay([in] BSTR relayUrl)` : graceful に CLOSE→CLOSED を待機。強制切断時は `State=Faulted`。
- `VARIANT_BOOL HasRelay([in] BSTR relayUrl)` : セッション存在確認。
- `INostrSubscription* OpenSubscription([in] BSTR relayUrl, [in] SAFEARRAY(NostrFilter) filters, [in] INostrEventCallback* callback, [in] SubscriptionOptions options)` : REQ を発行し購読ハンドルを返す。`Id` は 64 文字 hex で自動生成。
- `void PublishEvent([in] BSTR relayUrl, [in] NostrEvent eventPayload)` : EVENT 送信。Signature が空なら `INostrSigner` による署名＋Id 計算を内部実装し OK/NOTICE の応答を待機。
- `void RespondAuth([in] BSTR relayUrl, [in] NostrEvent authEvent)` : kind:22242 イベントを元に AUTH メッセージを送信。
- `void RefreshRelayInfo([in] BSTR relayUrl)` : NIP-11 に従うメタ情報更新をトリガー。
- `SAFEARRAY(BSTR) ListRelays()` : 登録済みリレー URL を列挙。

### INostrRelaySession
- プロパティ: `Url`, `State` (`Disconnected`/`Connecting`/`Connected`/`Faulted`), `LastOkResult`, `SupportedNips` (`SAFEARRAY(LONG)`), `WriteEnabled`, `ReadEnabled`。
- メソッド: `void Reconnect()`, `void Close()`, `RelayDescriptor GetDescriptor()`, `void UpdatePolicy([in] RelayDescriptor descriptor)`。

### INostrSubscription
- プロパティ: `Id`, `Status` (`Pending`/`Active`/`Draining`/`Closed`), `Filters`。
- メソッド: `void UpdateFilters([in] SAFEARRAY(NostrFilter) filters)` → `REQ` を再送。`KeepAlive=false` の場合は `CLOSE` を即送出。
- メソッド: `void Close()` → `CLOSE` を送信し状態を `Closed` に更新。

### INostrEventCallback (クライアント実装)
- `void OnEvent([in] BSTR relayUrl, [in] NostrEvent event)` : EVENT 受信。
- `void OnEndOfStoredEvents([in] BSTR relayUrl, [in] BSTR subscriptionId)` : EOSE 通知。
- `void OnNotice([in] BSTR relayUrl, [in] BSTR message)` : NOTICE が発生した際。
- `void OnClosed([in] BSTR relayUrl, [in] BSTR subscriptionId, [in] BSTR reason)` : `CLOSED` を受信。

### INostrAuthCallback
- `void OnAuthRequired([in] AuthChallenge challenge)` : `auth-required` を受信。
- `void OnAuthSucceeded([in] BSTR relayUrl)` / `void OnAuthFailed([in] BSTR relayUrl, [in] BSTR reason)` : OK 応答の成否を通知。

### INostrSigner
- `BSTR Sign([in] NostrEventDraft draft)` : `Id` 計算前のドラフトから 64 バイト hex 署名を返す。
- `BSTR GetPublicKey()` : 現在の公開鍵を返却 (リレー側の権限判定用)。

## イベント送信フロー
1. `NostrEventDraft` を組み立て (`CreatedAt`, `Kind`, `Tags`, `Content`)。
2. `INostrSigner.Sign` で署名し、同時に `Id` を算出。
3. `PublishEvent` で EVENT 送信。
4. リレーからの `OK` を待機し、`success=false` の場合は `OnNotice` 相当のエラーをコールバックへフォワード。

## イベント受信 / フィルタリング
1. `OpenSubscription` で REQ を発行。`SubscriptionOptions.AutoRequeryWindowSeconds` が正の場合、`since`/`until` を自動調整し欠損を補う。
2. リレーからの `EVENT` は `OnEvent` で逐次通知。
3. 蓄積分の送信完了時に `EOSE` を検知して `Status=Active` へ遷移。
4. サブスクリプション終了は `Close()` または `CLOSE`/`CLOSED` で管理。

## 認証 (NIP-42)
- リレーが `auth-required` で購読/投稿を拒否した場合、`INostrAuthCallback.OnAuthRequired` に `AuthChallenge` を引き渡す。
- クライアントは challenge と relay URL を `NostrEvent` (kind:22242) に変換し `RespondAuth` を呼ぶ。
- 成功時は `OK` メッセージから結果を解析し `OnAuthSucceeded` を通知。失敗時は `OnAuthFailed`。

## リレー接続管理
- `ConnectRelay` は NIP-11 のメタ情報 (`supported_nips`, `limitation`, `auth` など) をキャッシュし、`RelayDescriptor.Metadata` として公開。
- `RelayDescriptor.Preferred` に基づき、書き込み優先リレーと読み出し専用リレーを別々に管理。
- NIP-65 のリレーリスト (kind:10002) を `Tags` から解析し `RelayCatalog` (将来拡張) に反映する余地を残す。
- ネットワーク障害検出時は自動でバックオフし、`Reconnect()` で手動復旧を提供。

## エラーと状態通知
- EVENT 応答 `OK` の `success=false` 時は `OnNotice` へ理由を転送し、同時に `INostrRelaySession.LastOkResult` を更新。
- リレーからの `NOTICE` はログとコールバックの二重化で保持。
- サブスクリプションごとの `CLOSED` には `reason` を付与し、NIP-42 の `auth-required` 等をアプリ側で再認証できるよう保持。

## 実装メモ
- WebSocket は WinHTTP または MsQuic ベースを想定。TLS 証明書検証は OS 設定に委譲。
- 署名 (secp256k1 Schnorr) は外部ライブラリへ委譲し、COM 境界ではプレーンな hex 文字列を受け渡す。
- タイムスタンプは `double` で扱い、丸めによるハッシュ不一致を避けるため整数秒へ切り捨ててから署名計算を行う。
- マルチスレッド COM を想定し、購読通知は `IConnectionPoint` 経由でマルシャリング。
