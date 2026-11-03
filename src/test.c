#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>

typedef HRESULT (STDMETHODCALLTYPE *DllGetClassObject_t)(REFCLSID, REFIID, void **);

int main(void) {
    HMODULE module = LoadLibraryW(L"CursorExplorerCommand.dll");
    if (!module) {
        printf("load fail %lu\n", GetLastError());
        return 1;
    }
    DllGetClassObject_t get_class = (DllGetClassObject_t)GetProcAddress(module, "DllGetClassObject");
    if (!get_class) {
        printf("missing export\n");
        return 1;
    }

    const GUID clsid = {0xE4C91A27, 0x5B6D, 0x4F38, {0x9A, 0x10, 0x7D, 0x2E, 0x8C, 0x4B, 0x6F, 0x51}};
    IClassFactory *factory = NULL;
    HRESULT hr = get_class(&clsid, &IID_IClassFactory, (void **)&factory);
    printf("factory %08lx\n", (unsigned long)hr);
    if (FAILED(hr)) return 1;

    IExplorerCommand *cmd = NULL;
    hr = factory->lpVtbl->CreateInstance(factory, NULL, &IID_IExplorerCommand, (void **)&cmd);
    printf("create %08lx\n", (unsigned long)hr);
    if (FAILED(hr)) return 1;

    LPWSTR title = NULL;
    hr = cmd->lpVtbl->GetTitle(cmd, NULL, &title);
    char titleUtf8[256] = {0};
    if (title) WideCharToMultiByte(CP_UTF8, 0, title, -1, titleUtf8, sizeof(titleUtf8), NULL, NULL);
    printf("title %08lx [%s]\n", (unsigned long)hr, titleUtf8);

    EXPCMDSTATE state = 99;
    hr = cmd->lpVtbl->GetState(cmd, NULL, FALSE, &state);
    printf("state %08lx value %lu\n", (unsigned long)hr, (unsigned long)state);

    LPWSTR icon = NULL;
    hr = cmd->lpVtbl->GetIcon(cmd, NULL, &icon);
    char iconUtf8[400] = {0};
    if (icon) WideCharToMultiByte(CP_UTF8, 0, icon, -1, iconUtf8, sizeof(iconUtf8), NULL, NULL);
    printf("icon %08lx [%s]\n", (unsigned long)hr, iconUtf8);

    if (title) CoTaskMemFree(title);
    if (icon) CoTaskMemFree(icon);
    cmd->lpVtbl->Release(cmd);
    factory->lpVtbl->Release(factory);
    return 0;
}
