# COM_Nostr 実装計画 (2025-09-21)

## ゴールとスコープ
- INostrClient を中心とする公開 COM インターフェイスを全実装し、NIP-01 / 11 / 15 / 20 / 42 / 65 に沿ったリレー接続・購読・投稿・認証フローを提供する。
- 既存の NostrSigner を活用し、署名未設定時の自動署名や LastOkResult の更新など README.md に記載された振る舞いを満たす。
- strfry リレーを用いた統合テストを MSTest で自動化し、docker + dockurr/strfry を用いてテストごとに独立したリレーを起動して検証できるようにする。
- ドキュメント (README.md, TEXT_FILE_OVERVIEW.md, Nostr プロトコル要約) とコードが齟齬なく同期している状態でリリースできるようにする。

## 前提・制約
- .NET 8 / Windows COM 環境で動作すること。全コードは C# で記述し、新たな拡張メソッドは定義しない。
- フォールバック実装は避け、各機能は実際に動作する形で提供する。未実装の場合は明示的に機能を限定し README を更新する。
- テストでは docker と strfry がローカルに存在する前提で、テストケースごとに異なるリレー (ポート＋ボリューム) を立ち上げる。
- COM クライアントからのコールバックはスレッド境界を考慮し、STA/MTA のどちらでも安全に動作するようマルシャリング戦略を設計する。
- ClientOptions.WebSocketFactoryProgId を尊重し、既定実装は ClientWebSocket を使用しつつ、外部 COM ファクトリで代替できる構成を用意する。
- 実装時には Nostrプロトコルの現行仕様まとめ.docx を常に参照し、プロトコル差異があればドキュメントを更新する。

## フェーズ詳細
### フェーズ0: 詳細設計と技術検討
- README.md と Nostr 仕様ドキュメントから機能要件・状態遷移を洗い出し、クラス構成図と責務分担を決定する。
- WebSocket 実装選定 (ClientWebSocket + CancellationToken ラッパ) と COM コールバックマルシャリング方針 (SynchronizationContext or IConnectionPointContainer) を比較し、採用案を README に追記する。
- 例外ポリシー (COMException の HRESULT マッピング、E_NOSTR_SIGNER_MISSING 定数化) とログインターフェイス（必要なら内部のみ）を定義する。
- 成果物: 設計メモ (docs/ 以下)、更新済み README.md の設計セクション。

### フェーズ1: 通信基盤・依存性注入レイヤー
- INostrClient.Initialize で使用する設定モデルとリソース初期化 (HttpClientFactory, WebSocket ファクトリ、シリアライザ) を実装。
- JSON シリアライズ/デシリアライズのヘルパーを用意し、イベント・REQ/OK/NOTICE メッセージの型安全な構造体を定義。
- WebSocket ファクトリを COM ProgID から生成できるよう Type.GetTypeFromProgID → Activator.CreateInstance のガード付きコードを実装し、エラーメッセージを統一。
- 成果物: IWebSocketConnection (内部 interface)、WebSocketConnection 実装、初期化ロジック単体テスト。

### フェーズ2: リレーセッション管理 (INostrRelaySession)
- RelaySessionState の状態遷移 (Disconnected→Connecting→Connected/Faulted) をステートマシンとして実装。
- ConnectRelay で NIP-11 メタデータを HTTP 取得し、RelayDescriptor.Metadata と SupportedNips を更新。
- セッションごとにメッセージ受信ループ (非同期 Task) を起動し、接続終了時にクリーンアップ。
- DisconnectRelay と Reconnect の再接続ロジック、指数バックオフ設定、HasRelay の高速参照を実装。
- 成果物: RelaySession クラス、NIP-11 フェッチ用ユニットテスト (HTTP ハンドラ差し替え)。

### フェーズ3: サブスクリプション制御 (INostrSubscription)
- サブスクリプション ID 生成器 (64 文字 hex) と SubscriptionOptions の反映 (KeepAlive, AutoRequeryWindowSeconds, MaxQueueLength) を実装。
- REQ メッセージ生成・送信、EOSE/CLOSED 受信に応じた Status 遷移、UpdateFilters による再送を実装。
- KeepAlive=false 時のドレイン処理、AutoRequery 対応のための last-seen timestamp 管理を導入。
- 成果物: Subscription クラス、フィルタ変換ユーティリティ、シミュレートしたリレー応答に対する単体テスト。

### フェーズ4: イベント投稿・署名連携
- PublishEvent 内で NostrEvent→NostrEventDraft を生成し、署名が空の場合は INostrSigner 経由で署名＋ID 計算。
- EVENT 送信後、OK または NOTICE 応答を LastOkResult・OnNotice に伝搬し、タイムアウト・再試行ポリシーを設定。
- 署名者未設定時の例外 (E_NOSTR_SIGNER_MISSING) を HRESULT にマップし、COM クライアントに伝達。
- 成果物: EVENT 送信パイプライン、署名ワークフロー単体テスト、タイムアウトのための設定値。

