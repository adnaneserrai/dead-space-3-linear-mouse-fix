// Dead Space 3 Linear Mouse Fix v1.0
// Exact target: x86 deadspace3.exe timestamp 0x511E9327, image size 0x012D4000/0x012D6000.
//
// Final lightweight build derived from the validated v0.8.2 HIP + ADS experiment.
// No binary logger, no Raw Input monitoring, no WndProc work, no DPI/sensitivity
// compensation. The functional algorithm is unchanged:
//   1) identify the DirectInput system mouse and publish same-thread GetDeviceState X/Y;
//   2) on only the two verified mouse-look ABC510 paths (HIP/ADS), call AB9830 vanilla;
//      if a fresh non-zero mouse sample exists and pre magnitude > 1, restore pre X/Y;
//   3) on only the matching D820 caller, call D820 vanilla first, reconstruct the exact
//      validated post-state model, and while the same-profile mouse-tail latch is active,
//      replace only a truly clamped output with preclamp * scale.
//
// Controller/other D820 callers are never corrected. Below the AB magnitude-1 clamp and
// below the D820 per-axis limit, the vanilla outputs are left untouched.

// -----------------------------------------------------------------------------
// Minimal Win32 / COM / DirectInput declarations (no CRT)
// -----------------------------------------------------------------------------
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef unsigned long DWORD;
typedef unsigned long ULONG;
typedef unsigned long ULONG_PTR;
typedef unsigned long SIZE_T;
typedef unsigned int UINT;
typedef unsigned short WORD;
typedef long LONG;
typedef long HRESULT;
typedef int BOOL;
typedef unsigned char BYTE;
typedef signed int INT32;
typedef unsigned int UINT32;
typedef DWORD (__stdcall *LPTHREAD_START_ROUTINE)(LPVOID);

extern "C" int _fltused = 0;
extern "C" void* memcpy(void* dst,const void* src,SIZE_T n){
    volatile BYTE* d=(volatile BYTE*)dst;const volatile BYTE* s=(const volatile BYTE*)src;
    for(SIZE_T i=0;i<n;++i)d[i]=s[i];return dst;
}
extern "C" void* memset(void* dst,int value,SIZE_T n){
    volatile BYTE* d=(volatile BYTE*)dst;BYTE v=(BYTE)value;for(SIZE_T i=0;i<n;++i)d[i]=v;return dst;
}

#define WINAPI __stdcall
#define TRUE 1
#define FALSE 0
#define DLL_PROCESS_ATTACH 1
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 0x00000001UL
#define CREATE_ALWAYS 2UL
#define FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define INVALID_HANDLE_VALUE ((HANDLE)(ULONG_PTR)0xFFFFFFFFUL)
#define PAGE_READWRITE 0x04UL
#define PAGE_EXECUTE_READWRITE 0x40UL
#define PAGE_NOACCESS 0x01UL
#define PAGE_GUARD 0x100UL
#define MEM_COMMIT 0x1000UL
#define MEM_RESERVE 0x2000UL
#define MEM_RELEASE 0x8000UL
#define TH32CS_SNAPMODULE 0x00000008UL
#define TH32CS_SNAPMODULE32 0x00000010UL
#define UINT_ERROR 0xFFFFFFFFU

