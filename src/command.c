#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const GUID CLSID_OpenInCursor = {
    0xE4C91A27, 0x5B6D, 0x4F38, {0x9A, 0x10, 0x7D, 0x2E, 0x8C, 0x4B, 0x6F, 0x51}
};

static const wchar_t kTitle[] = L"Open project in Cursor";

static HMODULE g_module;
static LONG g_obj;
static LONG g_lock;

typedef struct Command {
    IExplorerCommandVtbl *lpVtbl;
    LONG ref;
} Command;

static void ApplyDefaultExe(wchar_t *buf, size_t cch) {
    wchar_t local[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        _snwprintf(buf, cch, L"%s\\Programs\\cursor\\Cursor.exe", local);
        buf[cch - 1] = 0;
        return;
    }
    wcsncpy(buf, L"Cursor.exe", cch - 1);
    buf[cch - 1] = 0;
}

static void ResolveCursorExe(wchar_t *buf, size_t cch) {
    ApplyDefaultExe(buf, cch);
    if (!g_module) return;

    wchar_t mod[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_module, mod, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    wchar_t *slash = wcsrchr(mod, L'\\');
    if (!slash) return;
    if ((size_t)(slash - mod) + 12 >= MAX_PATH) return;
    wcscpy(slash + 1, L"cursor.path");

    HANDLE file = CreateFileW(mod, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;

    char raw[1024];
    DWORD got = 0;
    BOOL ok = ReadFile(file, raw, sizeof(raw) - 1, &got, NULL);
    CloseHandle(file);
    if (!ok || !got) return;
    raw[got] = 0;

    char *s = raw;
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) {
        *--end = 0;
    }
    if (!*s) return;

    wchar_t configured[1024];
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, configured, 1024)) return;
    DWORD expanded = ExpandEnvironmentStringsW(configured, buf, (DWORD)cch);
    if (expanded == 0 || expanded > cch) ApplyDefaultExe(buf, cch);
}

