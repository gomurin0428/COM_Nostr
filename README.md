# COM_Nostr

## コンポーネント概要
- Windows クライアントから Nostr リレーと対話する COM オートメーション対応コンポーネント。
- イベント送信・受信、フィルタ指定、認証 (NIP-42)、複数リレー接続管理を最小構成で提供する。
- 参照仕様: 「Nostrプロトコルの現行仕様まとめ.docx」に整理された NIP-01 / NIP-15 / NIP-20 / NIP-42 / NIP-65 / NIP-11 の要点。
- 詳細設計メモは `docs/phase0_design.md` にまとめている。

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
- `ClientOptions` : `WebSocketFactoryProgId` (BSTR; ProgID が指す型は `COM_Nostr.Internal.IWebSocketFactory` を実装するか `ClientWebSocket` 派生インスタンスを返す必要がある), `UserAgent` (BSTR; 未指定時は `COM_Nostr/1.0` を適用), `ConnectTimeoutSeconds` / `SendTimeoutSeconds` / `ReceiveTimeoutSeconds` (VARIANT; 正の秒数。数値/文字列いずれも可)。
- `NostrOkResult` : `Success` (VARIANT_BOOL), `EventId` (BSTR), `Message` (BSTR)。

## インターフェイス詳細
### INostrClient
- `void Initialize([in] ClientOptions options)` : WebSocket 実装やデフォルトタイムアウトを一括設定。呼び出しスレッドの `SynchronizationContext` を捕捉し、`ComCallbackDispatcher` に保存する。同期コンテキストが無い場合は内部 STA ワーカースレッドを立ち上げ、以後すべての COM コールバックをそこへ直列ディスパッチする。同じインスタンスで二度以上呼び出すと `E_NOSTR_ALREADY_INITIALIZED` を返す。
- `void SetSigner([in] INostrSigner* signer)` : Schnorr 署名実装を注入。署名者が未設定で `PublishEvent`/`RespondAuth` を呼ぶと `E_NOSTR_SIGNER_MISSING` を返す。
- `INostrRelaySession* ConnectRelay([in] RelayDescriptor descriptor, [in] INostrAuthCallback* authCallback)` : NIP-11 メタ取得→WebSocket 接続→`Connected` 状態遷移。`authCallback` は `ComCallbackDispatcher` 経由で発火。
- `void DisconnectRelay([in] BSTR relayUrl)` : graceful に CLOSE→CLOSED を待機。強制切断時は `State=Faulted`。
- `VARIANT_BOOL HasRelay([in] BSTR relayUrl)` : セッション存在確認。
- `INostrSubscription* OpenSubscription([in] BSTR relayUrl, [in] SAFEARRAY(NostrFilter) filters, [in] INostrEventCallback* callback, [in] SubscriptionOptions options)` : REQ を発行し購読ハンドルを返す。`Id` は 64 文字 hex で自動生成し、コールバックは `ComCallbackDispatcher` を通じて直列ディスパッチされる。同期コンテキストを捕捉できていればそのコンテキスト上で、捕捉できない場合は内部 STA ワーカー上で実行する。`SubscriptionOptions.KeepAlive` が false のときは初回 `EOSE` 後に `CLOSE` を自動送信し、`AutoRequeryWindowSeconds` が正の値なら `UpdateFilters` 時に `since` を補正する。`MaxQueueLength` を設定するとキュー上限に達した際の挙動を `QueueOverflowStrategy` で制御でき、既定 (`DropOldest`) では最古のイベントを破棄し、`Throw` を選ぶと購読を `CLOSED` に遷移させる。
- `void PublishEvent([in] BSTR relayUrl, [in] NostrEvent eventPayload)` : EVENT 送信。Signature が空なら `INostrSigner` による署名＋Id 計算を内部実装し、リレーの `OK` 応答を `ClientOptions.ReceiveTimeout` (未指定時は 10 秒) まで待機して `LastOkResult` に反映。`OK.success=false` や `NOTICE` は `INostrEventCallback.OnNotice` に転送し、timeout は `HRESULT_FROM_WIN32(ERROR_TIMEOUT)`、拒否は `E_NOSTR_WEBSOCKET_ERROR` を返す。
| `E_NOSTR_OBJECT_DISPOSED` | `0x88990004` | `Dispose` 呼び出し後に API を利用した場合。 |
| `E_NOSTR_ALREADY_INITIALIZED` | `0x88990005` | 同一インスタンスで `Initialize` を二度以上実行した場合。 |
| `E_CLASSNOTREGISTERED` | `0x80040154` | `ClientOptions.WebSocketFactoryProgId` が未登録の ProgID を指している場合。 |
- `void RespondAuth([in] BSTR relayUrl, [in] NostrEvent authEvent)` : kind:22242 イベントを元に AUTH メッセージを送信。
- `void RefreshRelayInfo([in] BSTR relayUrl)` : NIP-11 に従うメタ情報更新をトリガー。
- `SAFEARRAY(BSTR) ListRelays()` : 登録済みリレー URL を列挙。
- ※ .NET クライアントでは `Dispose()` を呼び出して全リレーセッションと内部リソースを解放できる。Dispose 後に API を利用すると `E_NOSTR_OBJECT_DISPOSED` が返る。