extern "C" __declspec(dllimport) HMODULE WINAPI GetModuleHandleA(LPCSTR);
extern "C" __declspec(dllimport) DWORD WINAPI GetModuleFileNameA(HMODULE,LPSTR,DWORD);
extern "C" __declspec(dllimport) HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
extern "C" __declspec(dllimport) BOOL WINAPI WriteFile(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
extern "C" __declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
extern "C" __declspec(dllimport) HANDLE WINAPI CreateThread(LPVOID,DWORD,LPTHREAD_START_ROUTINE,LPVOID,DWORD,DWORD*);
extern "C" __declspec(dllimport) BOOL WINAPI DisableThreadLibraryCalls(HMODULE);

struct LARGE_INTEGER_X { long long QuadPart; };
struct MEMORY_BASIC_INFORMATION_X {
    LPVOID BaseAddress; LPVOID AllocationBase; DWORD AllocationProtect; SIZE_T RegionSize;
    DWORD State; DWORD Protect; DWORD Type;
};
struct MODULEENTRY32A_X {
    DWORD dwSize; DWORD th32ModuleID; DWORD th32ProcessID; DWORD GlblcntUsage; DWORD ProccntUsage;
    BYTE* modBaseAddr; DWORD modBaseSize; HMODULE hModule; char szModule[256]; char szExePath[260];
};
struct GUID_X { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; };
struct DIOBJECTDATAFORMAT_X { const GUID_X* pguid; DWORD dwOfs; DWORD dwType; DWORD dwFlags; };
struct DIDATAFORMAT_X { DWORD dwSize; DWORD dwObjSize; DWORD dwFlags; DWORD dwDataSize; DWORD dwNumObjs; DIOBJECTDATAFORMAT_X* rgodf; };
struct DIMOUSESTATE_X { LONG lX; LONG lY; LONG lZ; BYTE rgbButtons[4]; };
struct DIMOUSESTATE2_X { LONG lX; LONG lY; LONG lZ; BYTE rgbButtons[8]; };

static_assert(sizeof(void*)==4,"DS3 fix must be compiled x86");
static_assert(sizeof(GUID_X)==16,"GUID ABI mismatch");
static_assert(sizeof(DIOBJECTDATAFORMAT_X)==16,"DIOBJECTDATAFORMAT x86 ABI mismatch");
static_assert(sizeof(DIDATAFORMAT_X)==24,"DIDATAFORMAT x86 ABI mismatch");
static_assert(sizeof(DIMOUSESTATE_X)==16,"DIMOUSESTATE ABI mismatch");
static_assert(sizeof(DIMOUSESTATE2_X)==20,"DIMOUSESTATE2 ABI mismatch");
static_assert(sizeof(MEMORY_BASIC_INFORMATION_X)==28,"x86 MBI layout mismatch");
static_assert(sizeof(MODULEENTRY32A_X)==548,"x86 MODULEENTRY32 layout mismatch");

static const GUID_X kGuidSysMouse   ={0x6F1D2B60UL,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidSysMouseEm ={0x6F1D2B80UL,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidSysMouseEm2={0x6F1D2B81UL,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidXAxis={0xA36D02E0UL,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidYAxis={0xA36D02E1UL,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidZAxis={0xA36D02E2UL,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};

// -----------------------------------------------------------------------------
// Dynamically resolved APIs
// -----------------------------------------------------------------------------
typedef LPVOID (WINAPI *VirtualAllocFn)(LPVOID,SIZE_T,DWORD,DWORD);
typedef BOOL (WINAPI *VirtualFreeFn)(LPVOID,SIZE_T,DWORD);
typedef BOOL (WINAPI *VirtualProtectFn)(LPVOID,SIZE_T,DWORD,DWORD*);
typedef SIZE_T (WINAPI *VirtualQueryFn)(LPCVOID,MEMORY_BASIC_INFORMATION_X*,SIZE_T);
typedef BOOL (WINAPI *FlushInstructionCacheFn)(HANDLE,LPCVOID,SIZE_T);
typedef HANDLE (WINAPI *GetCurrentProcessFn)(void);
typedef DWORD (WINAPI *GetCurrentProcessIdFn)(void);
typedef DWORD (WINAPI *GetCurrentThreadIdFn)(void);
typedef void (WINAPI *SleepFn)(DWORD);
typedef BOOL (WINAPI *QueryPerformanceCounterFn)(LARGE_INTEGER_X*);
typedef BOOL (WINAPI *QueryPerformanceFrequencyFn)(LARGE_INTEGER_X*);
typedef HMODULE (WINAPI *LoadLibraryAFn)(LPCSTR);
typedef DWORD (WINAPI *GetLastErrorFn)(void);
typedef void (WINAPI *SetLastErrorFn)(DWORD);
typedef HANDLE (WINAPI *CreateToolhelp32SnapshotFn)(DWORD,DWORD);
typedef BOOL (WINAPI *Module32FirstAFn)(HANDLE,MODULEENTRY32A_X*);
typedef BOOL (WINAPI *Module32NextAFn)(HANDLE,MODULEENTRY32A_X*);
typedef UINT (WINAPI *GetPrivateProfileIntAFn)(LPCSTR,LPCSTR,int,LPCSTR);

static VirtualAllocFn pVirtualAlloc=0;
static VirtualFreeFn pVirtualFree=0;
static VirtualProtectFn pVirtualProtect=0;
static VirtualQueryFn pVirtualQuery=0;
static FlushInstructionCacheFn pFlushInstructionCache=0;
static GetCurrentProcessFn pGetCurrentProcess=0;
static GetCurrentProcessIdFn pGetCurrentProcessId=0;
static GetCurrentThreadIdFn pGetCurrentThreadId=0;
static SleepFn pSleep=0;
static QueryPerformanceCounterFn pQueryPerformanceCounter=0;
static QueryPerformanceFrequencyFn pQueryPerformanceFrequency=0;
static LoadLibraryAFn pLoadLibraryA=0;
static GetLastErrorFn pGetLastError=0;
static SetLastErrorFn pSetLastError=0;
static CreateToolhelp32SnapshotFn pCreateToolhelp32Snapshot=0;
static Module32FirstAFn pModule32FirstA=0;
static Module32NextAFn pModule32NextA=0;
static GetPrivateProfileIntAFn pGetPrivateProfileIntA=0;

// -----------------------------------------------------------------------------
// DirectInput declarations
// -----------------------------------------------------------------------------
typedef HRESULT (WINAPI *DirectInput8CreateFn)(HINSTANCE,DWORD,const GUID_X*,LPVOID*,LPVOID);
typedef HRESULT (WINAPI *DI8CreateDeviceFn)(LPVOID,const GUID_X*,LPVOID*,LPVOID);
typedef HRESULT (WINAPI *DIDSetDataFormatFn)(LPVOID,const DIDATAFORMAT_X*);
typedef HRESULT (WINAPI *DIDGetDeviceStateFn)(LPVOID,DWORD,LPVOID);
static DirectInput8CreateFn g_real_DirectInput8Create=0;

// D820: ECX=self, stack: float dt, float* x, float* y.
typedef BOOL (__attribute__((thiscall)) *ProcessorFn)(void*,float,float*,float*);
static ProcessorFn g_processor=0;
// ABC510: thiscall + nine 32-bit stack args, ret 0x24.
typedef UINT32 (__attribute__((thiscall)) *ABC510Fn)(void*,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32);
static ABC510Fn g_abc510=0;
// AB9830: ECX=self, float* x, float* y, float curve0, float curve1.
typedef void (__attribute__((thiscall)) *AB9830Fn)(void*,float*,float*,float,float);
static AB9830Fn g_ab9830=0;

// -----------------------------------------------------------------------------
// Validated fix state / constants
// -----------------------------------------------------------------------------
static HMODULE g_module=0;
static BYTE* g_base=0;
static HANDLE g_status=INVALID_HANDLE_VALUE;
static long long g_qpc_frequency=0;
static volatile LONG g_status_lock=0;
static volatile LONG g_config_enabled=1;
static volatile LONG g_fix_active=0;

static const UINT32 kFreshMouseWindowUs=5000;
static const UINT32 kTailTimeoutUs=250000;
static const UINT32 kTailStableSamples=3;

enum MouseLookProfile { PROFILE_NONE=0, PROFILE_HIP=1, PROFILE_ADS=2 };
static const UINT32 kHipABC510ReturnRva=0x0033F3EDU;
static const UINT32 kHipD820ReturnRva=0x0033F407U;
static const UINT32 kAdsABC510ReturnRva=0x00149F17U;
static const UINT32 kAdsD820ReturnRva=0x00149F2EU;
static const UINT32 kTargetAB9830ReturnRva=0x006BC599U;

struct StateSnapshot {
    UINT32 bits_04,bits_08,bits_0c,bits_10,bits_14,bits_18,bits_1c,bits_20,bits_24,bits_28,bits_2c;
    BYTE flag_30; BYTE reserved[3];
};
struct PathGateSlot { volatile LONG thread_id; volatile LONG hip_depth; volatile LONG ads_depth; };
struct FreshMouseSlot {
    volatile LONG thread_id;
    volatile LONG generation;
    volatile LONG consumed_generation;
    long long qpc_end;
    INT32 x,y;
    volatile LONG valid;
};
struct MouseTailSlot {
    volatile LONG thread_id;
    volatile LONG active;
    volatile LONG generation;
    volatile LONG seen_generation;
    volatile LONG stable_under_count;
    volatile LONG profile;
    long long last_bypass_qpc;
};
struct D820Model { BOOL valid; float preclamp_x,preclamp_y,scale,limit; };
struct VtablePatch {
    LPVOID* vtable;
    UINT32 kind; // 1=IDirectInput8, 2=mouse IDirectInputDevice8
    UINT32 slot_count;
    LPVOID original[16];
    UINT32 patched_mask;
    UINT32 conflict_mask;
    UINT32 owner_module_base;
};
struct MouseDevice {
    LPVOID object;
    LPVOID* vtable;
    UINT32 format_kind,x_offset,y_offset,z_offset,data_size;
};
struct ModuleInfo { UINT32 base,size; };

static PathGateSlot g_path_gate[32];
static FreshMouseSlot g_fresh_mouse[32];
static MouseTailSlot g_mouse_tail[32];
static VtablePatch g_vtable_patches[16];
static MouseDevice g_mouse_devices[16];
static ModuleInfo g_modules[128];
static volatile LONG g_wrap_lock=0,g_module_lock=0;
static UINT32 g_vtable_patch_count=0,g_mouse_device_count=0;
static volatile LONG g_module_count=0;

// -----------------------------------------------------------------------------
// Basic helpers
// -----------------------------------------------------------------------------
static DWORD cstrlen(const char* s){DWORD n=0;while(s&&s[n])++n;return n;}
static BOOL streq(const char* a,const char* b){if(!a||!b)return FALSE;while(*a&&*b){if(*a!=*b)return FALSE;++a;++b;}return *a==*b;}
static char lower_ascii(char c){return(c>='A'&&c<='Z')?(char)(c+('a'-'A')):c;}
static BOOL streqi(const char* a,const char* b){if(!a||!b)return FALSE;while(*a&&*b){if(lower_ascii(*a)!=lower_ascii(*b))return FALSE;++a;++b;}return *a==*b;}
static BOOL nameeq8(const BYTE* p,const char* s){for(int i=0;i<8;++i){char c=(char)p[i],w=s[i];if(c!=w)return FALSE;if(!w)return TRUE;}return s[8]==0;}
static BOOL guid_equal(const GUID_X* a,const GUID_X* b){if(!a||!b)return FALSE;if(a->Data1!=b->Data1||a->Data2!=b->Data2||a->Data3!=b->Data3)return FALSE;for(int i=0;i<8;++i)if(a->Data4[i]!=b->Data4[i])return FALSE;return TRUE;}
static UINT32 guid_kind(const GUID_X* g){if(!g)return 0;if(guid_equal(g,&kGuidXAxis))return 1;if(guid_equal(g,&kGuidYAxis))return 2;if(guid_equal(g,&kGuidZAxis))return 3;return 0;}
static BOOL is_mouse_guid(const GUID_X* g){return g&&(guid_equal(g,&kGuidSysMouse)||guid_equal(g,&kGuidSysMouseEm)||guid_equal(g,&kGuidSysMouseEm2));}
static UINT32 read_u32(const void* p,DWORD off){return p?*(const UINT32*)((const BYTE*)p+off):0;}
static UINT32 float_bits(float v){union{float f;UINT32 u;}x;x.f=v;return x.u;}
static float bits_float(UINT32 u){union{float f;UINT32 u;}x;x.u=u;return x.f;}
static float absf_local(float v){return v<0.0f?-v:v;}
static BOOL finite_f(float v){return (float_bits(v)&0x7F800000U)!=0x7F800000U;}
static BOOL hr_success(HRESULT hr){return hr>=0;}
static void zero_bytes(void* p,DWORD n){BYTE* b=(BYTE*)p;for(DWORD i=0;i<n;++i)b[i]=0;}
static void tiny_lock(volatile LONG* lock){while(__sync_val_compare_and_swap(lock,0,1)!=0){}}
static void tiny_unlock(volatile LONG* lock){__sync_lock_release(lock);}
static long long qpc_now(){LARGE_INTEGER_X q;q.QuadPart=0;if(pQueryPerformanceCounter)pQueryPerformanceCounter(&q);return q.QuadPart;}

static void build_path(char* out,DWORD cap,const char* name){
    DWORD n=GetModuleFileNameA((HMODULE)0,out,cap-1);if(!n||n>=cap-1){out[0]=0;return;}
    while(n&&out[n-1]!='\\'&&out[n-1]!='/')--n;DWORD j=0;while(name[j]&&n+j+1<cap){out[n+j]=name[j];++j;}out[n+j]=0;
}
static void status_lock(){while(__sync_val_compare_and_swap(&g_status_lock,0,1)!=0){if(pSleep)pSleep(0);}}
static void status_unlock(){__sync_lock_release(&g_status_lock);}
static void status_text(const char* s){if(g_status==INVALID_HANDLE_VALUE)return;status_lock();DWORD w=0;WriteFile(g_status,s,cstrlen(s),&w,0);status_unlock();}

static void snapshot_state(void* self,StateSnapshot* s){
    s->bits_04=read_u32(self,0x04);s->bits_08=read_u32(self,0x08);s->bits_0c=read_u32(self,0x0C);s->bits_10=read_u32(self,0x10);
    s->bits_14=read_u32(self,0x14);s->bits_18=read_u32(self,0x18);s->bits_1c=read_u32(self,0x1C);s->bits_20=read_u32(self,0x20);
    s->bits_24=read_u32(self,0x24);s->bits_28=read_u32(self,0x28);s->bits_2c=read_u32(self,0x2C);s->flag_30=self?*((const BYTE*)self+0x30):0;
    s->reserved[0]=s->reserved[1]=s->reserved[2]=0;
}

// -----------------------------------------------------------------------------
// Fresh mouse / camera-path / tail state: unchanged functional logic from v0.8.2
// -----------------------------------------------------------------------------
static PathGateSlot* path_gate_slot(UINT32 tid,BOOL create){
    for(UINT32 i=0;i<32;++i)if((UINT32)g_path_gate[i].thread_id==tid)return &g_path_gate[i];
    if(!create)return 0;
    for(UINT32 i=0;i<32;++i){LONG expected=0;if(__sync_bool_compare_and_swap(&g_path_gate[i].thread_id,expected,(LONG)tid)){g_path_gate[i].hip_depth=0;g_path_gate[i].ads_depth=0;return &g_path_gate[i];}}
    return 0;
}
static LONG path_gate_depth(UINT32 tid,LONG profile){PathGateSlot* s=path_gate_slot(tid,FALSE);if(!s)return 0;return profile==PROFILE_HIP?s->hip_depth:(profile==PROFILE_ADS?s->ads_depth:0);}
static LONG path_gate_profile(UINT32 tid){PathGateSlot* s=path_gate_slot(tid,FALSE);if(!s)return PROFILE_NONE;if(s->ads_depth>0)return PROFILE_ADS;if(s->hip_depth>0)return PROFILE_HIP;return PROFILE_NONE;}

static FreshMouseSlot* fresh_mouse_slot(UINT32 tid,BOOL create){
    for(UINT32 i=0;i<32;++i)if((UINT32)g_fresh_mouse[i].thread_id==tid)return &g_fresh_mouse[i];
    if(!create)return 0;
    for(UINT32 i=0;i<32;++i){LONG expected=0;if(__sync_bool_compare_and_swap(&g_fresh_mouse[i].thread_id,expected,(LONG)tid)){g_fresh_mouse[i].generation=0;g_fresh_mouse[i].consumed_generation=0;g_fresh_mouse[i].qpc_end=0;g_fresh_mouse[i].x=0;g_fresh_mouse[i].y=0;g_fresh_mouse[i].valid=0;return &g_fresh_mouse[i];}}
    return 0;
}
static void fresh_mouse_publish(UINT32 tid,long long qpc,INT32 x,INT32 y){
    FreshMouseSlot* s=fresh_mouse_slot(tid,TRUE);if(!s)return;
    s->qpc_end=qpc;s->x=x;s->y=y;__sync_synchronize();s->valid=1;__sync_add_and_fetch(&s->generation,1);
}
struct FreshMouseClaim { BOOL fresh; BOOL nonzero; INT32 x,y; UINT32 generation,age_ticks; };
static FreshMouseClaim fresh_mouse_claim(UINT32 tid,long long now){
    FreshMouseClaim c;c.fresh=FALSE;c.nonzero=FALSE;c.x=0;c.y=0;c.generation=0;c.age_ticks=0;FreshMouseSlot* s=fresh_mouse_slot(tid,FALSE);if(!s||!s->valid)return c;
    LONG gen=s->generation;if(gen<=0||gen==s->consumed_generation)return c;long long q=s->qpc_end;if(now<q)return c;long long ticks=now-q;
    s->consumed_generation=gen;c.fresh=TRUE;c.x=s->x;c.y=s->y;c.generation=(UINT32)gen;c.age_ticks=(ticks>0xFFFFFFFFLL)?0xFFFFFFFFU:(UINT32)ticks;
    BOOL within=(g_qpc_frequency>0&&ticks>=0&&(ticks*1000000LL)<=g_qpc_frequency*(long long)kFreshMouseWindowUs);if(!within)return c;
    c.nonzero=(c.x!=0||c.y!=0);return c;
}

static MouseTailSlot* mouse_tail_slot(UINT32 tid,BOOL create){
    for(UINT32 i=0;i<32;++i)if((UINT32)g_mouse_tail[i].thread_id==tid)return &g_mouse_tail[i];
    if(!create)return 0;
    for(UINT32 i=0;i<32;++i){LONG expected=0;if(__sync_bool_compare_and_swap(&g_mouse_tail[i].thread_id,expected,(LONG)tid)){g_mouse_tail[i].active=0;g_mouse_tail[i].generation=0;g_mouse_tail[i].seen_generation=0;g_mouse_tail[i].stable_under_count=0;g_mouse_tail[i].profile=PROFILE_NONE;g_mouse_tail[i].last_bypass_qpc=0;return &g_mouse_tail[i];}}
    return 0;
}
static void mouse_tail_start(UINT32 tid,long long now,LONG profile){
    MouseTailSlot* s=mouse_tail_slot(tid,TRUE);if(!s)return;
    s->last_bypass_qpc=now;s->stable_under_count=0;s->profile=profile;s->active=1;__sync_add_and_fetch(&s->generation,1);
}

// EXACT validated v0.8.1/v0.8.2 post-vanilla D820 model.
static BOOL reconstruct_d820_model(const StateSnapshot* post,D820Model* out){
    if(!post||!out||!post->flag_30)return FALSE;
    UINT32 count_cur=post->bits_1c,count_prev=post->bits_2c;if(count_cur>0x7FFFFFFFU||count_prev>0x7FFFFFFFU)return FALSE;
    float blend=bits_float(post->bits_08),limit=bits_float(post->bits_0c);
    float a10=bits_float(post->bits_10),a14=bits_float(post->bits_14),dtcur=bits_float(post->bits_18);
    float a20=bits_float(post->bits_20),a24=bits_float(post->bits_24),dtprev=bits_float(post->bits_28);
    if(!finite_f(blend)||!finite_f(limit)||!finite_f(a10)||!finite_f(a14)||!finite_f(dtcur)||!finite_f(a20)||!finite_f(a24)||!finite_f(dtprev)||limit<0.0f)return FALSE;
    double curx=0.0,cury=0.0,prevx=0.0,prevy=0.0;
    if(count_cur){double avg=(double)dtcur/(double)count_cur;curx=(double)a10*avg;cury=(double)a14*avg;}else dtcur=0.0f;
    if(count_prev){double avg=(double)dtprev/(double)count_prev;prevx=(double)a20*avg;prevy=(double)a24*avg;}else dtprev=0.0f;
    double effdt=(double)dtcur,px=curx,py=cury;
    if(count_prev){double denom=(double)dtprev+(double)dtcur;if(denom>-1.0e-20&&denom<1.0e-20)return FALSE;double alpha=(double)blend*((double)dtprev/denom);double om=1.0-alpha;effdt=(double)dtcur*om+(double)dtprev*alpha;px=curx*om+prevx*alpha;py=cury*om+prevy*alpha;}
    double scale=effdt>0.0?1.0/(effdt*30.0):0.0;
    out->valid=TRUE;out->preclamp_x=(float)px;out->preclamp_y=(float)py;out->scale=(float)scale;out->limit=limit;return TRUE;
}

// -----------------------------------------------------------------------------
// Export resolver / required APIs
// -----------------------------------------------------------------------------
static LPVOID resolve_export_depth(HMODULE module,const char* wanted,DWORD depth){
    if(!module||!wanted||depth>6)return 0;BYTE* base=(BYTE*)module;DWORD e_lfanew=*(DWORD*)(base+0x3C);BYTE* pe=base+e_lfanew;
    if(*(DWORD*)pe!=0x00004550UL)return 0;BYTE* opt=pe+24;DWORD er=*(DWORD*)(opt+0x60),es=*(DWORD*)(opt+0x64);if(!er)return 0;
    BYTE* exp=base+er;DWORD nn=*(DWORD*)(exp+0x18);DWORD* funcs=(DWORD*)(base+*(DWORD*)(exp+0x1C));DWORD* names=(DWORD*)(base+*(DWORD*)(exp+0x20));WORD* ords=(WORD*)(base+*(DWORD*)(exp+0x24));
    for(DWORD i=0;i<nn;++i){const char* name=(const char*)(base+names[i]);if(!streq(name,wanted))continue;DWORD fr=funcs[ords[i]];
        if(fr>=er&&fr<er+es){const char* f=(const char*)(base+fr);char mn[96],fn[128];DWORD m=0,n=0;while(*f&&*f!='.'&&m+5<sizeof(mn))mn[m++]=*f++;if(*f!='.')return 0;++f;while(*f&&n+1<sizeof(fn))fn[n++]=*f++;fn[n]=0;mn[m++]='.';mn[m++]='d';mn[m++]='l';mn[m++]='l';mn[m]=0;HMODULE next=GetModuleHandleA(mn);if(!next&&pLoadLibraryA)next=pLoadLibraryA(mn);if(!next||fn[0]=='#')return 0;return resolve_export_depth(next,fn,depth+1);}return(LPVOID)(base+fr);
    }return 0;
}
static LPVOID resolve_export(HMODULE m,const char* n){return resolve_export_depth(m,n,0);}
static BOOL resolve_apis(){
    HMODULE k=GetModuleHandleA("kernel32.dll");if(!k)return FALSE;
    pLoadLibraryA=(LoadLibraryAFn)resolve_export(k,"LoadLibraryA");pVirtualAlloc=(VirtualAllocFn)resolve_export(k,"VirtualAlloc");pVirtualFree=(VirtualFreeFn)resolve_export(k,"VirtualFree");
    pVirtualProtect=(VirtualProtectFn)resolve_export(k,"VirtualProtect");pVirtualQuery=(VirtualQueryFn)resolve_export(k,"VirtualQuery");pFlushInstructionCache=(FlushInstructionCacheFn)resolve_export(k,"FlushInstructionCache");
    pGetCurrentProcess=(GetCurrentProcessFn)resolve_export(k,"GetCurrentProcess");pGetCurrentProcessId=(GetCurrentProcessIdFn)resolve_export(k,"GetCurrentProcessId");pGetCurrentThreadId=(GetCurrentThreadIdFn)resolve_export(k,"GetCurrentThreadId");
    pSleep=(SleepFn)resolve_export(k,"Sleep");pQueryPerformanceCounter=(QueryPerformanceCounterFn)resolve_export(k,"QueryPerformanceCounter");pQueryPerformanceFrequency=(QueryPerformanceFrequencyFn)resolve_export(k,"QueryPerformanceFrequency");
    pGetLastError=(GetLastErrorFn)resolve_export(k,"GetLastError");pSetLastError=(SetLastErrorFn)resolve_export(k,"SetLastError");pCreateToolhelp32Snapshot=(CreateToolhelp32SnapshotFn)resolve_export(k,"CreateToolhelp32Snapshot");
    pModule32FirstA=(Module32FirstAFn)resolve_export(k,"Module32First");pModule32NextA=(Module32NextAFn)resolve_export(k,"Module32Next");pGetPrivateProfileIntA=(GetPrivateProfileIntAFn)resolve_export(k,"GetPrivateProfileIntA");
    return pLoadLibraryA&&pVirtualAlloc&&pVirtualFree&&pVirtualProtect&&pVirtualQuery&&pFlushInstructionCache&&pGetCurrentProcess&&pGetCurrentProcessId&&pGetCurrentThreadId&&pSleep&&pQueryPerformanceCounter&&pQueryPerformanceFrequency&&pGetLastError&&pSetLastError&&pCreateToolhelp32Snapshot&&pModule32FirstA&&pModule32NextA;
}

// -----------------------------------------------------------------------------
// PE fingerprint, signatures and inline hooks (same signatures/callsites as v0.8.2)
// -----------------------------------------------------------------------------
static BOOL pe_info(BYTE* base,DWORD* timestamp,DWORD* image_size,BYTE** text,DWORD* text_rva,DWORD* text_size){
    if(!base||base[0]!='M'||base[1]!='Z')return FALSE;DWORD e=*(DWORD*)(base+0x3C);BYTE* pe=base+e;if(*(DWORD*)pe!=0x00004550UL||*(WORD*)(pe+4)!=0x014C)return FALSE;
    WORD ns=*(WORD*)(pe+6);*timestamp=*(DWORD*)(pe+8);BYTE* opt=pe+24;if(*(WORD*)opt!=0x010B)return FALSE;*image_size=*(DWORD*)(opt+0x38);WORD optsz=*(WORD*)(pe+20);BYTE* sh=opt+optsz;
    for(WORD i=0;i<ns;++i,sh+=40){if(nameeq8(sh,".text")){DWORD vs=*(DWORD*)(sh+8),rva=*(DWORD*)(sh+12);*text=base+rva;*text_rva=rva;*text_size=vs;return TRUE;}}return FALSE;
}
static BOOL match_mask(const BYTE* p,const BYTE* pat,const char* mask,DWORD n){for(DWORD i=0;i<n;++i)if(mask[i]=='x'&&p[i]!=pat[i])return FALSE;return TRUE;}
static BYTE* find_processor(BYTE* text,DWORD size,DWORD* matches){static const BYTE pat[]={0x55,0x8B,0xEC,0x83,0xEC,0x14,0xD9,0xEE,0x53,0x56,0x8B,0xF1,0x8B,0x0D,0,0,0,0,0x8A,0x81,0x74,0x05,0x00,0x00,0x33,0xDB};static const char mask[]="xxxxxxxxxxxxxx????xxxxxxxx";BYTE* found=0;*matches=0;for(DWORD i=0;i+sizeof(pat)<=size;++i)if(text[i]==0x55&&match_mask(text+i,pat,mask,sizeof(pat))){found=text+i;++*matches;}return found;}
static BYTE* find_abc510(BYTE* text,DWORD size,DWORD* matches){static const BYTE pat[]={0x55,0x8B,0xEC,0x53,0x8B,0xD9,0x80,0xBB,0x50,0x49,0x00,0x00,0x00,0x0F,0x84,0,0,0,0,0x8B,0x45,0x08,0x3C,0x12};static const char mask[]="xxxxxxxxxxxxxxx????xxxxx";BYTE* found=0;*matches=0;for(DWORD i=0;i+sizeof(pat)<=size;++i)if(text[i]==0x55&&match_mask(text+i,pat,mask,sizeof(pat))){found=text+i;++*matches;}return found;}
static BYTE* find_ab9830(BYTE* text,DWORD size,DWORD* matches){static const BYTE pat[]={0x55,0x8B,0xEC,0xD9,0x45,0x10,0xD9,0x05,0,0,0,0,0xD9,0xC0,0xDD,0xEA,0xDF,0xE0,0xDD,0xD9,0xF6,0xC4,0x44};static const char mask[]="xxxxxxxx????xxxxxxxxxxx";BYTE* found=0;*matches=0;for(DWORD i=0;i+sizeof(pat)<=size;++i)if(text[i]==0x55&&match_mask(text+i,pat,mask,sizeof(pat))){found=text+i;++*matches;}return found;}
static BYTE* rel32_call_target(BYTE* call){if(!call||call[0]!=0xE8)return 0;INT32 rel=*(INT32*)(call+1);return call+5+rel;}
static DWORD collect_direct_call_returns(BYTE* text,DWORD size,BYTE* target,UINT32* out,DWORD cap){DWORD count=0;for(DWORD i=0;i+5<=size;++i){if(text[i]!=0xE8)continue;INT32 rel=*(const INT32*)(text+i+1);BYTE* dst=text+i+5+rel;if(dst==target){if(count<cap)out[count]=(UINT32)((text+i+5)-g_base);++count;}}return count;}
static BOOL expected_callsites(const UINT32* got,DWORD count){static const UINT32 expected[]={0x00149F2E,0x0014F806,0x00190725,0x00247892,0x002788AE,0x0027932C,0x0027A4BA,0x0033F407,0x0034E6A5};if(count!=sizeof(expected)/sizeof(expected[0]))return FALSE;for(DWORD i=0;i<count;++i)if(got[i]!=expected[i])return FALSE;return TRUE;}
static BOOL bytes_equal(const BYTE* p,const BYTE* q,DWORD n){for(DWORD i=0;i<n;++i)if(p[i]!=q[i])return FALSE;return TRUE;}
static LPVOID install_hook(BYTE* target,const BYTE* expected,DWORD n,LPVOID hook){
    if(n<5||!bytes_equal(target,expected,n))return 0;BYTE* tramp=(BYTE*)pVirtualAlloc(0,n+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tramp)return 0;
    for(DWORD i=0;i<n;++i)tramp[i]=target[i];tramp[n]=0xE9;*(INT32*)(tramp+n+1)=(INT32)((target+n)-(tramp+n+5));DWORD oldp=0;if(!pVirtualProtect(target,n,PAGE_EXECUTE_READWRITE,&oldp)){pVirtualFree(tramp,0,MEM_RELEASE);return 0;}
    target[0]=0xE9;*(INT32*)(target+1)=(INT32)((BYTE*)hook-(target+5));for(DWORD i=5;i<n;++i)target[i]=0x90;pFlushInstructionCache(pGetCurrentProcess(),target,n);DWORD ignored=0;pVirtualProtect(target,n,oldp,&ignored);return tramp;
}

// -----------------------------------------------------------------------------
// One-time module cache used only for the validated vtable conflict heuristic
// -----------------------------------------------------------------------------
static int find_module_index(UINT32 base){LONG count=g_module_count;for(LONG i=0;i<count;++i)if(g_modules[i].base==base)return(int)i;return-1;}
static void cache_module(UINT32 base,UINT32 size){if(!base||!size)return;tiny_lock(&g_module_lock);if(find_module_index(base)<0&&g_module_count<128){LONG n=g_module_count;g_modules[n].base=base;g_modules[n].size=size;__sync_synchronize();g_module_count=n+1;}tiny_unlock(&g_module_lock);}
static void scan_modules(){HANDLE snap=pCreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pGetCurrentProcessId());if(snap==INVALID_HANDLE_VALUE)return;MODULEENTRY32A_X me;zero_bytes(&me,sizeof(me));me.dwSize=sizeof(me);if(pModule32FirstA(snap,&me)){do{cache_module((UINT32)(ULONG_PTR)me.modBaseAddr,me.modBaseSize);}while(pModule32NextA(snap,&me));}CloseHandle(snap);}
static UINT32 module_base_for_address(LPVOID p){UINT32 a=(UINT32)(ULONG_PTR)p;LONG count=g_module_count;for(LONG i=0;i<count;++i){UINT32 b=g_modules[i].base,s=g_modules[i].size;if(a>=b&&a-b<s)return b;}return 0;}

static BOOL readable_span(const void* ptr,SIZE_T bytes){
    if(!ptr||bytes==0)return FALSE;const BYTE* p=(const BYTE*)ptr;SIZE_T remain=bytes;while(remain){MEMORY_BASIC_INFORMATION_X mbi;SIZE_T got=pVirtualQuery(p,&mbi,sizeof(mbi));if(got<sizeof(mbi)||mbi.State!=MEM_COMMIT||(mbi.Protect&PAGE_NOACCESS)||(mbi.Protect&PAGE_GUARD))return FALSE;const BYTE* end=(const BYTE*)mbi.BaseAddress+mbi.RegionSize;if(p>=end)return FALSE;SIZE_T avail=(SIZE_T)(end-p),step=avail<remain?avail:remain;p+=step;remain-=step;}return TRUE;
}

// -----------------------------------------------------------------------------
// In-place COM vtable slot patching. Object vptr is never replaced.
// Final build patches only the slots required by the fix: CreateDevice,
// SetDataFormat, GetDeviceState. Observation-only v0.8.2 hooks are removed.
// -----------------------------------------------------------------------------
extern "C" HRESULT WINAPI hook_DI8_CreateDevice(LPVOID,const GUID_X*,LPVOID*,LPVOID);
extern "C" HRESULT WINAPI hook_Mouse_SetDataFormat(LPVOID,const DIDATAFORMAT_X*);
extern "C" HRESULT WINAPI hook_Mouse_GetDeviceState(LPVOID,DWORD,LPVOID);

static VtablePatch* find_vtable_patch(LPVOID* vt,UINT32 kind){UINT32 n=g_vtable_patch_count;for(UINT32 i=0;i<n;++i)if(g_vtable_patches[i].vtable==vt&&g_vtable_patches[i].kind==kind)return &g_vtable_patches[i];return 0;}
static VtablePatch* find_patch_for_self(LPVOID self,UINT32 kind){if(!self||!readable_span(self,sizeof(LPVOID)))return 0;LPVOID* vt=*(LPVOID**)self;if(!vt)return 0;return find_vtable_patch(vt,kind);}
static MouseDevice* find_mouse_device(LPVOID object){UINT32 n=g_mouse_device_count;for(UINT32 i=0;i<n;++i)if(g_mouse_devices[i].object==object)return &g_mouse_devices[i];return 0;}
static BOOL patch_vtable_slot(LPVOID* vt,UINT32 slot,LPVOID expected,LPVOID replacement){DWORD oldp=0;if(!vt||!pVirtualProtect(&vt[slot],sizeof(LPVOID),PAGE_READWRITE,&oldp))return FALSE;BOOL ok=FALSE;if(vt[slot]==expected){vt[slot]=replacement;__sync_synchronize();ok=(vt[slot]==replacement);}DWORD ignored=0;pVirtualProtect(&vt[slot],sizeof(LPVOID),oldp,&ignored);return ok;}

static BOOL prepare_di8_vtable(LPVOID object){
    if(!object||!readable_span(object,sizeof(LPVOID)))return FALSE;LPVOID* vt=*(LPVOID**)object;if(!vt||!readable_span(vt,11*sizeof(LPVOID)))return FALSE;
    tiny_lock(&g_wrap_lock);VtablePatch* existing=find_vtable_patch(vt,1);if(existing){BOOL ok=(existing->patched_mask&(1U<<3))!=0;tiny_unlock(&g_wrap_lock);return ok;}
    if(g_vtable_patch_count>=16){tiny_unlock(&g_wrap_lock);return FALSE;}UINT32 idx=g_vtable_patch_count;VtablePatch* vp=&g_vtable_patches[idx];zero_bytes(vp,sizeof(*vp));vp->vtable=vt;vp->kind=1;vp->slot_count=11;for(UINT32 i=0;i<11;++i)vp->original[i]=vt[i];
    vp->owner_module_base=module_base_for_address(vp->original[0]);if(!vp->owner_module_base){scan_modules();vp->owner_module_base=module_base_for_address(vp->original[0]);}
    __sync_synchronize();g_vtable_patch_count=idx+1;UINT32 target_module=module_base_for_address(vp->original[3]);if(!target_module){scan_modules();target_module=module_base_for_address(vp->original[3]);}
    if(!vp->owner_module_base||target_module!=vp->owner_module_base)vp->conflict_mask|=(1U<<3);else if(patch_vtable_slot(vt,3,vp->original[3],(LPVOID)hook_DI8_CreateDevice))vp->patched_mask|=(1U<<3);else vp->conflict_mask|=(1U<<3);
    BOOL ok=(vp->patched_mask&(1U<<3))!=0;tiny_unlock(&g_wrap_lock);return ok;
}

static MouseDevice* add_mouse_device(LPVOID object){
    if(!object||!readable_span(object,sizeof(LPVOID)))return 0;LPVOID* vt=*(LPVOID**)object;if(!vt||!readable_span(vt,14*sizeof(LPVOID)))return 0;
    tiny_lock(&g_wrap_lock);MouseDevice* md=find_mouse_device(object);if(!md){if(g_mouse_device_count>=16){tiny_unlock(&g_wrap_lock);return 0;}UINT32 mi=g_mouse_device_count;md=&g_mouse_devices[mi];zero_bytes(md,sizeof(*md));md->object=object;md->vtable=vt;md->x_offset=md->y_offset=md->z_offset=UINT_ERROR;__sync_synchronize();g_mouse_device_count=mi+1;}
    VtablePatch* vp=find_vtable_patch(vt,2);if(!vp){if(g_vtable_patch_count>=16){tiny_unlock(&g_wrap_lock);return md;}UINT32 idx=g_vtable_patch_count;vp=&g_vtable_patches[idx];zero_bytes(vp,sizeof(*vp));vp->vtable=vt;vp->kind=2;vp->slot_count=14;for(UINT32 i=0;i<14;++i)vp->original[i]=vt[i];
        vp->owner_module_base=module_base_for_address(vp->original[0]);if(!vp->owner_module_base){scan_modules();vp->owner_module_base=module_base_for_address(vp->original[0]);}__sync_synchronize();g_vtable_patch_count=idx+1;
        const UINT32 slots[2]={9,11};LPVOID hooks[2]={(LPVOID)hook_Mouse_GetDeviceState,(LPVOID)hook_Mouse_SetDataFormat};
        for(UINT32 k=0;k<2;++k){UINT32 slot=slots[k],target_module=module_base_for_address(vp->original[slot]);if(!target_module){scan_modules();target_module=module_base_for_address(vp->original[slot]);}if(!vp->owner_module_base||target_module!=vp->owner_module_base){vp->conflict_mask|=(1U<<slot);continue;}if(patch_vtable_slot(vt,slot,vp->original[slot],hooks[k]))vp->patched_mask|=(1U<<slot);else vp->conflict_mask|=(1U<<slot);}
    }
    tiny_unlock(&g_wrap_lock);return md;
}
static LPVOID original_slot_for_self(LPVOID self,UINT32 kind,UINT32 slot){VtablePatch* vp=find_patch_for_self(self,kind);if(!vp||slot>=vp->slot_count)return 0;return vp->original[slot];}

// -----------------------------------------------------------------------------
// DirectInput hooks needed only for the fresh-mouse gate
// -----------------------------------------------------------------------------
extern "C" HRESULT WINAPI hook_DirectInput8Create(HINSTANCE hinst,DWORD version,const GUID_X* riid,LPVOID* out,LPVOID punk){
    DWORD incoming_error=pGetLastError();pSetLastError(incoming_error);HRESULT hr=g_real_DirectInput8Create(hinst,version,riid,out,punk);DWORD api_error=pGetLastError();
    LPVOID object=(hr_success(hr)&&out&&readable_span(out,sizeof(LPVOID)))?*out:0;if(object)prepare_di8_vtable(object);pSetLastError(api_error);return hr;
}
extern "C" HRESULT WINAPI hook_DI8_CreateDevice(LPVOID self,const GUID_X* rguid,LPVOID* out,LPVOID punk){
    DWORD incoming_error=pGetLastError();DI8CreateDeviceFn original=(DI8CreateDeviceFn)original_slot_for_self(self,1,3);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}
    BOOL mouse=FALSE;if(rguid&&readable_span(rguid,sizeof(GUID_X)))mouse=is_mouse_guid(rguid);pSetLastError(incoming_error);HRESULT hr=original(self,rguid,out,punk);DWORD api_error=pGetLastError();LPVOID dev=(hr_success(hr)&&out&&readable_span(out,sizeof(LPVOID)))?*out:0;if(mouse&&dev)add_mouse_device(dev);pSetLastError(api_error);return hr;
}
extern "C" HRESULT WINAPI hook_Mouse_SetDataFormat(LPVOID self,const DIDATAFORMAT_X* fmt){
    DWORD incoming_error=pGetLastError();DIDSetDataFormatFn original=(DIDSetDataFormatFn)original_slot_for_self(self,2,11);MouseDevice* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self,fmt);}
    UINT32 x=UINT_ERROR,y=UINT_ERROR,z=UINT_ERROR,kind=0;BOOL readable=fmt&&readable_span(fmt,sizeof(DIDATAFORMAT_X));if(readable&&fmt->rgodf&&fmt->dwObjSize>=sizeof(DIOBJECTDATAFORMAT_X)&&fmt->dwNumObjs<=128&&readable_span(fmt->rgodf,(SIZE_T)fmt->dwObjSize*fmt->dwNumObjs)){
        for(DWORD i=0;i<fmt->dwNumObjs;++i){const DIOBJECTDATAFORMAT_X* o=(const DIOBJECTDATAFORMAT_X*)((const BYTE*)fmt->rgodf+(SIZE_T)i*fmt->dwObjSize);UINT32 gk=0;if(o->pguid&&readable_span(o->pguid,sizeof(GUID_X)))gk=guid_kind(o->pguid);if(gk==1)x=o->dwOfs;else if(gk==2)y=o->dwOfs;else if(gk==3)z=o->dwOfs;}
    }
    if(readable){if(fmt->dwDataSize==sizeof(DIMOUSESTATE_X)&&x==0&&y==4&&z==8)kind=1;else if(fmt->dwDataSize==sizeof(DIMOUSESTATE2_X)&&x==0&&y==4&&z==8)kind=2;else if(x!=UINT_ERROR&&y!=UINT_ERROR&&x+4<=fmt->dwDataSize&&y+4<=fmt->dwDataSize)kind=3;}
    pSetLastError(incoming_error);HRESULT hr=original(self,fmt);DWORD api_error=pGetLastError();if(hr_success(hr)&&readable){md->format_kind=kind;md->x_offset=x;md->y_offset=y;md->z_offset=z;md->data_size=fmt->dwDataSize;}pSetLastError(api_error);return hr;
}
extern "C" HRESULT WINAPI hook_Mouse_GetDeviceState(LPVOID self,DWORD cb,LPVOID data){
    DWORD incoming_error=pGetLastError();DIDGetDeviceStateFn original=(DIDGetDeviceStateFn)original_slot_for_self(self,2,9);MouseDevice* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self,cb,data);}
    pSetLastError(incoming_error);HRESULT hr=original(self,cb,data);DWORD api_error=pGetLastError();if(hr_success(hr)&&data&&md->x_offset!=UINT_ERROR&&md->y_offset!=UINT_ERROR&&md->x_offset+4<=cb&&md->y_offset+4<=cb&&readable_span(data,cb)){
        INT32 x=*(const INT32*)((const BYTE*)data+md->x_offset);INT32 y=*(const INT32*)((const BYTE*)data+md->y_offset);fresh_mouse_publish(pGetCurrentThreadId(),qpc_now(),x,y);
    }
    pSetLastError(api_error);return hr;
}

