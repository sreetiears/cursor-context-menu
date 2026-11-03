using System;
using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;

public static class MenuPack {
    const uint STGM_READ = 0;
    const uint STGM_READWRITE = 2;
    const uint STGM_SHARE_EXCLUSIVE = 0x10;
    const uint STGM_SHARE_DENY_WRITE = 0x20;
    const uint STGM_CREATE = 0x1000;

    [DllImport("ole32.dll")]
    static extern int CoInitializeEx(IntPtr reserved, uint coinit);

    [DllImport("ole32.dll")]
    static extern int CoCreateInstance(ref Guid clsid, IntPtr outer, uint clsctx, ref Guid iid, out IntPtr obj);

    [DllImport("urlmon.dll", CharSet = CharSet.Unicode)]
    static extern int CreateUri(string uri, uint flags, IntPtr reserved, out IntPtr uriOut);

    [DllImport("shlwapi.dll", CharSet = CharSet.Unicode)]
    static extern int SHCreateStreamOnFileEx(string path, uint mode, uint attrs, bool create, IntPtr template, out IntPtr stream);

    [DllImport("MSSign32.dll", CharSet = CharSet.Unicode)]
    static extern int SignerSignEx2(
        uint dwFlags,
        IntPtr pSubjectInfo,
        IntPtr pSigningCert,
        IntPtr pSignatureInfo,
        IntPtr pProviderInfo,
        uint dwTimestampFlags,
        IntPtr pszAlgorithmOid,
        IntPtr pwszTimestampURL,
        IntPtr pCryptAttrs,
        IntPtr pSipData,
        IntPtr pSignerContext,
        IntPtr pCryptoPolicy,
        IntPtr pReserved);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int CreatePackageWriterFn(IntPtr self, IntPtr output, IntPtr settings, out IntPtr writer);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int AddPayloadFn(IntPtr self, IntPtr fileName, IntPtr contentType, int compression, IntPtr input);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int CloseFn(IntPtr self, IntPtr manifest);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate uint ReleaseFn(IntPtr self);

