#include <windows.h>

// Proxy/stub DLL skeleton for COM_Nostr_Native.
// The COM interfaces in COM_Nostr_Native rely on Automation-compatible types,
// so we intentionally expose no custom marshaling logic here.
// Keeping the exports allows regsvr32 integration without failing the build.

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID)
{
    return TRUE;
}

extern "C" HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_OK;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID, REFIID, LPVOID* ppv)
{
    if (ppv != nullptr)
    {
        *ppv = nullptr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI DllRegisterServer(void)
{
    return S_OK;
}

extern "C" HRESULT WINAPI DllUnregisterServer(void)
{
    return S_OK;
}
