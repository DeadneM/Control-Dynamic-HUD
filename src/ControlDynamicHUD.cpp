// Control Dynamic HUD - V0.1 Probe
// Freestanding x64 Windows DLL. No CRT, no external dependencies.
// Loaded by reg2k's Control Plugin Loader from plugins\\*.dll.

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using uptr = unsigned long long;
using usize = unsigned long long;
using BOOL = int;
using DWORD = unsigned long;
using HANDLE = void*;
using HMODULE = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPCWSTR = const wchar_t*;

#define WINAPI __attribute__((ms_abi))
#define DLL_PROCESS_ATTACH 1

static constexpr DWORD GENERIC_WRITE = 0x40000000u;
static constexpr DWORD FILE_SHARE_READ = 0x00000001u;
static constexpr DWORD FILE_SHARE_WRITE = 0x00000002u;
static constexpr DWORD CREATE_ALWAYS = 2u;
static constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x00000080u;
static constexpr uptr INVALID_HANDLE_VALUE_U = ~uptr(0);

struct UNICODE_STRING_T {
    u16 Length;
    u16 MaximumLength;
    wchar_t* Buffer;
};

struct LIST_ENTRY_T {
    LIST_ENTRY_T* Flink;
    LIST_ENTRY_T* Blink;
};

using FnCreateFileW = HANDLE (WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE);
using FnWriteFile   = BOOL   (WINAPI*)(HANDLE, LPCVOID, DWORD, DWORD*, LPVOID);
using FnCloseHandle = BOOL   (WINAPI*)(HANDLE);

static inline void* get_peb() {
    void* peb;
    __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(peb));
    return peb;
}

static u16 rd16(const void* p) { return *(const volatile u16*)p; }
static u32 rd32(const void* p) { return *(const volatile u32*)p; }
static uptr rdptr(const void* p) { return *(const volatile uptr*)p; }

static char lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') return char(c + ('a' - 'A'));
    return c;
}
static wchar_t lower_w(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return wchar_t(c + (L'a' - L'A'));
    return c;
}
static bool streq_ascii(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
    return *a == *b;
}
static bool module_name_eq(const UNICODE_STRING_T* us, const char* ascii) {
    if (!us || !us->Buffer) return false;
    usize n = us->Length / sizeof(wchar_t);
    usize m = 0; while (ascii[m]) ++m;
    if (n != m) return false;
    for (usize i = 0; i < n; ++i) {
        wchar_t wc = lower_w(us->Buffer[i]);
        char ac = lower_ascii(ascii[i]);
        if (wc != (wchar_t)(unsigned char)ac) return false;
    }
    return true;
}

static void* find_module(const char* baseName) {
    u8* peb = (u8*)get_peb();
    if (!peb) return nullptr;
    u8* ldr = (u8*)rdptr(peb + 0x18);
    if (!ldr) return nullptr;
    LIST_ENTRY_T* head = (LIST_ENTRY_T*)(ldr + 0x20);
    for (LIST_ENTRY_T* e = head->Flink; e && e != head; e = e->Flink) {
        u8* entry = (u8*)e - 0x10;
        UNICODE_STRING_T* bn = (UNICODE_STRING_T*)(entry + 0x58);
        if (module_name_eq(bn, baseName)) return (void*)rdptr(entry + 0x30);
    }
    return nullptr;
}

static void* first_module_base() {
    u8* peb = (u8*)get_peb();
    if (!peb) return nullptr;
    u8* ldr = (u8*)rdptr(peb + 0x18);
    if (!ldr) return nullptr;
    LIST_ENTRY_T* head = (LIST_ENTRY_T*)(ldr + 0x20);
    LIST_ENTRY_T* e = head->Flink;
    if (!e || e == head) return nullptr;
    u8* entry = (u8*)e - 0x10;
    return (void*)rdptr(entry + 0x30);
}

static const UNICODE_STRING_T* first_module_name() {
    u8* peb = (u8*)get_peb();
    if (!peb) return nullptr;
    u8* ldr = (u8*)rdptr(peb + 0x18);
    if (!ldr) return nullptr;
    LIST_ENTRY_T* head = (LIST_ENTRY_T*)(ldr + 0x20);
    LIST_ENTRY_T* e = head->Flink;
    if (!e || e == head) return nullptr;
    u8* entry = (u8*)e - 0x10;
    return (const UNICODE_STRING_T*)(entry + 0x58);
}