    [StructLayout(LayoutKind.Sequential)]
    struct PackageSettings {
        public int forceZip32;
        public IntPtr hashMethod;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct SignerFileInfo {
        public uint cbSize;
        public IntPtr pwszFileName;
        public IntPtr hFile;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct SignerSubjectInfo {
        public uint cbSize;
        public IntPtr pdwIndex;
        public uint dwSubjectChoice;
        public IntPtr pSignerFileInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct SignerCertStoreInfo {
        public uint cbSize;
        public IntPtr pSigningCert;
        public uint dwCertPolicy;
        public IntPtr hCertStore;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct SignerCert {
        public uint cbSize;
        public uint dwCertChoice;
        public IntPtr pCertStoreInfo;
        public IntPtr hwnd;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct SignerSignatureInfo {
        public uint cbSize;
        public uint algidHash;
        public uint dwAttrChoice;
        public IntPtr pAttrAuthcode;
        public IntPtr psAuthenticated;
        public IntPtr psUnauthenticated;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct SignerSignEx2Params {
        public uint dwFlags;
        public IntPtr pSubjectInfo;
        public IntPtr pSigningCert;
        public IntPtr pSignatureInfo;
        public IntPtr pProviderInfo;
        public uint dwTimestampFlags;
        public IntPtr pszAlgorithmOid;
        public IntPtr pwszTimestampURL;
        public IntPtr pCryptAttrs;
        public IntPtr pSipData;
        public IntPtr pSignerContext;
        public IntPtr pCryptoPolicy;
        public IntPtr pReserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct AppxSipClientData {
        public IntPtr pSignerParams;
        public IntPtr pAppxSipState;
    }

    static IntPtr Vtbl(IntPtr obj, int slot) {
        IntPtr vtbl = Marshal.ReadIntPtr(obj);
        return Marshal.ReadIntPtr(vtbl, slot * IntPtr.Size);
    }

    static void Release(IntPtr obj) {
        if (obj == IntPtr.Zero) return;
        var rel = (ReleaseFn)Marshal.GetDelegateForFunctionPointer(Vtbl(obj, 2), typeof(ReleaseFn));
        rel(obj);
    }

    static IntPtr Uni(string value) {
        return Marshal.StringToHGlobalUni(value);
    }

    public static int Pack() {
        int hr = CoInitializeEx(IntPtr.Zero, 2);
        Console.WriteLine("CoInit {0:X8}", hr);

        Guid clsid = new Guid("5842a140-ff9f-4166-8f5c-62f5b7b0c781");
        Guid iid = new Guid("beb94909-e451-438b-b5a7-d79e767b75d8");
        IntPtr factory;
        hr = CoCreateInstance(ref clsid, IntPtr.Zero, 1, ref iid, out factory);
        Console.WriteLine("Factory {0:X8}", hr);
        if (hr < 0) return hr;

        IntPtr hashUri;
        hr = CreateUri("http://www.w3.org/2001/04/xmlenc#sha256", 0x100, IntPtr.Zero, out hashUri);
        Console.WriteLine("CreateUri {0:X8}", hr);
        if (hr < 0) return hr;

        string root = System.IO.Directory.GetCurrentDirectory();
        System.IO.Directory.CreateDirectory(System.IO.Path.Combine(root, "pkg"));
        string msixPath = System.IO.Path.Combine(root, "pkg", "Cursor.OpenProject.msix");
        if (System.IO.File.Exists(msixPath)) System.IO.File.Delete(msixPath);

        IntPtr output;
        hr = SHCreateStreamOnFileEx(msixPath, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0x80, true, IntPtr.Zero, out output);
        Console.WriteLine("Output {0:X8}", hr);
        if (hr < 0) return hr;

        PackageSettings settings = new PackageSettings();
        settings.forceZip32 = 0;
        settings.hashMethod = hashUri;
        IntPtr settingsPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(PackageSettings)));
        Marshal.StructureToPtr(settings, settingsPtr, false);

        IntPtr writer;
        var create = (CreatePackageWriterFn)Marshal.GetDelegateForFunctionPointer(Vtbl(factory, 3), typeof(CreatePackageWriterFn));
        hr = create(factory, output, settingsPtr, out writer);
        Console.WriteLine("Writer {0:X8} size {1}", hr, Marshal.SizeOf(typeof(PackageSettings)));
        if (hr < 0) return hr;

        string[] names = {
            @"Assets\StoreLogo.png",
            @"Assets\Square44x44Logo.png",
            @"Assets\Square150x150Logo.png"
        };
        string[] disks = {
            System.IO.Path.Combine(root, "Assets", "StoreLogo.png"),
            System.IO.Path.Combine(root, "Assets", "Square44x44Logo.png"),
            System.IO.Path.Combine(root, "Assets", "Square150x150Logo.png")
        };
        var add = (AddPayloadFn)Marshal.GetDelegateForFunctionPointer(Vtbl(writer, 3), typeof(AddPayloadFn));
        for (int i = 0; i < names.Length; i++) {
            IntPtr stream;
            hr = SHCreateStreamOnFileEx(disks[i], STGM_READ | STGM_SHARE_DENY_WRITE, 0, false, IntPtr.Zero, out stream);
            if (hr < 0) {
                Console.WriteLine("open {0} {1:X8}", disks[i], hr);
                return hr;
            }
            IntPtr name = Uni(names[i]);
            IntPtr type = Uni("image/png");
            hr = add(writer, name, type, 1, stream);
            Marshal.FreeHGlobal(name);
            Marshal.FreeHGlobal(type);
            Release(stream);
            Console.WriteLine("add {0} {1:X8}", names[i], hr);
            if (hr < 0) return hr;
        }

        IntPtr manifest;
        hr = SHCreateStreamOnFileEx(System.IO.Path.Combine(root, "AppxManifest.xml"), STGM_READ | STGM_SHARE_DENY_WRITE, 0, false, IntPtr.Zero, out manifest);
        Console.WriteLine("Manifest {0:X8}", hr);
        if (hr < 0) return hr;
        var close = (CloseFn)Marshal.GetDelegateForFunctionPointer(Vtbl(writer, 4), typeof(CloseFn));
        hr = close(writer, manifest);
        Console.WriteLine("Close {0:X8}", hr);
        Release(manifest);
        Release(writer);
        Release(output);
        Release(factory);
        Release(hashUri);
        Marshal.FreeHGlobal(settingsPtr);
        return hr;
    }

    public static int Sign() {
        X509Store store = new X509Store(StoreName.My, StoreLocation.CurrentUser);
        store.Open(OpenFlags.ReadOnly);
        X509Certificate2 cert = null;
        foreach (X509Certificate2 item in store.Certificates) {
            if (item.Subject == "CN=CursorOpenProject") {
                cert = item;
                break;
            }
        }
        if (cert == null) {
            Console.WriteLine("cert missing");
            return 1;
        }
        Console.WriteLine("Using " + cert.Thumbprint);

        string path = System.IO.Path.Combine(System.IO.Directory.GetCurrentDirectory(), "pkg", "Cursor.OpenProject.msix");
        IntPtr pathPtr = Uni(path);
        IntPtr indexPtr = Marshal.AllocHGlobal(4);
        Marshal.WriteInt32(indexPtr, 0);

        SignerFileInfo fileInfo = new SignerFileInfo();
        fileInfo.cbSize = (uint)Marshal.SizeOf(typeof(SignerFileInfo));
        fileInfo.pwszFileName = pathPtr;
        IntPtr fileInfoPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SignerFileInfo)));
        Marshal.StructureToPtr(fileInfo, fileInfoPtr, false);

        SignerSubjectInfo subject = new SignerSubjectInfo();
        subject.cbSize = (uint)Marshal.SizeOf(typeof(SignerSubjectInfo));
        subject.pdwIndex = indexPtr;
        subject.dwSubjectChoice = 1;
        subject.pSignerFileInfo = fileInfoPtr;
        IntPtr subjectPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SignerSubjectInfo)));
        Marshal.StructureToPtr(subject, subjectPtr, false);