static BOOL patch_directinput8_import(){
    BYTE* base=g_base;DWORD e=*(DWORD*)(base+0x3C);BYTE* pe=base+e;BYTE* opt=pe+24;DWORD import_rva=*(DWORD*)(opt+0x68);if(!import_rva)return FALSE;BYTE* desc=base+import_rva;
    for(;*(DWORD*)desc||*(DWORD*)(desc+12)||*(DWORD*)(desc+16);desc+=20){DWORD oft=*(DWORD*)(desc+0),name_rva=*(DWORD*)(desc+12),ft=*(DWORD*)(desc+16);if(!name_rva||!ft)continue;const char* dll=(const char*)(base+name_rva);if(!streqi(dll,"DINPUT8.dll"))continue;DWORD* names=(DWORD*)(base+(oft?oft:ft));DWORD* iat=(DWORD*)(base+ft);
        for(DWORD i=0;names[i];++i){if(names[i]&0x80000000UL)continue;const char* fn=(const char*)(base+names[i]+2);if(!streq(fn,"DirectInput8Create"))continue;DWORD old=iat[i];if(!old)return FALSE;if(old==(DWORD)(ULONG_PTR)hook_DirectInput8Create)return TRUE;
            DWORD oldp=0;if(!pVirtualProtect(&iat[i],sizeof(DWORD),PAGE_READWRITE,&oldp))return FALSE;g_real_DirectInput8Create=(DirectInput8CreateFn)(ULONG_PTR)old;iat[i]=(DWORD)(ULONG_PTR)hook_DirectInput8Create;DWORD ignored=0;pVirtualProtect(&iat[i],sizeof(DWORD),oldp,&ignored);return TRUE;}
    }return FALSE;
}

