# COM_Nostr_Native 完成計画 (2025-09-25)

## 目的
- COM_Nostr_Native で公開予定の全 COM インターフェイス (`INostrClient` / `INostrRelaySession` / `INostrSubscription` / DTO・コールバック) を .NET 版と同等の契約で実装する。
- strfry リレーを用いた統合テストとネイティブ単体テストの双方で主要シナリオ (接続・購読・イベント送信・AUTH) を再現し、全テストを通過させる。
- ビルド・登録・デバッグ手順を最新化し、他開発者が追加作業なしでビルド／テスト／配布できる状態を作る。

## 現状整理 (2025-09-25 時点)
- プロジェクトは C++/ATL の骨組みが作成済みだが、`CNostrClient` / `CNostrRelaySession` / `CNostrSubscription` の全メソッドが `E_NOTIMPL` スタブのまま。
- DTO とシリアライザ、WinHTTP ベースのランタイムコンポーネントは雛形が存在するが、セッション管理・購読・イベント送受信との接続が未実装。
- C# MSTest 側では COM_Nostr_Native 完成後に通すことを想定したテストケースが追加済みだが、現状は `E_NOTIMPL` により失敗する。
- msbuild による Debug/Release|x64 ビルドは依存ライブラリ整備後に成功しており、レジストリ登録もオブジェクトマップ拡張で準備済み。
- TROUBLESHOOTING.md には WinHTTP / docker 操作の既知課題が記載されているが、Native 実装固有の落とし穴更新が必要。

## ワークストリームと順序
1. **基盤リファクタ**: ランタイム資源 (`NativeHttpClient`, `WinHttpWebSocket`, DTO 変換) を最終形の API で固定し、共通同期・エラーマッピングヘルパーを整える。
2. **リレーセッション実装**: WinHTTP 接続・NIP-11 取得・状態遷移・LastOkResult 更新を `CNostrRelaySession` と内部セッション管理クラスに実装する。
3. **購読・受信パイプライン**: `CNostrSubscription` とコールバックディスパッチャを完成させ、REQ/EOSE/CLOSE のシーケンスとキュー制御を実装する。
4. **イベント送信・AUTH**: 署名フロー、Publish/AUTH 応答待ち、エラーハンドリング、`INostrSigner` 連携を仕上げる。
5. **COM 公開と仕上げ**: `CNostrClient` の公開 API を組み上げ、Dispose/リソース解放、登録スクリプト、ドキュメント・テスト群を更新する。

## 詳細タスク

### INostrClient
- [x] `Initialize`: ClientOptions DTO 正規化、`ComCallbackDispatcher` 起動、二重初期化検知 (`E_NOSTR_ALREADY_INITIALIZED`) を追加。
- [x] `SetSigner`: `INostrSigner` COM 参照の取得・保持・解放と `E_POINTER`/`E_NOSTR_OBJECT_DISPOSED` ハンドリング。
- [ ] `ConnectRelay`: RelayDescriptor 検証→セッション生成→NIP-11 取得→WebSocket 接続→セッション辞書登録→`INostrAuthCallback` 通知の流れを実装。
- [ ] `DisconnectRelay` / `HasRelay` / `ListRelays`: セッション辞書を同期化し、URL 正規化 (`RelayUriUtilities`) を通じて判定。
- [ ] `OpenSubscription`: フィルタ検証、購読生成、コールバック登録、`SubscriptionOptions` 適用を行い、購読 ID を返す。
- [ ] `PublishEvent` / `RespondAuth`: 署名補完、JSON 生成、送信、`LastOkResult` 更新、タイムアウト (`HRESULT_FROM_WIN32(ERROR_TIMEOUT)`) 処理を実装。
- [ ] `RefreshRelayInfo`: NIP-11 再取得と `RelayDescriptor.Metadata` 更新をセッションへ委譲。
- [ ] Dispose/FinalRelease: 受信スレッド・セッション・購読を順次停止し、コールバックスレッドを解放。

### INostrRelaySession
- [ ] 状態管理フィールドと `RelayDescriptor` 保持を実装し、`get_Url`/`get_State`/`get_WriteEnabled`/`get_ReadEnabled` を返す。
- [ ] WinHTTP 経由で NIP-11 を取得し、`SupportedNips`/`Metadata` に反映するロジックを追加。
- [ ] WebSocket 接続・再接続 (`Reconnect`)・明示切断 (`Close`) の実装と、`E_NOSTR_RELAY_NOT_CONNECTED` マッピングを行う。
- [ ] `LastOkResult` キャッシュの更新と COM DTO 変換 (`get_LastOkResult`) を実装。
- [ ] `UpdatePolicy`: RelayDescriptor 差分適用、Preferred 切り替え、KeepAlive 設定を `WinHttpWebSocket` へ連携。