        SignerCertStoreInfo storeInfo = new SignerCertStoreInfo();
        storeInfo.cbSize = (uint)Marshal.SizeOf(typeof(SignerCertStoreInfo));
        storeInfo.pSigningCert = cert.Handle;
        storeInfo.dwCertPolicy = 8;
        IntPtr storeInfoPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SignerCertStoreInfo)));
        Marshal.StructureToPtr(storeInfo, storeInfoPtr, false);

        SignerCert signerCert = new SignerCert();
        signerCert.cbSize = (uint)Marshal.SizeOf(typeof(SignerCert));
        signerCert.dwCertChoice = 2;
        signerCert.pCertStoreInfo = storeInfoPtr;
        IntPtr signerCertPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SignerCert)));
        Marshal.StructureToPtr(signerCert, signerCertPtr, false);

        SignerSignatureInfo signature = new SignerSignatureInfo();
        signature.cbSize = (uint)Marshal.SizeOf(typeof(SignerSignatureInfo));
        signature.algidHash = 0x800c; // CALG_SHA_256
        signature.dwAttrChoice = 0;
        IntPtr signaturePtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SignerSignatureInfo)));
        Marshal.StructureToPtr(signature, signaturePtr, false);

        SignerSignEx2Params signerParams = new SignerSignEx2Params();
        signerParams.pSubjectInfo = subjectPtr;
        signerParams.pSigningCert = signerCertPtr;
        signerParams.pSignatureInfo = signaturePtr;
        IntPtr signerParamsPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SignerSignEx2Params)));
        Marshal.StructureToPtr(signerParams, signerParamsPtr, false);

        AppxSipClientData sip = new AppxSipClientData();
        sip.pSignerParams = signerParamsPtr;
        IntPtr sipPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(AppxSipClientData)));
        Marshal.StructureToPtr(sip, sipPtr, false);

        signerParams.pSipData = sipPtr;
        Marshal.StructureToPtr(signerParams, signerParamsPtr, false);

        int hr = SignerSignEx2(0, subjectPtr, signerCertPtr, signaturePtr, IntPtr.Zero, 0, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, sipPtr, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        Console.WriteLine("Sign {0:X8}", hr);

        sip = (AppxSipClientData)Marshal.PtrToStructure(sipPtr, typeof(AppxSipClientData));
        Release(sip.pAppxSipState);
        store.Close();
        return hr;
    }
}