static HRESULT STDMETHODCALLTYPE Cmd_QueryInterface(IExplorerCommand *This, REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IExplorerCommand)) {
        *ppv = This;
        This->lpVtbl->AddRef(This);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Cmd_AddRef(IExplorerCommand *This) {
    return (ULONG)InterlockedIncrement(&((Command *)This)->ref);
}

static ULONG STDMETHODCALLTYPE Cmd_Release(IExplorerCommand *This) {
    Command *self = (Command *)This;
    ULONG n = (ULONG)InterlockedDecrement(&self->ref);
    if (n == 0) {
        InterlockedDecrement(&g_obj);
        free(self);
    }
    return n;
}

static HRESULT STDMETHODCALLTYPE Cmd_GetTitle(IExplorerCommand *This, IShellItemArray *items, LPWSTR *name) {
    (void)This;
    (void)items;
    if (!name) return E_POINTER;
    return SHStrDupW(kTitle, name);
}

static HRESULT STDMETHODCALLTYPE Cmd_GetIcon(IExplorerCommand *This, IShellItemArray *items, LPWSTR *icon) {
    (void)This;
    (void)items;
    if (!icon) return E_POINTER;
    wchar_t exe[MAX_PATH];
    wchar_t spec[MAX_PATH + 8];
    ResolveCursorExe(exe, MAX_PATH);
    _snwprintf(spec, MAX_PATH + 8, L"%s,0", exe);
    spec[MAX_PATH + 7] = 0;
    return SHStrDupW(spec, icon);
}

static HRESULT STDMETHODCALLTYPE Cmd_GetToolTip(IExplorerCommand *This, IShellItemArray *items, LPWSTR *tip) {
    (void)This;
    (void)items;
    if (tip) *tip = NULL;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE Cmd_GetCanonicalName(IExplorerCommand *This, GUID *guid) {
    (void)This;
    if (!guid) return E_POINTER;
    *guid = CLSID_OpenInCursor;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Cmd_GetState(IExplorerCommand *This, IShellItemArray *items, BOOL slow, EXPCMDSTATE *state) {
    (void)This;
    (void)items;
    (void)slow;
    if (!state) return E_POINTER;
    wchar_t exe[MAX_PATH];
    ResolveCursorExe(exe, MAX_PATH);
    *state = (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) ? ECS_HIDDEN : ECS_ENABLED;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Cmd_Invoke(IExplorerCommand *This, IShellItemArray *items, IBindCtx *ctx) {
    (void)This;
    (void)ctx;
    if (!items) return E_INVALIDARG;

    DWORD count = 0;
    HRESULT hr = items->lpVtbl->GetCount(items, &count);
    if (FAILED(hr)) return hr;
    if (count == 0) return E_INVALIDARG;

    wchar_t *params = (wchar_t *)calloc(32768, sizeof(wchar_t));
    wchar_t *cmdline = (wchar_t *)calloc(32768, sizeof(wchar_t));
    if (!params || !cmdline) {
        free(params);
        free(cmdline);
        return E_OUTOFMEMORY;
    }

    wchar_t firstDir[4096];
    firstDir[0] = 0;
    size_t used = 0;

    DWORD limit = count > 16 ? 16 : count;
    for (DWORD i = 0; i < limit; i++) {
        IShellItem *item = NULL;
        if (FAILED(items->lpVtbl->GetItemAt(items, i, &item)) || !item) continue;
        LPWSTR name = NULL;
        hr = item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &name);
        item->lpVtbl->Release(item);
        if (FAILED(hr) || !name) continue;

        if (!firstDir[0]) {
            wcsncpy(firstDir, name, 4095);
            firstDir[4095] = 0;
        }

        size_t nlen = wcslen(name);
        if (used + nlen + 4 >= 32767) {
            CoTaskMemFree(name);
            break;
        }
        if (used) params[used++] = L' ';
        params[used++] = L'"';
        memcpy(params + used, name, nlen * sizeof(wchar_t));
        used += nlen;
        params[used++] = L'"';
        params[used] = 0;
        CoTaskMemFree(name);
    }

    if (!params[0]) {
        free(params);
        free(cmdline);
        return E_FAIL;
    }

    wchar_t exe[MAX_PATH];
    ResolveCursorExe(exe, MAX_PATH);
    _snwprintf(cmdline, 32768, L"\"%s\" %s", exe, params);
    cmdline[32767] = 0;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    DWORD flags = CREATE_BREAKAWAY_FROM_JOB | CREATE_NEW_PROCESS_GROUP;
    BOOL ok = CreateProcessW(exe, cmdline, NULL, NULL, FALSE, flags, NULL, firstDir[0] ? firstDir : NULL, &si, &pi);
    if (!ok) {
        ok = CreateProcessW(exe, cmdline, NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP, NULL, firstDir[0] ? firstDir : NULL, &si, &pi);
    }

    free(params);
    free(cmdline);
    if (!ok) return HRESULT_FROM_WIN32(GetLastError());
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Cmd_GetFlags(IExplorerCommand *This, EXPCMDFLAGS *flags) {
    (void)This;
    if (!flags) return E_POINTER;
    *flags = ECF_DEFAULT;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Cmd_EnumSubCommands(IExplorerCommand *This, IEnumExplorerCommand **commands) {
    (void)This;
    if (commands) *commands = NULL;
    return E_NOTIMPL;
}

static const IExplorerCommandVtbl g_cmdVtbl = {
    Cmd_QueryInterface,
    Cmd_AddRef,
    Cmd_Release,
    Cmd_GetTitle,
    Cmd_GetIcon,
    Cmd_GetToolTip,
    Cmd_GetCanonicalName,
    Cmd_GetState,
    Cmd_Invoke,
    Cmd_GetFlags,
    Cmd_EnumSubCommands
};

static HRESULT STDMETHODCALLTYPE Factory_QueryInterface(IClassFactory *This, REFIID riid, void **ppv) {
    (void)This;
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory)) {
        *ppv = This;
        This->lpVtbl->AddRef(This);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Factory_AddRef(IClassFactory *This) {
    (void)This;
    return 2;
}

static ULONG STDMETHODCALLTYPE Factory_Release(IClassFactory *This) {
    (void)This;
    return 1;
}

static HRESULT STDMETHODCALLTYPE Factory_CreateInstance(IClassFactory *This, IUnknown *outer, REFIID riid, void **ppv) {
    (void)This;
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;

    Command *cmd = (Command *)calloc(1, sizeof(Command));
    if (!cmd) return E_OUTOFMEMORY;
    cmd->lpVtbl = (IExplorerCommandVtbl *)&g_cmdVtbl;
    cmd->ref = 1;
    InterlockedIncrement(&g_obj);

    HRESULT hr = cmd->lpVtbl->QueryInterface((IExplorerCommand *)cmd, riid, ppv);
    cmd->lpVtbl->Release((IExplorerCommand *)cmd);
    return hr;
}

static HRESULT STDMETHODCALLTYPE Factory_LockServer(IClassFactory *This, BOOL lock) {
    (void)This;
    if (lock) InterlockedIncrement(&g_lock);
    else InterlockedDecrement(&g_lock);
    return S_OK;
}

static const IClassFactoryVtbl g_factoryVtbl = {
    Factory_QueryInterface,
    Factory_AddRef,
    Factory_Release,
    Factory_CreateInstance,
    Factory_LockServer
};

static IClassFactory g_factory = { (IClassFactoryVtbl *)&g_factoryVtbl };

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

HRESULT STDMETHODCALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (!IsEqualGUID(rclsid, &CLSID_OpenInCursor)) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.lpVtbl->QueryInterface(&g_factory, riid, ppv);
}

HRESULT STDMETHODCALLTYPE DllCanUnloadNow(void) {
    return (g_obj == 0 && g_lock == 0) ? S_OK : S_FALSE;
}
