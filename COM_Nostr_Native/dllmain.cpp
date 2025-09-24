// dllmain.cpp : DllMain の実装です。

#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "COMNostrNative_i.h"
#include "dllmain.h"
#include "src/com/NostrComClasses.h"
#include "src/dto/NostrDtoComObjects.h"

using com::nostr::native::CAuthChallenge;
using com::nostr::native::CClientOptions;
using com::nostr::native::CNostrClient;
using com::nostr::native::CNostrEvent;
using com::nostr::native::CNostrEventDraft;
using com::nostr::native::CNostrFilter;
using com::nostr::native::CNostrOkResult;
using com::nostr::native::CNostrRelaySession;
using com::nostr::native::CNostrSigner;
using com::nostr::native::CNostrSubscription;
using com::nostr::native::CNostrTagQuery;
using com::nostr::native::CRelayDescriptor;
using com::nostr::native::CSubscriptionOptions;

OBJECT_ENTRY_AUTO(__uuidof(NostrClient), CNostrClient)
OBJECT_ENTRY_AUTO(__uuidof(NostrRelaySession), CNostrRelaySession)
OBJECT_ENTRY_AUTO(__uuidof(NostrSubscription), CNostrSubscription)
OBJECT_ENTRY_AUTO(__uuidof(NostrSigner), CNostrSigner)
OBJECT_ENTRY_AUTO(__uuidof(NostrEvent), CNostrEvent)
OBJECT_ENTRY_AUTO(__uuidof(NostrTagQuery), CNostrTagQuery)
OBJECT_ENTRY_AUTO(__uuidof(NostrFilter), CNostrFilter)
OBJECT_ENTRY_AUTO(__uuidof(RelayDescriptor), CRelayDescriptor)
OBJECT_ENTRY_AUTO(__uuidof(SubscriptionOptions), CSubscriptionOptions)
OBJECT_ENTRY_AUTO(__uuidof(AuthChallenge), CAuthChallenge)
OBJECT_ENTRY_AUTO(__uuidof(NostrEventDraft), CNostrEventDraft)
OBJECT_ENTRY_AUTO(__uuidof(ClientOptions), CClientOptions)
OBJECT_ENTRY_AUTO(__uuidof(NostrOkResult), CNostrOkResult)

CCOMNostrNativeModule _AtlModule;

// DLL エントリ ポイント
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    return _AtlModule.DllMain(dwReason, lpReserved);
}