static void* resolve_export(void* module, const char* name, int depth = 0);

static void* resolve_forwarder(const char* fwd, int depth) {
    if (depth > 4) return nullptr;
    char moduleName[64];
    char symbol[128];
    usize i = 0;
    while (fwd[i] && fwd[i] != '.' && i < 55) { moduleName[i] = lower_ascii(fwd[i]); ++i; }
    if (fwd[i] != '.') return nullptr;
    moduleName[i++] = 0;
    usize ml = 0; while (moduleName[ml]) ++ml;
    if (ml < 4 || !(moduleName[ml-4]=='.' && moduleName[ml-3]=='d' && moduleName[ml-2]=='l' && moduleName[ml-1]=='l')) {
        moduleName[ml++]='.'; moduleName[ml++]='d'; moduleName[ml++]='l'; moduleName[ml++]='l'; moduleName[ml]=0;
    }
    usize j = 0;
    while (fwd[i] && j < sizeof(symbol)-1) symbol[j++] = fwd[i++];
    symbol[j] = 0;
    if (symbol[0] == '#') return nullptr;
    void* mod = find_module(moduleName);
    if (!mod) return nullptr;
    return resolve_export(mod, symbol, depth + 1);
}

static void* resolve_export(void* module, const char* name, int depth) {
    if (!module || depth > 4) return nullptr;
    u8* base = (u8*)module;
    if (rd16(base) != 0x5A4D) return nullptr;
    u32 lfanew = rd32(base + 0x3C);
    u8* nt = base + lfanew;
    if (rd32(nt) != 0x00004550) return nullptr;
    u8* opt = nt + 24;
    if (rd16(opt) != 0x20B) return nullptr;
    u32 exportRva = rd32(opt + 0x70);
    u32 exportSize = rd32(opt + 0x74);
    if (!exportRva || !exportSize) return nullptr;
    u8* exp = base + exportRva;
    u32 numberOfNames = rd32(exp + 0x18);
    u32 funcsRva = rd32(exp + 0x1C);
    u32 namesRva = rd32(exp + 0x20);
    u32 ordsRva = rd32(exp + 0x24);
    u32* funcs = (u32*)(base + funcsRva);
    u32* names = (u32*)(base + namesRva);
    u16* ords = (u16*)(base + ordsRva);
    for (u32 i = 0; i < numberOfNames; ++i) {
        const char* n = (const char*)(base + names[i]);
        if (!streq_ascii(n, name)) continue;
        u32 fnRva = funcs[ords[i]];
        if (fnRva >= exportRva && fnRva < exportRva + exportSize) {
            return resolve_forwarder((const char*)(base + fnRva), depth);
        }
        return base + fnRva;
    }
    return nullptr;
}

struct WinApi {
    FnCreateFileW CreateFileW;
    FnWriteFile WriteFile;
    FnCloseHandle CloseHandle;
};

static bool init_api(WinApi& api) {
    void* k32 = find_module("kernel32.dll");
    if (!k32) return false;
    api.CreateFileW = (FnCreateFileW)resolve_export(k32, "CreateFileW");
    api.WriteFile = (FnWriteFile)resolve_export(k32, "WriteFile");
    api.CloseHandle = (FnCloseHandle)resolve_export(k32, "CloseHandle");
    return api.CreateFileW && api.WriteFile && api.CloseHandle;
}

static void write_bytes(WinApi& api, HANDLE h, const char* s, DWORD n) {
    DWORD written = 0;
    api.WriteFile(h, s, n, &written, nullptr);
}
static void write_cstr(WinApi& api, HANDLE h, const char* s) {
    DWORD n = 0; while (s[n]) ++n;
    write_bytes(api, h, s, n);
}
static void write_line(WinApi& api, HANDLE h, const char* s) {
    write_cstr(api, h, s); write_bytes(api, h, "\r\n", 2);
}
static void write_hex64(WinApi& api, HANDLE h, uptr v) {
    char b[18]; b[0]='0'; b[1]='x';
    const char* hex="0123456789ABCDEF";
    for (int i=0;i<16;++i) b[2+i]=hex[(v >> ((15-i)*4)) & 0xF];
    write_bytes(api,h,b,18);
}
static void write_dec(WinApi& api, HANDLE h, u64 v) {
    char b[32]; int p=31; b[p--]=0;
    if (v==0) b[p--]='0';
    while (v && p>=0) { b[p--]=char('0'+(v%10)); v/=10; }
    write_cstr(api,h,b+p+1);
}
static void write_wide_lossy(WinApi& api, HANDLE h, const UNICODE_STRING_T* us) {
    if (!us || !us->Buffer) { write_cstr(api,h,"<unknown>"); return; }
    usize n=us->Length/2;
    char b[260]; usize out=0;
    for (usize i=0;i<n && out<sizeof(b)-1;++i) {
        wchar_t c=us->Buffer[i]; b[out++]=(c>=32 && c<127)?char(c):'?';
    }
    b[out]=0; write_cstr(api,h,b);
}

