# コードスタイルと設計方針
- C++ 側は C++20 固定、警告をエラー扱い、ATL/RAII で COM ライフサイクルと同期を管理し、WinHTTP WebSocket を Threadpool Work で受信する。
- Fallback 実装は禁止され、未完了機能は E_NOTIMPL か API 未公開で明示する。C# の新しい拡張メソッド追加は禁止。
- テストは strfry リレーを Docker (`dockurr/strfry`) で起動しつつ実行し、テストケースごとに独立したリレーを使う。Docker Desktop が停止しているとタイムアウトするため事前に稼働確認する。
- ドキュメント (README.md/TROUBLESHOOTING.md/TEXT_FILE_OVERVIEW.md) は変更内容に合わせて更新または削除し、Nostr 仕様の参照は `Nostrプロトコルの現行仕様まとめ.docx` を基準とする。