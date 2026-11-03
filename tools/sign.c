#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <wincrypt.h>
#include <stdio.h>

typedef struct _SIGNER_FILE_INFO {
    DWORD cbSize;
    LPCWSTR pwszFileName;
    HANDLE hFile;
} SIGNER_FILE_INFO;

typedef struct _SIGNER_BLOB_INFO {
    DWORD cbSize;
    GUID *pGuidSubject;
    DWORD cbBlob;
    BYTE *pbBlob;
    LPCWSTR pwszDisplayName;
} SIGNER_BLOB_INFO;

typedef struct _SIGNER_SUBJECT_INFO {
    DWORD cbSize;
    DWORD *pdwIndex;
    DWORD dwSubjectChoice;
    union {
        SIGNER_FILE_INFO *pSignerFileInfo;
        SIGNER_BLOB_INFO *pSignerBlobInfo;
    };
} SIGNER_SUBJECT_INFO;

#define SIGNER_SUBJECT_FILE 0x01

typedef struct _SIGNER_ATTR_AUTHCODE {
    DWORD cbSize;
    BOOL fCommercial;
    BOOL fIndividual;
    LPCWSTR pwszName;
    LPCWSTR pwszInfo;
} SIGNER_ATTR_AUTHCODE;

typedef struct _SIGNER_SIGNATURE_INFO {
    DWORD cbSize;
    ALG_ID algidHash;
    DWORD dwAttrChoice;
    union {
        SIGNER_ATTR_AUTHCODE *pAttrAuthcode;
    };
    PCRYPT_ATTRIBUTES psAuthenticated;
    PCRYPT_ATTRIBUTES psUnauthenticated;
} SIGNER_SIGNATURE_INFO;

#define SIGNER_NO_ATTR 0x00

typedef struct _SIGNER_CERT_STORE_INFO {
    DWORD cbSize;
    PCCERT_CONTEXT pSigningCert;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
} SIGNER_CERT_STORE_INFO;

#define SIGNER_CERT_POLICY_CHAIN 0x02
#define SIGNER_CERT_POLICY_CHAIN_NO_ROOT 0x08

typedef struct _SIGNER_SPC_CHAIN_INFO {
    DWORD cbSize;
    LPCWSTR pwszSpcFile;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
} SIGNER_SPC_CHAIN_INFO;

typedef struct _SIGNER_CERT {
    DWORD cbSize;
    DWORD dwCertChoice;
    union {
        LPCWSTR pwszSpcFile;
        SIGNER_CERT_STORE_INFO *pCertStoreInfo;
        SIGNER_SPC_CHAIN_INFO *pSpcChainInfo;
    };
    HWND hwnd;
} SIGNER_CERT;

#define SIGNER_CERT_STORE 0x02

typedef struct _SIGNER_CONTEXT {
    DWORD cbSize;
    DWORD cbBlob;
    BYTE *pbBlob;
} SIGNER_CONTEXT;

typedef struct _SIGNER_SIGN_EX2_PARAMS SIGNER_SIGN_EX2_PARAMS;
typedef struct _APPX_SIP_CLIENT_DATA {
    SIGNER_SIGN_EX2_PARAMS *pSignerParams;
    IUnknown *pAppxSipState;
} APPX_SIP_CLIENT_DATA;

struct _SIGNER_SIGN_EX2_PARAMS {
    DWORD dwFlags;
    SIGNER_SUBJECT_INFO *pSubjectInfo;
    SIGNER_CERT *pSigningCert;
    SIGNER_SIGNATURE_INFO *pSignatureInfo;
    void *pProviderInfo;
    DWORD dwTimestampFlags;
    PCSTR pszAlgorithmOid;
    PCWSTR pwszTimestampURL;
    PCRYPT_ATTRIBUTES pCryptAttrs;
    void *pSipData;
    SIGNER_CONTEXT **pSignerContext;
    void *pCryptoPolicy;
    void *pReserved;
};

typedef HRESULT (WINAPI *SignerSignEx2Function)(
    DWORD, SIGNER_SUBJECT_INFO *, SIGNER_CERT *, SIGNER_SIGNATURE_INFO *,
    void *, DWORD, PCSTR, PCWSTR, PCRYPT_ATTRIBUTES, void *,
    SIGNER_CONTEXT **, void *, void *);