struct ScanResult { uptr first; u32 count; };

static ScanResult scan_ascii_in_module(void* module, const char* needle) {
    ScanResult r{0,0};
    if (!module || !needle || !*needle) return r;
    u8* base=(u8*)module;
    if (rd16(base)!=0x5A4D) return r;
    u32 lfanew=rd32(base+0x3C); u8* nt=base+lfanew;
    if (rd32(nt)!=0x00004550) return r;
    u16 sections=rd16(nt+6); u16 optSize=rd16(nt+20);
    u8* sec=nt+24+optSize;
    usize nl=0; while (needle[nl]) ++nl;
    for (u16 si=0; si<sections; ++si, sec+=40) {
        u32 virtualSize=rd32(sec+8); u32 virtualAddress=rd32(sec+12); u32 characteristics=rd32(sec+36);
        if (!(characteristics & 0x40000000u) || virtualSize < nl) continue;
        u8* p=base+virtualAddress;
        for (u32 i=0; i+nl<=virtualSize; ++i) {
            bool ok=true;
            for (usize j=0;j<nl;++j) if (p[i+j]!=(u8)needle[j]) {ok=false;break;}
            if (ok) { if (!r.first) r.first=(uptr)(p+i); ++r.count; i += (u32)(nl?nl-1:0); }
        }
    }
    return r;
}

extern "C" __declspec(dllexport) int WINAPI CDH_Version() { return 1; }
extern "C" __declspec(dllexport) void* CDH_RelocAnchor = (void*)&CDH_Version;

extern "C" BOOL WINAPI DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return 1;

    WinApi api{};
    if (!init_api(api)) return 1;

    HANDLE h = api.CreateFileW(L"plugins\\ControlDynamicHUD.log", GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if ((uptr)h == INVALID_HANDLE_VALUE_U || !h) return 1;

    write_line(api,h,"Control Dynamic HUD V0.1 PROBE");
    write_line(api,h,"Mode: diagnostic only (no hooks, no patches, no memory writes)");

    write_cstr(api,h,"Executable: "); write_wide_lossy(api,h,first_module_name()); write_bytes(api,h,"\r\n",2);
    void* exe=first_module_base();
    write_cstr(api,h,"Executable base: "); write_hex64(api,h,(uptr)exe); write_bytes(api,h,"\r\n",2);

    void* coherent=find_module("coherentuigt.dll");
    write_cstr(api,h,"CoherentUIGT.dll: ");
    if (coherent) { write_hex64(api,h,(uptr)coherent); write_cstr(api,h," (loaded)"); }
    else write_cstr(api,h,"NOT FOUND");
    write_bytes(api,h,"\r\n",2);

    ScanResult hud=scan_ascii_in_module(exe,"uiresources\\p7\\hud.ui");
    write_cstr(api,h,"hud.ui string matches: "); write_dec(api,h,hud.count);
    write_cstr(api,h," first="); write_hex64(api,h,hud.first); write_bytes(api,h,"\r\n",2);

    ScanResult vis=scan_ascii_in_module(exe,"m_bIsHudVisible");
    write_cstr(api,h,"m_bIsHudVisible matches: "); write_dec(api,h,vis.count);
    write_cstr(api,h," first="); write_hex64(api,h,vis.first); write_bytes(api,h,"\r\n",2);

    const char* getViewName="?getView@Page@ui@@QEAAPEAVView@UIGT@Coherent@@XZ";
    void* getView = coherent ? resolve_export(coherent, getViewName) : nullptr;
    write_cstr(api,h,"Coherent Page::getView export: ");
    if (getView) write_hex64(api,h,(uptr)getView); else write_cstr(api,h,"NOT FOUND");
    write_bytes(api,h,"\r\n",2);

    write_line(api,h,"Probe complete.");
    api.CloseHandle(h);
    return 1;
}