// -----------------------------------------------------------------------------
// HIP + ADS selective AB9830 bypass: functional logic unchanged from v0.8.2
// -----------------------------------------------------------------------------
extern "C" UINT32 __attribute__((fastcall)) hook_abc510(void* self,void*,UINT32 a1,UINT32 a2,UINT32 a3,UINT32 a4,UINT32 a5,UINT32 a6,UINT32 a7,UINT32 a8,UINT32 a9){
    UINT32 tid=pGetCurrentThreadId();UINT32 ret=(UINT32)(ULONG_PTR)__builtin_return_address(0);LONG profile=PROFILE_NONE;
    if(g_base&&ret==(UINT32)(ULONG_PTR)(g_base+kHipABC510ReturnRva))profile=PROFILE_HIP;else if(g_base&&ret==(UINT32)(ULONG_PTR)(g_base+kAdsABC510ReturnRva))profile=PROFILE_ADS;
    PathGateSlot* slot=0;if(profile!=PROFILE_NONE){slot=path_gate_slot(tid,TRUE);if(slot){if(profile==PROFILE_HIP)__sync_add_and_fetch(&slot->hip_depth,1);else __sync_add_and_fetch(&slot->ads_depth,1);}}
    UINT32 result=g_abc510(self,a1,a2,a3,a4,a5,a6,a7,a8,a9);if(slot){if(profile==PROFILE_HIP)__sync_sub_and_fetch(&slot->hip_depth,1);else if(profile==PROFILE_ADS)__sync_sub_and_fetch(&slot->ads_depth,1);}return result;
}
extern "C" void __attribute__((fastcall)) hook_ab9830(void* self,void*,float* x,float* y,float curve0,float curve1){
    long long begin=qpc_now();UINT32 tid=pGetCurrentThreadId();UINT32 ret=(UINT32)(ULONG_PTR)__builtin_return_address(0);LONG profile=path_gate_profile(tid);LONG depth=path_gate_depth(tid,profile);
    BOOL targeted=(g_base&&profile!=PROFILE_NONE&&depth>0&&ret==(UINT32)(ULONG_PTR)(g_base+kTargetAB9830ReturnRva));float pre_x=0.0f,pre_y=0.0f;FreshMouseClaim claim;claim.fresh=FALSE;claim.nonzero=FALSE;claim.x=claim.y=0;claim.generation=claim.age_ticks=0;BOOL pre_over=FALSE;
    if(targeted){pre_x=x?*x:0.0f;pre_y=y?*y:0.0f;float mag2=pre_x*pre_x+pre_y*pre_y;pre_over=(mag2>1.0f);claim=fresh_mouse_claim(tid,begin);}
    g_ab9830(self,x,y,curve0,curve1);
    if(targeted){BOOL fresh_nonzero=(claim.fresh&&claim.nonzero&&g_qpc_frequency>0&&((long long)claim.age_ticks*1000000LL)<=g_qpc_frequency*(long long)kFreshMouseWindowUs);if(g_fix_active&&fresh_nonzero&&pre_over&&x&&y){*x=pre_x;*y=pre_y;mouse_tail_start(tid,qpc_now(),profile);}}
}

