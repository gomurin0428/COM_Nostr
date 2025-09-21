# Git管理テキストファイル一覧

| ファイル | 概要 |
| --- | --- |
| `.gitattributes` | 改行コードの正規化や特定拡張子のマージ・差分設定方針を定義。 |
| `.gitignore` | Visual Studio/ .NET 開発で不要な生成物を除外するための無視設定。 |
| `Agents.md` | コントリビューター向けの作業ガイドラインとテスト方針を箇条書きで記載。 |
| `README.md` | COM_Nostr コンポーネントの概要、公開 COM インターフェイス、設計方針、データモデルをまとめた中心ドキュメント。 |
| `COM_Nostr.sln` | 本体ライブラリとテストプロジェクトを含む Visual Studio ソリューション定義。 |
| `COM_Nostr/COM_Nostr.csproj` | COM 対応の .NET 8 プロジェクト設定と NBitcoin.Secp256k1 依存パッケージを指定。 |
| `COM_Nostr/Contracts/DataContracts.cs` | COM で公開するイベント、フィルタ、オプション等の DTO クラス群を定義。 |
| `COM_Nostr/Contracts/Enums.cs` | リレー状態とサブスクリプション状態を表す COM 列挙体を提供。 |
| `COM_Nostr/Contracts/Interfaces.cs` | INostrClient など COM インターフェイス群のメソッド／プロパティ契約を宣言。 |
| `COM_Nostr/Contracts/NostrClient.cs` | クライアント・リレーセッション・サブスクリプション実装の骨格 (未実装メソッド) を配置。 |
| `COM_Nostr/Contracts/NostrSigner.cs` | 環境変数の秘密鍵で Schnorr 署名とイベントID計算を行う COM 実装クラス。 |
| `COM_Nostr/Properties/AssemblyInfo.cs` | アセンブリのメタデータと COM 公開設定、テストプロジェクトへの InternalsVisibleTo を構成。 |
| `LICENSE.txt` | MIT License テンプレート本文。 |
| `UnitTest_COM_Nostr/MSTestSettings.cs` | MSTest のメソッド単位並列実行を有効化するアセンブリ属性を宣言。 |
| `UnitTest_COM_Nostr/Test1.cs` | NostrSigner の署名生成と検証動作を確認する MSTest テストケース群。 |
| `UnitTest_COM_Nostr/UnitTest_COM_Nostr.csproj` | テストプロジェクトのターゲットフレームワークや参照設定を定義。 |
| `ImplementationPlan.md` | COM_Nostr 実装フェーズとテスト戦略をまとめた計画ドキュメント。 |
| `docs/phase0_design.md` | フェーズ0で決定したクラス構成、WebSocket/コールバック方針、例外マッピングの詳細設計メモ。 |

> ※ バイナリ形式の `Nostrプロトコルの現行仕様まとめ.docx` は対象外としています。
