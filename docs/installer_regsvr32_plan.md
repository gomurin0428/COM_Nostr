# インストーラ登録対応方針 (2025-09-22)

## 背景
- Release版MSIではCOM_Nostr.comhost.dllが自動登録されず、手動でregsvr32を実行する必要がある。

## 目的
- Release/x64構成のMSIインストール時にcomhost DLLをregsvr32 /sで登録し、使用開始までの手順を自動化する。

## 対応方針
1. SetupRegisterコンソールを拡張し、CustomActionDataで受け取ったターゲットパスを元に`%SystemRoot%\System32\regsvr32.exe`をサイレント実行する。登録失敗時は非0終了コードでMSIを中断する。将来のアンインストール用に/unregisterフラグ対応も実装しておく。
2. SetupRegisterのビルド構成にRelease|x64を追加し、AnyCPUでも64bit CLR上で動作するよう`Prefer32Bit=false`を明示する。
3. Setup_COM_Nostr.vdprojのRelease構成にSetupRegister.exeを成果物として含め、Installイベント(Install/Commitシーケンス)にCustom Actionを追加。CustomActionDataで`/target="[TARGETDIR]COM_Nostr.comhost.dll"`を渡し、64bit OSのみ実行するようConditionに`VersionNT64`を設定する。Uninstallシーケンスにも`/unregister`を渡すCustom Actionを追加する。
4. MSIの登録フローがRelease構成でのみ動作するよう、Debug構成やx86のコンフィグにはCustom Actionを追加しない。

## 検証
- `dotnet build -c Release`でCOM_NostrとSetupRegisterがビルドされることを確認。
- SetupプロジェクトからRelease MSIを再生成し、テスト用VMでインストール → `reg query HKCR\\CLSID\\{...}` もしくは`regsvr32 /u`で登録状態を確認。手動でregsvr32実行なしでクライアントからCOM_Nostrが呼び出せることを目視確認。
- ビルド・インストールログにCustom Actionエラーが出ないことを確認。

## ドキュメント更新
- README.mdのインストール手順を自動登録前提に更新し、手動登録手順は補足として残す。
- TEXT_FILE_OVERVIEW.mdに本ドキュメントと更新対象ファイルを追記。必要に応じTROUBLESHOOTING.mdへ64bit regsvr32の注意点を追加。