### INostrRelaySession
- プロパティ: `Url`, `State` (`Disconnected`/`Connecting`/`Connected`/`Faulted`), `LastOkResult` (NostrOkResult), `SupportedNips` (`SAFEARRAY(LONG)`), `WriteEnabled`, `ReadEnabled`。
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
#### 既定実装 (COM_Nostr.NostrSigner)
- ProgID: `COM_Nostr.NostrSigner`。`INostrSigner` を実装する COM クラスです。
- 既定コンストラクタは環境変数 `NOSTR_PRIVATE_KEY` (64 桁 hex) の秘密鍵を読み込みます。未設定の場合は `InvalidOperationException` を送出します。
- `Sign` は `draft.PublicKey` をサイン済み公開鍵で上書きし、異なる鍵が指定されている場合は例外を返します。
- 署名対象は `[0, pubkey, created_at, kind, tags, content]` を UTF-8 JSON でシリアライズした 32 バイトハッシュ (NIP-01) です。
- 戻り値は 64 バイト (128 桁) の BIP-340 Schnorr 署名 (hex)。返却値を EVENT の `sig` に設定し、同じ計算で得られる Id を EVENT の `id` に設定してください。

## 設計方針 (フェーズ0)
- **WebSocket**: 既定は `ClientWebSocket` をラップした `ClientWebSocketConnection` を使用。`ClientOptions.WebSocketFactoryProgId` が指定された場合のみ外部 COM ファクトリを生成し、失敗時にフォールバックしない。
- **接続制御**: `RelaySession` が `Disconnected → Connecting → Connected/Faulted` を管理し、NIP-11 応答を `RelayDescriptor.Metadata`/`SupportedNips` にキャッシュする。
- **サブスクリプション**: `Subscription` が REQ/EOSE/CLOSED のステートマシンを実装。購読 ID は 32 バイト乱数の hex 表現。`AutoRequeryWindowSeconds` で `since` を自動調整。
- **COM コールバック**: `ComCallbackDispatcher` が `SynchronizationContext.Post` もしくは内部 STA ワーカースレッドを利用し、コールバック実行順序を直列化する。
- **署名と AUTH**: `PublishEvent` と `RespondAuth` では `INostrSigner` を必須とし、未設定時は `E_NOSTR_SIGNER_MISSING` を返す。
- **HTTP/NIP-11**: `NostrHttpClient` がカスタム `UserAgent` とタイムアウトを反映し、NIP-11 の JSON を `RelayDescriptor.Metadata` に格納する。
- **ログ**: 内部 `INostrLogger` を導入し、重要イベントと例外を記録する。外部公開は行わない。

### WebSocket ファクトリ差し替え
- `ClientOptions.WebSocketFactoryProgId` で指定する ProgID は、COM 生成した型が `COM_Nostr.Internal.IWebSocketFactory` を実装するか `System.Net.WebSockets.ClientWebSocket` 派生インスタンスを返す必要があります。
- `IWebSocketFactory.Create()` は `IWebSocketConnection` もしくは `ClientWebSocket` を返却し、`NostrClient` が `User-Agent` 設定と接続処理を引き継ぎます。
- ファクトリが `null` やサポート対象外の型を返した場合は `COMException (E_FAIL)` を送出し、既定実装へはフォールバックしません。

