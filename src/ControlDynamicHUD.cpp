// Control Dynamic HUD - V0.2 HUD lifecycle probe
// Freestanding x64 DLL for Control Plugin Loader. No visual HUD changes.

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using s32 = int;
using s64 = long long;
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
static constexpr DWORD FILE_SHARE_READ = 0x1u;
static constexpr DWORD FILE_SHARE_WRITE = 0x2u;
static constexpr DWORD CREATE_ALWAYS = 2u;
static constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x80u;
static constexpr DWORD PAGE_EXECUTE_READWRITE = 0x40u;
static constexpr DWORD MEM_COMMIT = 0x1000u;
static constexpr DWORD MEM_RESERVE = 0x2000u;
static constexpr uptr INVALID_HANDLE_VALUE_U = ~uptr(0);

struct UNICODE_STRING_T { u16 Length; u16 MaximumLength; wchar_t* Buffer; };
struct LIST_ENTRY_T { LIST_ENTRY_T* Flink; LIST_ENTRY_T* Blink; };

using FnCreateFileW = HANDLE (WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
using FnWriteFile = BOOL (WINAPI*)(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
using FnCloseHandle = BOOL (WINAPI*)(HANDLE);
using FnVirtualAlloc = LPVOID (WINAPI*)(LPVOID,usize,DWORD,DWORD);
using FnVirtualProtect = BOOL (WINAPI*)(LPVOID,usize,DWORD,DWORD*);
using FnFlushInstructionCache = BOOL (WINAPI*)(HANDLE,LPCVOID,usize);

typedef void* (WINAPI *FactoryFn)(void*, void*, void*, void*);
typedef void* (WINAPI *PageGetViewFn)(void*);
typedef void (WINAPI *ExecuteScriptFn)(void*, const char*, const char*);
typedef void (WINAPI *ReadyFn)(void*);

static HANDLE g_log = nullptr;
static FnWriteFile g_WriteFile = nullptr;
static FactoryFn g_originalFactory = nullptr;
static PageGetViewFn g_pageGetView = nullptr;
static ExecuteScriptFn g_executeScript = nullptr;
static ReadyFn g_originalReady = nullptr;
static uptr* g_readyEntry = nullptr;
static FnVirtualProtect g_VirtualProtect = nullptr;
static FnFlushInstructionCache g_FlushInstructionCache = nullptr;
static volatile u32 g_factoryCalls = 0;
static volatile u32 g_readyCalls = 0;

static constexpr u32 kCoherentTimeDateStamp = 0x5E8F6E9A;
static constexpr u32 kCoherentSizeOfImage = 0x329000;
static constexpr u32 kCoherentCheckSum = 0x00323A4A;
static constexpr uptr kExecuteScriptRva = 0xDBD00;
static constexpr uptr kViewPageOffset = 0xA8;

static const char kHealthScript[] = "window.__ControlDynamicHUDExecuteScriptProbe=1;";

static inline void* get_peb() { void* peb; __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(peb)); return peb; }
static u16 rd16(const void* p){ return *(const volatile u16*)p; }
static u32 rd32(const void* p){ return *(const volatile u32*)p; }
static uptr rdptr(const void* p){ return *(const volatile uptr*)p; }
static char lower_ascii(char c){ return (c>='A'&&c<='Z')?char(c+32):c; }
static wchar_t lower_w(wchar_t c){ return (c>=L'A'&&c<=L'Z')?wchar_t(c+32):c; }
static bool streq_ascii(const char* a,const char* b){ while(*a&&*b){if(*a!=*b)return false;++a;++b;}return *a==*b; }

static bool module_name_eq(const UNICODE_STRING_T* us,const char* ascii){
    if(!us||!us->Buffer)return false; usize n=us->Length/2,m=0; while(ascii[m])++m; if(n!=m)return false;
    for(usize i=0;i<n;++i) if(lower_w(us->Buffer[i])!=(wchar_t)(unsigned char)lower_ascii(ascii[i])) return false; return true;
}
static void* find_module(const char* baseName){
    u8* peb=(u8*)get_peb(); if(!peb)return nullptr; u8* ldr=(u8*)rdptr(peb+0x18); if(!ldr)return nullptr;
    LIST_ENTRY_T* head=(LIST_ENTRY_T*)(ldr+0x20); for(LIST_ENTRY_T* e=head->Flink;e&&e!=head;e=e->Flink){
        u8* entry=(u8*)e-0x10; auto* bn=(UNICODE_STRING_T*)(entry+0x58); if(module_name_eq(bn,baseName)) return (void*)rdptr(entry+0x30);
    } return nullptr;
}
static void* first_module_base(){
    u8* peb=(u8*)get_peb(); if(!peb)return nullptr; u8* ldr=(u8*)rdptr(peb+0x18); if(!ldr)return nullptr;
    auto* head=(LIST_ENTRY_T*)(ldr+0x20); auto* e=head->Flink; if(!e||e==head)return nullptr; return (void*)rdptr((u8*)e-0x10+0x30);
}
static const UNICODE_STRING_T* first_module_name(){
    u8* peb=(u8*)get_peb(); if(!peb)return nullptr; u8* ldr=(u8*)rdptr(peb+0x18); if(!ldr)return nullptr;
    auto* head=(LIST_ENTRY_T*)(ldr+0x20); auto* e=head->Flink; if(!e||e==head)return nullptr; return (const UNICODE_STRING_T*)((u8*)e-0x10+0x58);
}

static void* resolve_export(void* module,const char* name,int depth=0);
static void* resolve_forwarder(const char* fwd,int depth){
    if(depth>4)return nullptr; char modn[64],sym[128]; usize i=0;
    while(fwd[i]&&fwd[i]!='.'&&i<55){modn[i]=lower_ascii(fwd[i]);++i;} if(fwd[i]!='.')return nullptr; modn[i++]=0;
    usize ml=0; while(modn[ml])++ml; if(ml<4||!(modn[ml-4]=='.'&&modn[ml-3]=='d'&&modn[ml-2]=='l'&&modn[ml-1]=='l')){modn[ml++]='.';modn[ml++]='d';modn[ml++]='l';modn[ml++]='l';modn[ml]=0;}
    usize j=0; while(fwd[i]&&j<127)sym[j++]=fwd[i++]; sym[j]=0; if(sym[0]=='#')return nullptr; void* m=find_module(modn); return m?resolve_export(m,sym,depth+1):nullptr;
}
static void* resolve_export(void* module,const char* name,int depth){
    if(!module||depth>4)return nullptr; u8* base=(u8*)module; if(rd16(base)!=0x5A4D)return nullptr; u32 e=rd32(base+0x3C); u8* nt=base+e; if(rd32(nt)!=0x4550)return nullptr;
    u8* opt=nt+24; if(rd16(opt)!=0x20B)return nullptr; u32 er=rd32(opt+0x70),es=rd32(opt+0x74); if(!er||!es)return nullptr; u8* ex=base+er;
    u32 nn=rd32(ex+0x18),fr=rd32(ex+0x1C),nr=rd32(ex+0x20),orr=rd32(ex+0x24); u32* funcs=(u32*)(base+fr);u32* names=(u32*)(base+nr);u16* ords=(u16*)(base+orr);
    for(u32 i=0;i<nn;++i){const char* n=(const char*)(base+names[i]); if(!streq_ascii(n,name))continue; u32 r=funcs[ords[i]]; if(r>=er&&r<er+es)return resolve_forwarder((const char*)(base+r),depth); return base+r;} return nullptr;
}

struct WinApi { FnCreateFileW CreateFileW; FnWriteFile WriteFile; FnCloseHandle CloseHandle; FnVirtualAlloc VirtualAlloc; FnVirtualProtect VirtualProtect; FnFlushInstructionCache FlushInstructionCache; };
static bool init_api(WinApi& a){
    void* k=find_module("kernel32.dll"); if(!k)return false;
    a.CreateFileW=(FnCreateFileW)resolve_export(k,"CreateFileW"); a.WriteFile=(FnWriteFile)resolve_export(k,"WriteFile"); a.CloseHandle=(FnCloseHandle)resolve_export(k,"CloseHandle");
    a.VirtualAlloc=(FnVirtualAlloc)resolve_export(k,"VirtualAlloc"); a.VirtualProtect=(FnVirtualProtect)resolve_export(k,"VirtualProtect"); a.FlushInstructionCache=(FnFlushInstructionCache)resolve_export(k,"FlushInstructionCache");
    return a.CreateFileW&&a.WriteFile&&a.CloseHandle&&a.VirtualAlloc&&a.VirtualProtect&&a.FlushInstructionCache;
}

static void log_bytes(const char* s,DWORD n){ if(!g_log||!g_WriteFile)return; DWORD w=0; g_WriteFile(g_log,s,n,&w,nullptr); }
static void log_cstr(const char* s){ DWORD n=0;while(s[n])++n;log_bytes(s,n); }
static void log_line(const char* s){log_cstr(s);log_bytes("\r\n",2);}
static void log_hex(uptr v){char b[18];b[0]='0';b[1]='x';const char*h="0123456789ABCDEF";for(int i=0;i<16;++i)b[2+i]=h[(v>>((15-i)*4))&15];log_bytes(b,18);}
static void log_dec(u64 v){char b[32];int p=31;b[p--]=0;if(!v)b[p--]='0';while(v&&p>=0){b[p--]=char('0'+v%10);v/=10;}log_cstr(b+p+1);}
static void log_wide(const UNICODE_STRING_T* us){if(!us||!us->Buffer){log_cstr("<unknown>");return;}char b[260];usize n=us->Length/2,o=0;for(usize i=0;i<n&&o<259;++i){wchar_t c=us->Buffer[i];b[o++]=(c>=32&&c<127)?char(c):'?';}b[o]=0;log_cstr(b);}

static bool coherent_build_matches(void* module){
    if(!module)return false;
    u8* base=(u8*)module;
    if(rd16(base)!=0x5A4D)return false;
    u32 e=rd32(base+0x3C);u8* nt=base+e;
    if(rd32(nt)!=0x4550)return false;
    u32 timestamp=rd32(nt+8);
    u8* opt=nt+24;
    if(rd16(opt)!=0x20B)return false;
    u32 imageSize=rd32(opt+0x38);
    u32 checksum=rd32(opt+0x40);
    log_cstr("Coherent fingerprint: TimeDateStamp=");log_hex(timestamp);
    log_cstr(" SizeOfImage=");log_hex(imageSize);
    log_cstr(" CheckSum=");log_hex(checksum);log_bytes("\r\n",2);
    return timestamp==kCoherentTimeDateStamp&&imageSize==kCoherentSizeOfImage&&checksum==kCoherentCheckSum;
}

static bool get_text(void* module,u8*& text,u32& size){
    u8* b=(u8*)module;if(!b||rd16(b)!=0x5A4D)return false;u32 e=rd32(b+0x3C);u8* nt=b+e;if(rd32(nt)!=0x4550)return false;u16 ns=rd16(nt+6),os=rd16(nt+20);u8* sec=nt+24+os;
    for(u16 i=0;i<ns;++i,sec+=40){ if(sec[0]=='.'&&sec[1]=='t'&&sec[2]=='e'&&sec[3]=='x'&&sec[4]=='t'){u32 vs=rd32(sec+8),va=rd32(sec+12);text=b+va;size=vs;return true;} } return false;
}

static const u8 kFactoryPat[] = {
0x48,0x89,0x4C,0x24,0x08,0x53,0x48,0x83,0xEC,0x30,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,
0x49,0x8B,0xD8,0xBA,0x08,0x00,0x00,0x00,0xB9,0x10,0x03,0x00,0x00,0xFF,0x15,0,0,0,0,
0x48,0x89,0x44,0x24,0x40,0x48,0x8B,0x13,0x48,0x8B,0xC8,0xE8,0,0,0,0,0x90,0x48,0x83,0xC4,0x30,0x5B,0xC3};
static const char kFactoryMask[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx????xxxxxxx";

static uptr scan_factory(void* exe,u32& count){
    count=0;u8* t=nullptr;u32 sz=0;if(!get_text(exe,t,sz))return 0;usize plen=sizeof(kFactoryPat);uptr first=0;
    for(u32 i=0;i+plen<=sz;++i){bool ok=true;for(usize j=0;j<plen;++j){if(kFactoryMask[j]=='x'&&t[i+j]!=kFactoryPat[j]){ok=false;break;}}if(ok){if(!first)first=(uptr)(t+i);++count;i+=(u32)plen-1;}}
    return first;
}

static void write_abs_jump(u8* at,void* dst){ at[0]=0xFF;at[1]=0x25;at[2]=0;at[3]=0;at[4]=0;at[5]=0;*(uptr*)(at+6)=(uptr)dst; }

static void restore_ready_hook(){
    if(!g_readyEntry||!g_originalReady||!g_VirtualProtect)return;
    DWORD old=0;
    if(g_VirtualProtect((void*)g_readyEntry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        *g_readyEntry=(uptr)g_originalReady;
        DWORD dummy=0;
        g_VirtualProtect((void*)g_readyEntry,sizeof(uptr),old,&dummy);
        if(g_FlushInstructionCache) g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)g_readyEntry,sizeof(uptr));
        log_line("Ready vtable hook restored.");
    }
    g_readyEntry=nullptr;
}

static void WINAPI HookReady(void* self){
    u32 n=++g_readyCalls;
    if(g_originalReady) g_originalReady(self);

    void* view=(self&&g_pageGetView)?g_pageGetView(self):nullptr;
    uptr page=view?rdptr((u8*)view+kViewPageOffset):0;

    if(n<=4 || (view&&page)){
        log_cstr("HUD ready callback #");log_dec(n);
        log_cstr(" View*=");log_hex((uptr)view);
        log_cstr(" ViewPage*=");log_hex(page);
        log_bytes("\r\n",2);
    }

    if(view&&page&&g_executeScript){
        g_executeScript(view,kHealthScript,nullptr);
        log_line("Minimal ExecuteScript probe injected.");
        restore_ready_hook();
    } else if(n>=64){
        log_line("Ready probe cap reached without an injectable View; restoring original vtable.");
        restore_ready_hook();
    }
}

static bool install_ready_hook(void* hud){
    if(!hud||!g_VirtualProtect)return false;
    uptr vtbl=rdptr(hud);
    if(!vtbl)return false;
    uptr* entry=(uptr*)(vtbl + 27u*sizeof(uptr));
    uptr original=*entry;
    if(!original)return false;

    g_originalReady=(ReadyFn)original;
    g_readyEntry=entry;

    log_cstr("HUD ready vtable entry[27]: ");log_hex((uptr)entry);
    log_cstr(" original=");log_hex(original);log_bytes("\r\n",2);

    DWORD old=0;
    if(!g_VirtualProtect((void*)entry,sizeof(uptr),PAGE_EXECUTE_READWRITE,&old)){
        g_readyEntry=nullptr; g_originalReady=nullptr; return false;
    }
    *entry=(uptr)&HookReady;
    DWORD dummy=0;
    g_VirtualProtect((void*)entry,sizeof(uptr),old,&dummy);
    if(g_FlushInstructionCache) g_FlushInstructionCache((HANDLE)(uptr)-1,(void*)entry,sizeof(uptr));
    log_line("One-shot ready vtable hook installed.");
    return true;
}

static void* WINAPI HookFactory(void* a,void* b,void* c,void* d){
    u32 n=++g_factoryCalls;
    log_cstr("HUD factory call #");log_dec(n);log_cstr(" arg3=");log_hex((uptr)c);log_bytes("\r\n",2);
    void* r=g_originalFactory?g_originalFactory(a,b,c,d):nullptr;
    log_cstr("HUD object returned: ");log_hex((uptr)r);log_bytes("\r\n",2);
    if(r){
        uptr vtbl=rdptr((u8*)r+0x00);
        void* system=(void*)rdptr((u8*)r+0x10);
        u32 pageIndex=rd32((u8*)r+0x18);
        log_cstr("HUD vtable: ");log_hex(vtbl);log_bytes("\r\n",2);
        log_cstr("Initial HUD System*: ");log_hex((uptr)system);log_cstr(" pageIndex=");log_dec(pageIndex);log_bytes("\r\n",2);
        if(!g_readyEntry && !install_ready_hook(r)) log_line("FAIL-OPEN: could not install one-shot ready hook.");
    }
    return r;
}

static bool install_hook(WinApi& api,uptr target){
    constexpr usize stolen=19;
    u8* tramp=(u8*)api.VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tramp)return false;
    for(usize i=0;i<stolen;++i)tramp[i]=((u8*)target)[i];
    write_abs_jump(tramp+stolen,(void*)(target+stolen));
    g_originalFactory=(FactoryFn)tramp;
    DWORD old=0;if(!api.VirtualProtect((void*)target,stolen,PAGE_EXECUTE_READWRITE,&old))return false;
    write_abs_jump((u8*)target,(void*)&HookFactory);for(usize i=14;i<stolen;++i)((u8*)target)[i]=0x90;
    DWORD dummy=0;api.VirtualProtect((void*)target,stolen,old,&dummy);api.FlushInstructionCache((HANDLE)(uptr)-1,(void*)target,stolen);return true;
}