### フェーズ5: 認証 (NIP-42) とリレー側エラー処理
- リレーからの AUTH/uth-required を検知し INostrAuthCallback に通知、AuthChallenge を組み立て。
- RespondAuth で kind:22242 イベントを構築・署名し、AUTH 返信を送信。OK/NOTICE のハンドリングと OnAuthSucceeded/Failed の呼び分けを実装。
- CLOSED／NOTICE の理由テキストを解析し、認証再試行やクライアント通知のトリガーを定義。
- 成果物: AUTH 処理ユニットテスト (モックリレー) と、エラーコード→通知マッピング表。

### フェーズ6: 信頼性・リソース管理
- 送信キューと backpressure 制御 (MaxQueueLength) を実装し、超過時は古いイベント破棄や例外を選択可能にする。
- Keep-alive (PING/PONG またはタイムアウト監視) と自動再接続時の REQ 再送、AutoRequeryWindowSeconds 活用によるギャップ回収を実装。
- Dispose パターンで WebSocket/HTTP リソースを解放し、GC 最適化・多重解放耐性を確認。
- 成果物: リソースクリーンアップテスト、再接続 E2E テストケース。

### フェーズ7: COM 公開整備と API 一貫性
- NostrClient, RelaySession, Subscription に [ClassInterface(ClassInterfaceType.None)] 実装クラスを用意し、必要な COM 属性と GUID を付与。
- COM 例外ラッパーと HRESULT 定数を集中管理する HResults クラス (内部) を作成、全メソッドで統一的に使用。
- Initialize の二重呼び出し・破棄後再利用など境界ケースをテストし、ドキュメント化する。
- 成果物: COM 登録スクリプト更新 (必要なら)、README.md の使用例。

### フェーズ8: テスト & 品質保証
- **ユニットテスト**: メッセージシリアライザ、署名フロー、状態遷移、エラー変換を網羅。
- **統合テスト**: ClassInitialize/TestCleanup でテストごとに docker run --rm -p <port>:7777 -v <temp>:/app/strfry-db を起動・停止し、REQ/EVENT/AUTH フローを実機相当で検証。
- **ストレスシナリオ**: 高頻度 EVENT 送信、接続断→再接続、複数サブスクリプション同時操作を MSTest の DataRow でパラメータ化。
- dotnet build, dotnet test を CI ゲートに設定し、ビルドが通ることをリリース条件とする。

### フェーズ9: ドキュメント・引き渡し
- README.md に最終的な使用手順、サンプルコード、制限事項を追記し、インターフェイス定義の差分があれば反映。
- TEXT_FILE_OVERVIEW.md を更新し、新規ソース・テスト・ドキュメントの概要を追加。
- リリースノート (CHANGELOG 追加を検討) と COM 登録手順 (regasm, self-contained MSI 等) を整理。

## テスト戦略
- WebSocket 処理は擬似リレー (テスト用 WebSocketListener) を使った単体テストと、実リレー (strfry) を使った統合テストの二段構えでカバレッジを確保。
- docker 連携のため、テストごとに一時ディレクトリを生成し、docker run コマンドをラップしたヘルパーを用意して後始末を保証。
- 並列テスト時のポート衝突を避けるため、テストケースごとにランダムポートを割り当て、strfry 起動待機ロジックを実装。
- CI では docker が利用できない場合に備え、統合テストを明示的にカテゴリ分け ([TestCategory("Integration")]) し、必要に応じてスキップ可能にする。

## 開発フローと品質ゲート
- 各フェーズ完了時に dotnet build と関連ユニットテストを実行、フェーズ8完了時に完全な dotnet test (統合テスト含む) を通過させる。
- コードレビュー時は README.md の記述と実装の差分チェックリストを用い、API 破壊変更が無いか検証。
- 例外が HRESULT に正しくマップされているか、COM クライアント (VBScript/Excel など) からの簡易手動テストを行う。

## ドキュメント整備
- docs/ 配下に設計メモ (クラス図、シーケンス図、エラーマッピング表) を配置し、実装に合わせて随時更新。
- README.md のサンプルコードは動作確認済みのものに刷新し、strfry を使った検証手順を明記。
- Nostrプロトコルの現行仕様まとめ.docx と README.md の参照節を相互リンクさせ、仕様更新時に両方を見直すチェックリストを作成。

## リスクと対策
- **WebSocket 実装の制約**: ClientWebSocket の Windows 限定機能や TLS 証明書検証問題に備え、抽象化レイヤーで代替実装を差し替え可能にする。
- **COM コールバックのスレッド問題**: SynchronizationContext を用いた UI スレッドへのポスト、または TaskScheduler.FromCurrentSynchronizationContext() を Initialize 時に捕捉し、明示的に使用する。
- **strfry バージョン差異**: 統合テストで使用する docker タグを固定 (dockurr/strfry:latest の動作確認) し、破壊的変更があればテストを更新。
- **プロトコル変更**: NIP の更新があった場合は README と docs を同時に更新するフローを設け、バージョン差異を CHANGELOG に記載。

## 参考資料
- README.md (最新のインターフェイス仕様)
- Nostrプロトコルの現行仕様まとめ.docx (NIP-01/11/15/20/42/65 要約)
- dockurr/strfry リポジトリ (統合テスト用リレー)