### INostrSubscription
- [ ] `get_Id`/`get_Status`/`get_Filters`: 内部状態と SAFEARRAY 変換を実装。
- [ ] `UpdateFilters`: フィルタ正規化→REQ 再送→`AutoRequeryWindowSeconds` 補正→キュー再初期化を行う。
- [ ] `Close`: `CLOSE` 送信→受信待機→状態遷移→コールバック通知 (`OnClosed`) を実装。
- [ ] キュー溢れ (`MaxQueueLength`、`QueueOverflowStrategy`) の制御と `DropOldest`/`Throw` 分岐を実装。
- [ ] EVENT/EOSE/NOTICE ハンドリングを `ComCallbackDispatcher` にポストし、STA スレッドで直列化する。

### 共通ランタイム/DTO
- [ ] `ComCallbackDispatcher`: STA ワーカー生成、`Post`/`Shutdown`、例外→HRESULT マッピングを仕上げる。
- [ ] `NativeHttpClient` / `WinHttpWebSocket`: エラーコード→HRESULT 変換、タイムアウト、心拍/再接続トリガーを整備。
- [ ] DTO 変換 (`NostrJsonSerializer`, `ComValueHelpers`): `AuthChallenge`/`NostrOkResult`/`SubscriptionOptions` の相互変換と入力検証強化。
- [ ] `BackoffPolicy` と再接続アルゴリズムを実装し、セッション側で利用する。
- [ ] 署名ヘルパー: `libsecp256k1` を用いた `Id` 計算と署名検証 (デバッグ用) を実装。

### COM 公開・ビルド
- [ ] `Object Map` に最終的な CoClass/DTO を登録し、`DllRegisterServer`/`DllUnregisterServer` が期待通り動作することを確認。
- [ ] `COM_Nostr_Native.def` と Proxy/Stub のエクスポートを更新し、`regsvr32` 成功後に COM 生成できるよう調整。
- [ ] `build/native-deps.ps1` を最終構成に同期し、Release/Debug 両方の依存生成を CI で行えるようにする。
- [ ] `msbuild COM_Nostr_Native.vcxproj` (Debug/Release|x64) のビルドパイプラインと署名者 COM 登録スクリプトを Automation で整備。

## テスト戦略
- ビルド検証: `msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration={Debug,Release} /p:Platform=x64` を毎フェーズ完了時に実行し、警告ゼロを維持。
- C++ 単体テスト: `tests/native` を拡充し、HTTP/WebSocket モック、JSON 変換、Backoff、署名検証をカバー。`ctest --output-on-failure` を 20 秒タイムアウトで別プロセス実行。
- C# 統合テスト: `UnitTest_COM_Nostr_Native` (新規) を MSTest で作成し、テストごとに `docker run --rm -it -p <動的ポート>:7777 -v "${hostPath}:/app/strfry-db" --name strfry_<test> dockurr/strfry` を起動。各テスト終了時に確実に停止するヘルパーを整備。
- COM 登録検証: 64bit PowerShell で `regsvr32` → `New-Object -ComObject COM_Nostr_Native.NostrClient` を自動化し、`CLASS_E_CLASSNOTAVAILABLE` が解消されていることを確認。
- パフォーマンス/リソース: 長時間購読テストと Dispose テストでメモリリーク検診 (Debug CRT レポート) を実施。

## ドキュメントと共有資産更新
- README.md: Native 実装の完了内容、依存ライブラリ、制限事項 (WinHTTP 圧縮非対応等)、登録手順を更新。
- `docs/native_port_overview.md`: 実装済みコンポーネントの構成図とシーケンス図を完成版へ差し替え。
- `TROUBLESHOOTING.md`: WebSocket ハンドル解放漏れ、docker タイムアウト、`regsvr32` 失敗時の確認手順など新たな落とし穴を追記。
- `TEXT_FILE_OVERVIEW.md`: 新規追加ソース/テストの概要を追記し、ファイル全体の整合性を維持。
- `history.md` / `CHANGELOG.md`: マイルストーン達成時に作業内容と日時を記録。

## マイルストーン
| マイルストーン | 内容 | 完了条件 |
| --- | --- | --- |
| M1 | リレーセッションと購読基盤の実装完了 | `ConnectRelay`→`OpenSubscription`→`EOSE` のシナリオが C++ 単体テストで成功 |
| M2 | Publish/AUTH とエラーハンドリング完成 | `PublishEvent`/`RespondAuth` MSTest が docker strfry 上で成功し、LastOkResult が更新される |
| M3 | COM 公開と文書更新 | 64bit PowerShell から COM 生成成功、全テスト (msbuild + ctest + dotnet test) が通り README/TROUBLESHOOTING が更新済み |

## リスクと対策
- WinHTTP WebSocket が permessage-deflate を要求するリレーと互換性がない → README と TROUBLESHOOTING に制限を明記し、NOTICE 経由で利用者に通知。
- `INostrSigner` の外部依存が未準備の場合にテストが失敗する → テスト前に既定サイナー (C# 実装) を登録する PowerShell スクリプトを整備。
- docker 環境差異で strfry 起動が遅延する → テストヘルパーでリトライとログ収集を実装し、タイムアウトは 20 秒→40 秒まで段階的に拡張。
- COM コールバック STA スレッドが停止するリスク → `ComCallbackDispatcher` にウォッチドッグと停止検出ログを追加し、TROUBLESHOOTING に復旧手順を記載。