extern "C" __declspec(dllexport) int WINAPI CDH_Version(){return 51;}
extern "C" __declspec(dllexport) void* CDH_RelocAnchor=(void*)&CDH_Version;

extern "C" BOOL WINAPI DllMain(HMODULE,DWORD reason,LPVOID){
    if(reason!=DLL_PROCESS_ATTACH)return 1; WinApi api{};if(!init_api(api))return 1;
    g_VirtualProtect=api.VirtualProtect;
    g_FlushInstructionCache=api.FlushInstructionCache;
    g_WriteFile=api.WriteFile;g_log=api.CreateFileW(L"plugins\\ControlDynamicHUD.log",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!g_log||(uptr)g_log==INVALID_HANDLE_VALUE_U)return 1;
    log_line("Control Dynamic HUD V0.5A EXECUTESCRIPT PROBE");log_line("Mode: native one-shot bridge + minimal ExecuteScript probe only");
    log_cstr("Executable: ");log_wide(first_module_name());log_bytes("\r\n",2);void* exe=first_module_base();log_cstr("Executable base: ");log_hex((uptr)exe);log_bytes("\r\n",2);
    void* coh=find_module("coherentuigt.dll");log_cstr("CoherentUIGT.dll: ");if(coh){log_hex((uptr)coh);log_cstr(" (loaded)");}else log_cstr("NOT FOUND");log_bytes("\r\n",2);
    void* ui=find_module("ui_rmdwin10_f.dll");log_cstr("ui_rmdwin10_f.dll: ");if(ui){log_hex((uptr)ui);log_cstr(" (loaded)");}else log_cstr("NOT FOUND");log_bytes("\r\n",2);
    const char* pageGetViewName="?getView@Page@ui@@QEAAPEAVView@UIGT@Coherent@@XZ";
    g_pageGetView=ui?(PageGetViewFn)resolve_export(ui,pageGetViewName):nullptr;
    log_cstr("ui::Page::getView export: ");if(g_pageGetView)log_hex((uptr)g_pageGetView);else log_cstr("NOT FOUND");log_bytes("\r\n",2);
    if(coherent_build_matches(coh)){
        g_executeScript=(ExecuteScriptFn)((u8*)coh+kExecuteScriptRva);
        log_cstr("Coherent View::ExecuteScript: ");log_hex((uptr)g_executeScript);log_bytes("\r\n",2);
    }else{
        log_line("FAIL-OPEN: unsupported CoherentUIGT build; script injection disabled.");
    }
    u32 count=0;uptr factory=scan_factory(exe,count);log_cstr("HUD factory signature matches: ");log_dec(count);log_cstr(" first=");log_hex(factory);log_bytes("\r\n",2);
    if(count!=1||!factory){log_line("FAIL-OPEN: unique HUD factory not found; no hook installed.");return 1;}
    if(!g_pageGetView||!g_executeScript){log_line("FAIL-OPEN: native UI bridge unavailable; no hook installed.");return 1;}
    s32 rel=*(s32*)(factory+0x32);uptr ctor=factory+0x36+(s64)rel;
    log_cstr("HUD constructor target: ");log_hex(ctor);log_bytes("\r\n",2);
    if(!install_hook(api,factory)){log_line("FAIL-OPEN: hook installation failed.");return 1;}
    log_line("HUD factory hook installed.");return 1;
}