static HRESULT SignOnce(PCCERT_CONTEXT cert, const wchar_t *path, DWORD policy) {
    DWORD signerIndex = 0;
    SIGNER_FILE_INFO fileInfo = {0};
    fileInfo.cbSize = sizeof(fileInfo);
    fileInfo.pwszFileName = path;

    SIGNER_SUBJECT_INFO subjectInfo = {0};
    subjectInfo.cbSize = sizeof(subjectInfo);
    subjectInfo.pdwIndex = &signerIndex;
    subjectInfo.dwSubjectChoice = SIGNER_SUBJECT_FILE;
    subjectInfo.pSignerFileInfo = &fileInfo;

    SIGNER_CERT_STORE_INFO certStoreInfo = {0};
    certStoreInfo.cbSize = sizeof(certStoreInfo);
    certStoreInfo.dwCertPolicy = policy;
    certStoreInfo.pSigningCert = cert;

    SIGNER_CERT signerCert = {0};
    signerCert.cbSize = sizeof(signerCert);
    signerCert.dwCertChoice = SIGNER_CERT_STORE;
    signerCert.pCertStoreInfo = &certStoreInfo;

    SIGNER_SIGNATURE_INFO signatureInfo = {0};
    signatureInfo.cbSize = sizeof(signatureInfo);
    signatureInfo.algidHash = CALG_SHA_256;
    signatureInfo.dwAttrChoice = SIGNER_NO_ATTR;

    SIGNER_SIGN_EX2_PARAMS signerParams = {0};
    signerParams.pSubjectInfo = &subjectInfo;
    signerParams.pSigningCert = &signerCert;
    signerParams.pSignatureInfo = &signatureInfo;

    APPX_SIP_CLIENT_DATA sipClientData = {0};
    sipClientData.pSignerParams = &signerParams;
    signerParams.pSipData = &sipClientData;

    HMODULE module = LoadLibraryExW(L"MSSign32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return HRESULT_FROM_WIN32(GetLastError());
    SignerSignEx2Function sign = (SignerSignEx2Function)GetProcAddress(module, "SignerSignEx2");
    HRESULT hr = E_FAIL;
    if (!sign) hr = HRESULT_FROM_WIN32(GetLastError());
    else hr = sign(
        signerParams.dwFlags,
        signerParams.pSubjectInfo,
        signerParams.pSigningCert,
        signerParams.pSignatureInfo,
        signerParams.pProviderInfo,
        signerParams.dwTimestampFlags,
        signerParams.pszAlgorithmOid,
        signerParams.pwszTimestampURL,
        signerParams.pCryptAttrs,
        signerParams.pSipData,
        signerParams.pSignerContext,
        signerParams.pCryptoPolicy,
        signerParams.pReserved);
    FreeLibrary(module);
    if (sipClientData.pAppxSipState) sipClientData.pAppxSipState->lpVtbl->Release(sipClientData.pAppxSipState);
    printf("policy %lu sign %08lx\n", policy, (unsigned long)hr);
    return hr;
}

__declspec(dllexport) int SignMsix(void) {
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"My");
    if (!store) {
        printf("open store %08lx\n", (unsigned long)HRESULT_FROM_WIN32(GetLastError()));
        return 1;
    }
    PCCERT_CONTEXT cert = CertFindCertificateInStore(
        store, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, L"CursorOpenProject", NULL);
    if (!cert) {
        printf("cert not found %08lx\n", (unsigned long)HRESULT_FROM_WIN32(GetLastError()));
        return 1;
    }

    const wchar_t *path = L"pkg\\Cursor.OpenProject.msix";
    HRESULT hr = SignOnce(cert, path, SIGNER_CERT_POLICY_CHAIN_NO_ROOT);
    if (FAILED(hr)) hr = SignOnce(cert, path, SIGNER_CERT_POLICY_CHAIN);
    CertFreeCertificateContext(cert);
    CertCloseStore(store, 0);
    return FAILED(hr) ? 1 : 0;
}