// -----------------------------------------------------------------------------
// Selective post-vanilla D820 unclamp: EXACT validated v0.8.2 algorithm
// -----------------------------------------------------------------------------
extern "C" BOOL __attribute__((fastcall)) hook_processor(void* self,void*,float dt,float* x,float* y){
    void* ret=__builtin_return_address(0);BOOL result=g_processor(self,dt,x,y);long long now=qpc_now();if(!g_fix_active||!result)return result;
    UINT32 ret_addr=(UINT32)(ULONG_PTR)ret;LONG profile=PROFILE_NONE;if(g_base&&ret_addr==(UINT32)(ULONG_PTR)(g_base+kHipD820ReturnRva))profile=PROFILE_HIP;else if(g_base&&ret_addr==(UINT32)(ULONG_PTR)(g_base+kAdsD820ReturnRva))profile=PROFILE_ADS;if(profile==PROFILE_NONE)return result;
    UINT32 tid=pGetCurrentThreadId();MouseTailSlot* tail=mouse_tail_slot(tid,FALSE);if(!tail||!tail->active||tail->profile!=profile)return result;
    LONG gen=tail->generation;long long age=now-tail->last_bypass_qpc;if(age<0)age=0;BOOL timed_out=(g_qpc_frequency>0&&age*1000000LL>g_qpc_frequency*(long long)kTailTimeoutUs);if(timed_out){tail->active=0;tail->profile=PROFILE_NONE;tail->stable_under_count=0;return result;}
    StateSnapshot post;snapshot_state(self,&post);D820Model model;model.valid=FALSE;model.preclamp_x=model.preclamp_y=model.scale=model.limit=0.0f;if(!reconstruct_d820_model(&post,&model))return result;
    float lim=absf_local(model.limit);BOOL over=(absf_local(model.preclamp_x)>lim||absf_local(model.preclamp_y)>lim);
    if(over&&x&&y){*x=model.preclamp_x*model.scale;*y=model.preclamp_y*model.scale;tail->seen_generation=gen;tail->stable_under_count=0;}
    else{LONG stable=__sync_add_and_fetch(&tail->stable_under_count,1);if(stable>=(LONG)kTailStableSamples){tail->active=0;tail->profile=PROFILE_NONE;tail->stable_under_count=0;}}
    return result;
}

