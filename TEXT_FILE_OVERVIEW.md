# Git管理テキストファイル一覧

| ファイル | 概要 |
| --- | --- |
| `.gitattributes` | 改行コードの正規化や特定拡張子のマージ・差分設定方針を定義。 |
| `.gitignore` | Visual Studio/ .NET 開発で不要な生成物を除外するための無視設定。 |
| `Agents.md` | コントリビューター向けの作業ガイドラインとテスト方針を箇条書きで記載。 |
| `README.md` | COM_Nostr コンポーネントの概要、公開 COM インターフェイス、データモデル、Setup_COM_Nostr を用いた MSI 作成手順と手動登録手順をまとめた中心ドキュメント。 |
| `COM_Nostr.sln` | 本体ライブラリとテストプロジェクトを含む Visual Studio ソリューション定義。 |
| `COM_Nostr/COM_Nostr.csproj` | COM 対応の .NET 8 プロジェクト設定と NBitcoin.Secp256k1 依存パッケージに加え、生成済み `COM_Nostr.tlb` をアセンブリへ埋め込む設定。 |
| `COM_Nostr/COM_Nostr.idl` | COM 公開インターフェイスと列挙体、DTO コクラスをまとめた IDL 定義。タイプライブラリ生成時の基準にする。 |
| `COM_Nostr_Native/COMNostrNative.idl` | C++ ATL 版 `COM_Nostr_Native` のタイプライブラリ定義。COM_Nostr と同じインターフェイス/IID/CLSID を再宣言して移植を支える。 |
| `COM_Nostr/Contracts/DataContracts.cs` | COM で公開するイベント、フィルタ、オプション等の DTO クラス群を定義し、`SubscriptionOptions.QueueOverflowStrategy` を追加。 |
| `COM_Nostr/Contracts/Enums.cs` | リレー/サブスクリプション状態に加え、`QueueOverflowStrategy` 列挙体を提供。 |
| `COM_Nostr/Contracts/Interfaces.cs` | INostrClient など COM インターフェイス群のメソッド／プロパティ契約を宣言。 |
| `COM_Nostr/Contracts/NostrClient.cs` | `Initialize`/`RespondAuth`/`Dispose` を含む COM 実装本体。`ComCallbackDispatcher` を介したコールバック直列化、`HResults` を用いた HRESULT マッピング、二重 Initialize ガード、セッション掃除を実装。 |
| `COM_Nostr/Contracts/NostrSigner.cs` | 環境変数の秘密鍵で Schnorr 署名とイベントID計算を行う COM 実装クラス。 |
| `COM_Nostr/Internal/ClientRuntimeOptions.cs` | Initialize で正規化したタイムアウトや User-Agent を保持する内部設定モデル。 |
| `COM_Nostr/Internal/ClientWebSocketConnection.cs` | `System.Net.WebSockets.ClientWebSocket` を `IWebSocketConnection` にラップする実装。 |
| `COM_Nostr/Internal/IWebSocketConnection.cs` | WebSocket 送受信を抽象化する内部インターフェイス。 |
| `COM_Nostr/Internal/IWebSocketFactory.cs` | 外部差し替え用ファクトリが実装すべき公開インターフェイス。 |
| `COM_Nostr/Internal/NostrClientResources.cs` | HttpClient/WebSocket/シリアライザのファクトリを束ねたリソースホルダー。 |
| `COM_Nostr/Internal/NostrJsonSerializer.cs` | EVENT/REQ/OK/NOTICE/EOSE/CLOSED/AUTH の JSON 変換ロジックと AUTH challenge デシリアライズを提供。 |
| `COM_Nostr/Internal/ComCallbackDispatcher.cs` | 捕捉した `SynchronizationContext` または内部 STA ワーカースレッドで COM コールバックを直列実行するディスパッチャ実装。 |
| `COM_Nostr/Internal/NostrFilterConverter.cs` | COM から渡された NostrFilter/NostrTagQuery を内部 DTO に正規化するユーティリティ。 |
| `COM_Nostr/Internal/NostrProtocolModels.cs` | EVENT/REQ/OK/NOTICE/EOSE/CLOSED/AUTH 向け内部 DTO (AuthChallenge など) を定義する補助クラス群。 |
| `COM_Nostr/Internal/WebSocketFactoryResolver.cs` | ProgID 解析とファクトリ生成・検証を担当するユーティリティ。 |
| `COM_Nostr/Internal/BackoffPolicy.cs` | 再接続時の指数バックオフ遅延を計算する内部ユーティリティ。 |
| `COM_Nostr/Internal/HResults.cs` | 公開 API で使用するカスタム HRESULT と `COMException` ヘルパー (`InvalidArgument`/`WebSocketFailure` など) を集約。 |
| `COM_Nostr/Internal/NostrHttpClient.cs` | NIP-11 メタデータ取得とコンテンツ種別チェックを行う HTTP ラッパー。 |
| `COM_Nostr/Internal/NostrRelayInformation.cs` | NIP-11 応答の JSON とサポート NIP 配列を保持する DTO。 |
| `COM_Nostr/Internal/RelayUriUtilities.cs` | リレー URL の正規化と NIP-11 エンドポイント構築を行うヘルパー。 |
| `COM_Nostr/Properties/AssemblyInfo.cs` | アセンブリのメタデータと COM 公開設定、テストプロジェクトへの InternalsVisibleTo を構成。 |
| `LICENSE.txt` | MIT License テンプレート本文。 |
| `UnitTest_COM_Nostr/MSTestSettings.cs` | MSTest のメソッド単位並列実行を有効化するアセンブリ属性を宣言。 |
| `UnitTest_COM_Nostr/Test1.cs` | NostrSigner の署名生成と検証動作を確認する MSTest テストケース群。 |
| `UnitTest_COM_Nostr/NostrClientInitializationTests.cs` | Initialize の正規化／異常系に加え、二重 Initialize と Dispose 後再利用の境界テストを含む MSTest ケース。 |
| `UnitTest_COM_Nostr/NostrRelaySessionTests.cs` | docker で strfry リレーを起動し RelaySession の接続と NIP-11 取得を検証する MSTest。 |
| `UnitTest_COM_Nostr/NostrSubscriptionTests.cs` | docker strfry を用いた購読テスト。EOSE/KeepAlive、同期コンテキスト無しでのコールバック dispatch、キュー overflow (`DropOldest`/`Throw`)、リレー再起動後の自動再接続を検証。 |
| `UnitTest_COM_Nostr/NostrPublishEventTests.cs` | docker strfry を用いた EVENT 送信の署名成功ケースと署名不正時の NOTICE/COMException を検証する MSTest。 |
| `UnitTest_COM_Nostr/NostrAuthTests.cs` | モックセッションで AUTH メッセージや `auth-required` プレフィックスの通知連携を検証する MSTest。 |
| `UnitTest_COM_Nostr/StrfryRelayHost.cs` | テストごとに strfry コンテナを起動・停止し、`RestartAsync` でリレー再起動シナリオも提供する補助ユーティリティ。 |
| `UnitTest_COM_Nostr/UnitTest_COM_Nostr.csproj` | テストプロジェクトのターゲットフレームワークや参照設定を定義。 |
| `ImplementationPlan.md` | COM_Nostr 実装フェーズとテスト戦略をまとめた計画ドキュメント (完了タスクはチェックボックスで管理)。 |
| `CHANGELOG.md` | リリース履歴（初版 0.1.0 の主要トピック）を記録。 |
| `docs/phase0_design.md` | フェーズ0で決定したクラス構成、WebSocket/コールバック方針、例外マッピングの詳細設計メモ。 |
| `docs/installer_regsvr32_plan.md` | MSIでcomhost DLLをregsvr32登録するためのCustom Action追加方針と検証計画。 |
| `TROUBLESHOOTING.md` | QueueOverflowStrategy、docker strfry 再起動時の注意点、PowerShell 7 の 0x800080A5、32bit/64bit 登録ミスマッチ、.NET 8 アセンブリに対する tlbexp 失敗時の対処法をまとめたトラブルシュートメモ。 |

> ※ バイナリ形式の `Nostrプロトコルの現行仕様まとめ.docx` は対象外としています。