## 例外と HRESULT
| 定数 | 値 | シナリオ |
| --- | --- | --- |
| `E_NOSTR_SIGNER_MISSING` | `0x88990001` | 署名者未設定で EVENT/AUTH を実行した場合。
| `E_NOSTR_RELAY_NOT_CONNECTED` | `0x88990002` | 未接続リレーに操作した場合。
| `E_NOSTR_NOT_INITIALIZED` | `0x88990003` | `Initialize` を呼び出す前に API を使用した場合。
| `E_NOSTR_WEBSOCKET_ERROR` | `0x80200010` | NIP-11 取得や WebSocket 接続時のネットワーク障害。
| `E_INVALIDARG` | `0x80070057` | パラメータ検証エラー (URL/フィルタ等)。
| `HRESULT_FROM_WIN32(ERROR_TIMEOUT)` | `0x800705B4` | EVENT 応答待ちタイムアウト。
| `COR_E_FORMAT` | `0x80131537` | JSON シリアライズ/パース失敗。

- すべての公開メソッドで例外を捕捉し、`HResults` ヘルパーを介して対応する HRESULT を設定した `COMException` を送出する。

## イベント送信フロー
1. `NostrEventDraft` を組み立て (`CreatedAt`, `Kind`, `Tags`, `Content`)。
2. `INostrSigner.Sign` で署名し、同時に `Id` を算出。
3. `PublishEvent` で EVENT 送信。
4. リレーからの `OK` を待機し、`success=false` の場合は `OnNotice` にフォワードしつつ `LastOkResult` を更新。

## イベント受信 / フィルタリング
1. `OpenSubscription` で REQ を発行。`SubscriptionOptions.AutoRequeryWindowSeconds` が正の場合、`since`/`until` を自動調整し欠損を補う。
2. リレーからの `EVENT` は `OnEvent` で逐次通知。
3. 蓄積分の送信完了時に `EOSE` を検知して `Status=Active` へ遷移。
4. サブスクリプション終了は `Close()` または `CLOSE`/`CLOSED` で管理。

### INostrSubscription
- `Id` は購読開始時に生成される 64 文字の hex。COM 呼び出し側から読み取り可能。
- `Status` は `Pending` (REQ 送信直後) → `Active` (EOSE 受信後) → `Draining` (`Close` 実行中) → `Closed` へ遷移する。
- `UpdateFilters` はフィルタ配列を正規化して再度 REQ を送信する。`AutoRequeryWindowSeconds` が正の場合、直近イベント時刻から逆算した `since` を補正して取りこぼしを抑制する。
- `Close()` は `CLOSE` メッセージを送信し、リレーの `CLOSED` 応答で `Closed` 状態へ遷移する。
- イベントは内部キューで順序を保持してから `INostrEventCallback` に通知され、`MaxQueueLength` を超えた場合は最古のイベントから破棄する。

## 認証 (NIP-42)
- リレーからの `AUTH` メッセージを自動で解析し、最後に受け取った challenge をキャッシュした上で `INostrAuthCallback.OnAuthRequired` に `AuthChallenge` (UNIX 秒での `ExpiresAt` 含む) を通知する。
- `auth-required`/`auth-failed`/`restricted` などのプレフィックスを持つ `OK`/`NOTICE`/`CLOSED` を受信すると、状況に応じて `OnAuthRequired` や `OnAuthFailed` をフォローアップで呼び出す。
- `RespondAuth` は保存済み challenge とリレー URL をもとに `relay` / `challenge` タグを補完し、未署名の場合は `INostrSigner` を用いて kind:22242 EVENT を署名して送信する。challenge が未設定でタグも空の場合は `COMException(E_INVALIDARG)` をスローする。
- `AUTH` 応答の `OK` が success=true で返ると `OnAuthSucceeded` を通知し、再利用されないよう challenge キャッシュをクリアする。失敗時はメッセージを抽出して `OnAuthFailed` に連携する。

## リレー接続管理
- `ConnectRelay` は NIP-11 のメタ情報 (`supported_nips`, `limitation`, `auth` など) をキャッシュし、`RelayDescriptor.Metadata` として公開。
- `RelayDescriptor.Preferred` に基づき、書き込み優先リレーと読み出し専用リレーを別々に管理。
- NIP-65 のリレーリスト (kind:10002) を `Tags` から解析し `RelayCatalog` (将来拡張) に反映する余地を残す。
- ネットワーク障害検出時は受信タイムアウトやサーバーからの CLOSE を契機に自動バックオフで再接続し、成功後は既存サブスクリプションに対して REQ を再送して欠損を補う (`AutoRequeryWindowSeconds` を反映)。手動 `Reconnect()` も引き続き提供。

