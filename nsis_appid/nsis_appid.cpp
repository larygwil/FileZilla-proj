// Quick and dirty NSIS plugin to set the AppID on startmenu items.
// Windows 7 is so broken, using that COM shit for basic functionality...
// Sadly requires Visual Studio 2008 and recent Platform SDK to compile.

#include <windows.h>
#include <shlobj.h>
#include <propvarutil.h>
#include <propidl.h>
#include <propkey.h>
#include <nsis/nsis_tchar.h>
#include <nsis/pluginapi.h> // nsis plugin

void do_set_appid(LPCTSTR shortcut_path, LPCTSTR appid)
{
	IPropertyStore *store = 0;
	if (SHGetPropertyStoreFromParsingName(shortcut_path, 0, GPS_READWRITE, IID_PPV_ARGS(&store)) != S_OK)
		return;

	PROPVARIANT property;
	if (InitPropVariantFromString(appid, &property) == S_OK)
	{
		if (store->SetValue(PKEY_AppUserModel_ID, property) == S_OK)
			store->Commit();

		PropVariantClear(&property);
	}

	store->Release();
}

extern "C" void* memset(void* dest, int c, size_t count)
{
	char* p = (char*)dest;
	for (size_t i = 0; i < count; ++i)
		p[i] = c;

	return dest;
}

extern "C" {
void __declspec(dllexport) set_appid(HWND hwndParent, int string_size, 
														TCHAR *variables, stack_t **stacktop,
														extra_parameters *extra)
{
	EXDLL_INIT();

	LPTSTR shortcut_path = (LPTSTR)GlobalAlloc(0, string_size * sizeof(TCHAR));
	LPTSTR appid = (LPTSTR)GlobalAlloc(0, string_size * sizeof(TCHAR));

	if (shortcut_path &&
		appid &&
		!popstring(shortcut_path) &&
		!popstring(appid))
	{
		do_set_appid(shortcut_path, appid);
	}
	
	GlobalFree(shortcut_path);
	GlobalFree(appid);
}
}

BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
