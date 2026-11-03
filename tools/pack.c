#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

typedef struct IAppxPackageWriter IAppxPackageWriter;
typedef struct IAppxFactory IAppxFactory;
typedef struct IUri IUri;
HRESULT WINAPI CreateUri(LPCWSTR uri, DWORD flags, DWORD_PTR reserved, IUri **out);

typedef struct APPX_PACKAGE_SETTINGS {
    BOOL forceZip32;
    IUri *hashMethod;
} APPX_PACKAGE_SETTINGS;

typedef struct IAppxPackageWriterVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAppxPackageWriter *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IAppxPackageWriter *);
    ULONG (STDMETHODCALLTYPE *Release)(IAppxPackageWriter *);
    HRESULT (STDMETHODCALLTYPE *AddPayloadFile)(IAppxPackageWriter *, LPCWSTR fileName, LPCWSTR contentType, int compression, IStream *input);
    HRESULT (STDMETHODCALLTYPE *Close)(IAppxPackageWriter *, IStream *manifest);
} IAppxPackageWriterVtbl;

struct IAppxPackageWriter {
    const IAppxPackageWriterVtbl *lpVtbl;
};

typedef struct IAppxFactoryVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAppxFactory *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(IAppxFactory *);
    ULONG (STDMETHODCALLTYPE *Release)(IAppxFactory *);
    HRESULT (STDMETHODCALLTYPE *CreatePackageWriter)(IAppxFactory *, IStream *output, APPX_PACKAGE_SETTINGS *settings, IAppxPackageWriter **writer);
} IAppxFactoryVtbl;

struct IAppxFactory {
    const IAppxFactoryVtbl *lpVtbl;
};

static const GUID CLSID_AppxFactory = {0x5842a140, 0xff9f, 0x4166, {0x8f, 0x5c, 0x62, 0xf5, 0xb7, 0xb0, 0xc7, 0x81}};
static const GUID IID_IAppxFactory = {0xbeb94909, 0xe451, 0x438b, {0xb5, 0xa7, 0xd7, 0x9e, 0x76, 0x7b, 0x75, 0xd8}};

static HRESULT OpenRead(const wchar_t *path, IStream **stream) {
    return SHCreateStreamOnFileEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, NULL, stream);
}

static HRESULT AddFile(IAppxPackageWriter *writer, const wchar_t *packageName, const wchar_t *diskPath) {
    IStream *stream = NULL;
    HRESULT hr = OpenRead(diskPath, &stream);
    if (FAILED(hr)) {
        wprintf(L"open %s failed %08lx\n", diskPath, (unsigned long)hr);
        return hr;
    }
    hr = writer->lpVtbl->AddPayloadFile(writer, packageName, L"image/png", 1, stream);
    stream->lpVtbl->Release(stream);
    if (FAILED(hr)) wprintf(L"add %s failed %08lx\n", packageName, (unsigned long)hr);
    return hr;
}

__declspec(dllexport) int PackMsix(void) {
    printf("settings size %zu\n", sizeof(APPX_PACKAGE_SETTINGS));
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        printf("CoInitialize %08lx\n", (unsigned long)hr);
        return 1;
    }

    IAppxFactory *factory = NULL;
    hr = CoCreateInstance(&CLSID_AppxFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IAppxFactory, (void **)&factory);
    if (FAILED(hr)) {
        printf("CoCreateInstance %08lx\n", (unsigned long)hr);
        return 1;
    }

    CreateDirectoryW(L"pkg", NULL);
    DeleteFileW(L"pkg\\Cursor.OpenProject.msix");

    IStream *out = NULL;
    hr = SHCreateStreamOnFileEx(L"pkg\\Cursor.OpenProject.msix", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, FILE_ATTRIBUTE_NORMAL, TRUE, NULL, &out);
    if (FAILED(hr)) {
        printf("create msix %08lx\n", (unsigned long)hr);
        return 1;
    }

    IUri *hashUri = NULL;
    hr = CreateUri(L"http://www.w3.org/2001/04/xmlenc#sha256", 0x100, 0, &hashUri);
    printf("CreateUri %08lx\n", (unsigned long)hr);
    if (FAILED(hr)) return 1;

    APPX_PACKAGE_SETTINGS settings;
    ZeroMemory(&settings, sizeof(settings));
    settings.forceZip32 = FALSE;
    settings.hashMethod = hashUri;

    IAppxPackageWriter *writer = NULL;
    hr = factory->lpVtbl->CreatePackageWriter(factory, out, &settings, &writer);
    if (FAILED(hr)) {
        printf("CreatePackageWriter %08lx\n", (unsigned long)hr);
        return 1;
    }

    const wchar_t *files[] = {
        L"Assets\\StoreLogo.png",
        L"Assets\\Square44x44Logo.png",
        L"Assets\\Square150x150Logo.png"
    };
    for (int i = 0; i < 3; i++) {
        hr = AddFile(writer, files[i], files[i]);
        if (FAILED(hr)) return 1;
    }

    IStream *manifest = NULL;
    hr = OpenRead(L"AppxManifest.xml", &manifest);
    if (FAILED(hr)) {
        printf("open manifest %08lx\n", (unsigned long)hr);
        return 1;
    }
    hr = writer->lpVtbl->Close(writer, manifest);
    printf("Close %08lx\n", (unsigned long)hr);
    manifest->lpVtbl->Release(manifest);
    writer->lpVtbl->Release(writer);
    out->lpVtbl->Release(out);
    factory->lpVtbl->Release(factory);
    CoUninitialize();
    return FAILED(hr) ? 1 : 0;
}
