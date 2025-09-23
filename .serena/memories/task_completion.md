# タスク完了前チェック
- 変更後は必ず msbuild で Debug|x64 ビルドを通し、必要に応じて Release も確認する。
- テストを走らせる場合は、各ケース専用の strfry Docker コンテナを事前に起動し、実行後は確実に停止 ( `docker stop <name>` ) させる。
- ドキュメントと TEXT_FILE_OVERVIEW.md を変更内容に合わせて更新し、落とし穴は TROUBLESHOOTING.md に記録する。
- history.md に今回の作業内容と確認コマンドを追記する。