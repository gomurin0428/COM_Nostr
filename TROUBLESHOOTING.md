# Troubleshooting

- NostrClient は 1 度しか Initialize できません。2 回目以降は `E_NOSTR_ALREADY_INITIALIZED` が返るため、新しいインスタンスを生成してください。`Dispose()` 後に任意の API を呼び出すと `E_NOSTR_OBJECT_DISPOSED` が返るので、再利用せず破棄してください。
- QueueOverflowStrategy を Throw に設定した状態で購読コールバックが長時間ブロックすると、Subscription queue overflow. で CLOSED になる。MaxQueueLength を十分に確保するか、DropOldest に切り替えてイベントを間引いてください。
- Docker 上の strfry リレーを再起動するテスト (RestartAsync) を実行する際は、既存コンテナとポート競合しないことを確認してください。停止済みでも --rm オプションが動作しない環境では手動で docker stop が必要です。
- RestartAsync は毎回コンテナ名を再生成するため、テストが異常終了した場合は docker ps -a で孤立した strfry-test-* を停止・削除してから再実行してください。