// -----------------------------------------------------------------------------
// Initialization: verify exact executable/path links, install only required hooks.
// No runtime logger. INI 0 means completely inert after startup.
// -----------------------------------------------------------------------------
static DWORD WINAPI init_thread(LPVOID){
    char status_path[1024];build_path(status_path,1024,"DS3_LinearMouseFix.status.txt");g_status=CreateFileA(status_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    status_text("Dead Space 3 Linear Mouse Fix v1.0\r\n");status_text("Final lightweight HIP + ADS build; no binary logger or Raw Input monitor.\r\n");
    if(!resolve_apis()){status_text("REFUSED: required Win32 API resolution failed.\r\n");return 0;}
    char ini_path[1024];build_path(ini_path,1024,"DS3_LinearMouseFix.ini");g_config_enabled=pGetPrivateProfileIntA?pGetPrivateProfileIntA("Mouse","EnableLinearMouseFix",1,ini_path)?1:0:1;
    status_text(g_config_enabled?"CONFIG EnableLinearMouseFix=1\r\n":"CONFIG EnableLinearMouseFix=0\r\n");if(!g_config_enabled){status_text("DISABLED: no hooks installed.\r\n");return 0;}

    g_base=(BYTE*)GetModuleHandleA(0);DWORD ts=0,image_size=0,text_rva=0,text_size=0;BYTE* text=0;if(!pe_info(g_base,&ts,&image_size,&text,&text_rva,&text_size)){status_text("REFUSED: invalid/non-x86 PE image.\r\n");return 0;}
    if(ts!=0x511E9327UL||(image_size!=0x012D4000UL&&image_size!=0x012D6000UL)){status_text("REFUSED: DS3 executable fingerprint mismatch.\r\n");return 0;}
    LARGE_INTEGER_X fq;fq.QuadPart=0;if(!pQueryPerformanceFrequency(&fq)||fq.QuadPart<=0){status_text("REFUSED: QueryPerformanceFrequency failed.\r\n");return 0;}g_qpc_frequency=fq.QuadPart;
    scan_modules();

    // Patch the proven static DINPUT8 import first so the mouse device is seen even if
    // DirectInput initializes while the .text verification below is finishing.
    BOOL di_ok=patch_directinput8_import();

    BYTE* d820=0;DWORD d820_matches=0;for(DWORD attempt=0;attempt<240;++attempt){d820=find_processor(text,text_size,&d820_matches);if(d820_matches==1&&d820)break;pSleep(250);}
    DWORD abc_matches=0,curve_matches=0;BYTE* abc=find_abc510(text,text_size,&abc_matches);BYTE* curve=find_ab9830(text,text_size,&curve_matches);
    if(d820_matches!=1||!d820||abc_matches!=1||!abc||curve_matches!=1||!curve){status_text("REFUSED: required runtime signatures are not unique.\r\n");return 0;}

    UINT32 calls[32];DWORD call_count=collect_direct_call_returns(text,text_size,d820,calls,32);if(!expected_callsites(calls,call_count)){status_text("REFUSED: D820 caller set mismatch.\r\n");return 0;}
    BYTE* hip_abc_call=g_base+0x0033F3E8U;BYTE* ads_abc_call=g_base+0x00149F12U;BYTE* curve_call=g_base+0x006BC594U;BYTE* hip_d820_call=g_base+0x0033F402U;BYTE* ads_d820_call=g_base+0x00149F29U;
    BOOL links_ok=(rel32_call_target(hip_abc_call)==abc)&&(rel32_call_target(ads_abc_call)==abc)&&(rel32_call_target(curve_call)==curve)&&(rel32_call_target(hip_d820_call)==d820)&&(rel32_call_target(ads_d820_call)==d820);
    if(!links_ok){status_text("REFUSED: HIP/ADS camera-path link verification failed.\r\n");return 0;}

    static const BYTE d820_pro[]={0x55,0x8B,0xEC,0x83,0xEC,0x14};
    static const BYTE abc_pro[]={0x55,0x8B,0xEC,0x53,0x8B,0xD9};
    static const BYTE curve_pro[]={0x55,0x8B,0xEC,0xD9,0x45,0x10};
    if(!bytes_equal(d820,d820_pro,sizeof(d820_pro))||!bytes_equal(abc,abc_pro,sizeof(abc_pro))||!bytes_equal(curve,curve_pro,sizeof(curve_pro))){status_text("REFUSED: hook prologue mismatch.\r\n");return 0;}

    g_processor=(ProcessorFn)install_hook(d820,d820_pro,sizeof(d820_pro),(LPVOID)hook_processor);
    g_abc510=(ABC510Fn)install_hook(abc,abc_pro,sizeof(abc_pro),(LPVOID)hook_abc510);
    if(g_abc510)g_ab9830=(AB9830Fn)install_hook(curve,curve_pro,sizeof(curve_pro),(LPVOID)hook_ab9830);
    if(!di_ok||!g_processor||!g_abc510||!g_ab9830){status_text("REFUSED: one or more required hooks could not be installed; correction remains inactive.\r\n");return 0;}

    __sync_synchronize();g_fix_active=1;
    status_text("ACTIVE: validated HIP + ADS linear mouse correction enabled.\r\n");
    status_text("HIP ABC510=0x0033F3ED D820=0x0033F407; ADS ABC510=0x00149F17 D820=0x00149F2E.\r\n");
    return 0;
}

extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){g_module=module;DisableThreadLibraryCalls(module);HANDLE t=CreateThread(0,0,init_thread,0,0,0);if(t)CloseHandle(t);}return TRUE;
}
