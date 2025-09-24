// dllmain.cpp : DllMain の実装です。

#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "COMNostrNative_i.h"
#include "dllmain.h"
#include "src/com/NostrComClasses.h"

using com::nostr::native::CNostrClient;
using com::nostr::native::CNostrRelaySession;
using com::nostr::native::CNostrSigner;
using com::nostr::native::CNostrSubscription;

OBJECT_ENTRY_AUTO(__uuidof(NostrClient), CNostrClient)
OBJECT_ENTRY_AUTO(__uuidof(NostrRelaySession), CNostrRelaySession)
OBJECT_ENTRY_AUTO(__uuidof(NostrSubscription), CNostrSubscription)
OBJECT_ENTRY_AUTO(__uuidof(NostrSigner), CNostrSigner)

CCOMNostrNativeModule _AtlModule;

// DLL エントリ ポイント
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    return _AtlModule.DllMain(dwReason, lpReserved);
}