## 実装メモ
- WebSocket は `ClientWebSocket` をラップし、送受信タスクを `CancellationToken` で制御する。外部 ProgID 指定時は生成失敗を HRESULT で返し、既定実装へはフォールバックしない。
- 署名 (secp256k1 Schnorr) は `COM_Nostr.NostrSigner` へ委譲し、COM 境界ではプレーンな hex 文字列を受け渡す。
- タイムスタンプは `double` で扱い、署名計算前に整数秒へ丸めてハッシュ不一致を避ける。
- コールバックは `ComCallbackDispatcher` によるシリアライズ実行でスレッド境界を統一し、`SynchronizationContext` が無い場合は専用 STA スレッドを生成して処理する。
- docker + dockurr/strfry を利用した統合テストを MSTest で自動化し、テストケースごとに独立したリレーを立ち上げる。

## インストールと登録手順

### MSI インストーラの作成 (Setup_COM_Nostr)
#### 前提条件
- Visual Studio 2022 (17.8 以降) と "Microsoft Visual Studio Installer Projects" 拡張機能
- .NET SDK 8.0 以上 (Release ビルド用) と .NET 8 Desktop Runtime (配布先にインストール)
- `dotnet build` 済みの `COM_Nostr\bin\Release\net8.0-windows` フォルダー

#### Setup_COM_Nostr プロジェクト構成手順
1. Visual Studio で `COM_Nostr.sln` を開き、構成を `Release|Any CPU` に合わせます。
2. `COM_Nostr` プロジェクトをビルドし、`COM_Nostr.comhost.dll` を含む最新の成果物を生成します。
3. `Setup_COM_Nostr` プロジェクトを開き、「ファイル システム」エディターの `アプリケーション フォルダー` を選択します。
4. `アプリケーション フォルダー` を右クリックし「プロジェクトの出力の追加」を選択、`Primary output from COM_Nostr (Active)` を追加します。追加後、プロパティ `Register` を `vsdrpDoNotRegister` に変更して `regasm` ベースの登録を無効化します。
5. 同じ `アプリケーション フォルダー` に Release ビルド成果物から次のファイルを追加します。
   - `COM_Nostr.comhost.dll` (プロパティ `Register` = `vsdrfCOMSelfReg`, `Vital` = `True`)
   - `COM_Nostr.dll`
   - `COM_Nostr.deps.json`
   - `COM_Nostr.runtimeconfig.json`
   - `NBitcoin.Secp256k1.dll`
6. 製品情報 (`Manufacturer`, `ProductVersion` など) を必要に応じて更新します。`ProductVersion` を変更した場合は Visual Studio が新しい `ProductCode` を生成するよう保存時のダイアログに従ってください。

#### MSI のビルドとインストール
1. Visual Studio のビルド構成を `Release` にしたまま、メニューの「ビルド」→「ソリューションのビルド」または `Setup_COM_Nostr` の「ビルド」を実行します。
2. 正常終了すると `Setup_COM_Nostr\Release\Setup_COM_Nostr.msi` が生成されます。配布時は同フォルダー内の CAB/Setup.exe を含め全ファイルをまとめて提供してください。
3. 配布先では管理者権限の PowerShell で `msiexec /i .\Setup_COM_Nostr.msi` を実行します。インストール時に `COM_Nostr.comhost.dll` の `DllRegisterServer` が自動実行され、COM ProgID `COM_Nostr.NostrClient` が登録されます。
4. アンインストールは `msiexec /x {ProductCode}` または Windows の「アプリと機能」から行います。アンインストール時には `DllUnregisterServer` が呼ばれます。

### 手動登録 (開発・デバッグ用)
1. `dotnet build COM_Nostr.sln -c Release` を実行して Release ビルドを生成します。
2. 管理者権限の PowerShell で `regsvr32.exe /s "<リポジトリパス>\COM_Nostr\COM_Nostr\bin\Release\net8.0-windows\COM_Nostr.comhost.dll"` を実行し、COM を登録します。
3. Excel や PowerShell (`New-Object -ComObject COM_Nostr.NostrClient`) で ProgID が作成できるか確認します。
4. 登録解除は `regsvr32.exe /u /s "...\COM_Nostr.comhost.dll"` を実行します。旧来の `regasm` (`COM_Nostr.dll`) は .NET 8 の comhost では利用しない点に注意してください。
## ドキュメント
- 設計メモ: `docs/phase0_design.md`
- 仕様要約: `Nostrプロトコルの現行仕様まとめ.docx`
- テキストファイル一覧: `TEXT_FILE_OVERVIEW.md`


