// dllmain.h : モジュール クラスの宣言です。

class CCOMNostrNativeModule : public ATL::CAtlDllModuleT< CCOMNostrNativeModule >
{
public :
	DECLARE_LIBID(LIBID_COMNostrNativeLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_COMNOSTRNATIVE, "{73a9e0cc-1c6c-4e6f-bbec-ce22a88b4087}")
};

extern class CCOMNostrNativeModule _AtlModule;
