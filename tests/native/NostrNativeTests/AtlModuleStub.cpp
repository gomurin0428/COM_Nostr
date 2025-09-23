#include "pch.h"

#include "../../../COM_Nostr_Native/COMNostrNative_i.h"

class CTestAtlModule : public ATL::CAtlDllModuleT<CTestAtlModule>
{
public:
    DECLARE_LIBID(LIBID_COMNostrNativeLib)
};

CTestAtlModule _AtlModule;
