// Dead Space 3 Raw Mouse Probe v0.5.1
// x86 / no CRT / diagnostic only.
//
// Strategy:
//   * DO NOT register another Raw Input receiver.
//   * Observe process Raw Input use by IAT-detouring USER32!RegisterRawInputDevices
//     USER32!GetRawInputData, and USER32!GetRawInputBuffer in every loaded module,
//     with periodic rescans.
//   * Preserve the existing verified D820 processor hook from v0.3.
//   * Producer paths only enqueue fixed-size records in memory. Disk I/O and
//     status summaries are performed by the writer thread.
//   * No X/Y replacement, no WndProc subclassing, no on-disk EXE modification.

// -----------------------------------------------------------------------------
// Minimal Win32 declarations (no windows.h / no CRT dependency)
// -----------------------------------------------------------------------------
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HWND;
typedef void* HRAWINPUT;
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
typedef unsigned short USHORT;
typedef long LONG;
typedef int BOOL;
typedef unsigned char BYTE;
typedef signed int INT32;
typedef unsigned int UINT32;
typedef DWORD (__stdcall *LPTHREAD_START_ROUTINE)(LPVOID);

extern "C" int _fltused = 0;
extern "C" void* memcpy(void* dst,const void* src,SIZE_T n){
    volatile BYTE* d=(volatile BYTE*)dst;const volatile BYTE* s=(const volatile BYTE*)src;
    for(SIZE_T i=0;i<n;++i)d[i]=s[i];
    return dst;
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
#define RID_INPUT 0x10000003U
#define RIM_TYPEMOUSE 0U
#define MOUSE_MOVE_ABSOLUTE 0x0001U
#define UINT_ERROR 0xFFFFFFFFU

extern "C" __declspec(dllimport) HMODULE WINAPI GetModuleHandleA(LPCSTR);
extern "C" __declspec(dllimport) DWORD WINAPI GetModuleFileNameA(HMODULE,LPSTR,DWORD);
extern "C" __declspec(dllimport) HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
extern "C" __declspec(dllimport) BOOL WINAPI WriteFile(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
extern "C" __declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
extern "C" __declspec(dllimport) HANDLE WINAPI CreateThread(LPVOID,DWORD,LPTHREAD_START_ROUTINE,LPVOID,DWORD,DWORD*);
extern "C" __declspec(dllimport) BOOL WINAPI DisableThreadLibraryCalls(HMODULE);

struct LARGE_INTEGER_X { long long QuadPart; };
struct RAWINPUTDEVICE_X { USHORT usUsagePage; USHORT usUsage; DWORD dwFlags; HWND hwndTarget; };
struct RAWINPUTHEADER_X { DWORD dwType; DWORD dwSize; HANDLE hDevice; ULONG_PTR wParam; };
struct RAWMOUSE_X {
    USHORT usFlags;
    union { ULONG ulButtons; struct { USHORT usButtonFlags; USHORT usButtonData; }; };
    ULONG ulRawButtons;
    LONG lLastX;
    LONG lLastY;
    ULONG ulExtraInformation;
};
struct RAWINPUT_MOUSE_X { RAWINPUTHEADER_X header; RAWMOUSE_X mouse; };
struct MEMORY_BASIC_INFORMATION_X {
    LPVOID BaseAddress;
    LPVOID AllocationBase;
    DWORD AllocationProtect;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
};
struct MODULEENTRY32A_X {
    DWORD dwSize;
    DWORD th32ModuleID;
    DWORD th32ProcessID;
    DWORD GlblcntUsage;
    DWORD ProccntUsage;
    BYTE* modBaseAddr;
    DWORD modBaseSize;
    HMODULE hModule;
    char szModule[256];
    char szExePath[260];
};

static_assert(sizeof(void*)==4,"DS3 probe must be compiled x86");
static_assert(sizeof(RAWINPUTDEVICE_X)==12,"x86 RAWINPUTDEVICE layout mismatch");
static_assert(sizeof(RAWINPUTHEADER_X)==16,"x86 RAWINPUTHEADER layout mismatch");
static_assert(__builtin_offsetof(RAWINPUTHEADER_X,dwType)==0,"x86 RAWINPUTHEADER dwType offset mismatch");
static_assert(__builtin_offsetof(RAWINPUTHEADER_X,dwSize)==4,"x86 RAWINPUTHEADER dwSize offset mismatch");
static_assert(__builtin_offsetof(RAWINPUTHEADER_X,hDevice)==8,"x86 RAWINPUTHEADER hDevice offset mismatch");
static_assert(__builtin_offsetof(RAWINPUTHEADER_X,wParam)==12,"x86 RAWINPUTHEADER wParam offset mismatch");
static_assert(sizeof(RAWMOUSE_X)==24,"x86 RAWMOUSE layout mismatch");
static_assert(__builtin_offsetof(RAWMOUSE_X,usFlags)==0,"x86 RAWMOUSE usFlags offset mismatch");
static_assert(__builtin_offsetof(RAWMOUSE_X,ulRawButtons)==8,"x86 RAWMOUSE ulRawButtons offset mismatch");
static_assert(__builtin_offsetof(RAWMOUSE_X,lLastX)==12,"x86 RAWMOUSE lLastX offset mismatch");
static_assert(__builtin_offsetof(RAWMOUSE_X,lLastY)==16,"x86 RAWMOUSE lLastY offset mismatch");
static_assert(sizeof(RAWINPUT_MOUSE_X)==40,"x86 mouse RAWINPUT minimum must be 40 bytes");
static_assert(__builtin_offsetof(RAWINPUT_MOUSE_X,mouse)==16,"x86 RAWINPUT mouse payload must start after 16-byte header");
static_assert(sizeof(MEMORY_BASIC_INFORMATION_X)==28,"x86 MBI layout mismatch");
static_assert(sizeof(MODULEENTRY32A_X)==548,"x86 MODULEENTRY32 layout mismatch");

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
typedef BOOL (WINAPI *IsWow64ProcessFn)(HANDLE,BOOL*);

typedef BOOL (WINAPI *RegisterRawInputDevicesFn)(const RAWINPUTDEVICE_X*,UINT,UINT);
typedef UINT (WINAPI *GetRawInputDataFn)(HRAWINPUT,UINT,LPVOID,UINT*,UINT);
typedef UINT (WINAPI *GetRawInputBufferFn)(LPVOID,UINT*,UINT);

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
static IsWow64ProcessFn pIsWow64Process=0;

static RegisterRawInputDevicesFn pRealRegisterRawInputDevices=0;
static GetRawInputDataFn pRealGetRawInputData=0;
static GetRawInputBufferFn pRealGetRawInputBuffer=0;
static RegisterRawInputDevicesFn pCallRegisterRawInputDevices=0;
static GetRawInputDataFn pCallGetRawInputData=0;
static GetRawInputBufferFn pCallGetRawInputBuffer=0;
static volatile LONG g_hotpatch_reg=0;
static volatile LONG g_hotpatch_raw=0;
static volatile LONG g_hotpatch_buffer=0;
static BOOL g_is_wow64=FALSE;

// -----------------------------------------------------------------------------
// Binary format v5.1. Every record is exactly 384 bytes.
// -----------------------------------------------------------------------------
enum RecordType {
    RECORD_MODULE = 1,
    RECORD_REG_CALL = 2,
    RECORD_REG_DEVICE = 3,
    RECORD_RAW_READ = 4,
    RECORD_PROCESSOR = 5,
    RECORD_STATS = 6,
    RECORD_META = 7,
    RECORD_IAT = 8,
    RECORD_RAW_BUFFER_CALL = 9,
    RECORD_RAW_BUFFER_MOUSE = 10
};

#pragma pack(push,1)
struct LogHeaderV5 {
    BYTE magic[8];              // "DS3RM51\0"
    UINT32 version;             // 51
    UINT32 header_size;
    UINT32 record_size;
    UINT32 flags;               // bit0 buffered writer, bit1 API observer; D820 status is in META
    long long qpc_frequency;
    UINT32 image_timestamp;
    UINT32 image_size;
    UINT32 text_rva;
    UINT32 queue_capacity_records;
    UINT32 flush_interval_ms;
    UINT32 module_rescan_ms;
    UINT32 reserved[8];
};

struct CommonV5 {
    long long qpc_begin;
    long long qpc_end;
    UINT32 sequence;
    UINT32 type;
    UINT32 thread_id;
    UINT32 caller_address;
    UINT32 caller_module_base;
    UINT32 caller_rva;
};

struct StateSnapshotV5 {
    UINT32 bits_04;
    UINT32 bits_08;
    UINT32 bits_0c;
    UINT32 bits_10;
    UINT32 bits_14;
    UINT32 bits_18;
    UINT32 bits_1c;
    UINT32 bits_20;
    UINT32 bits_24;
    UINT32 bits_28;
    UINT32 bits_2c;
    BYTE flag_30;
    BYTE reserved[3];
};

struct ModulePayloadV5 {
    UINT32 module_base;
    UINT32 module_size;
    char module_name[128];
    char module_path[176];
};

struct RegCallPayloadV5 {
    UINT32 call_id;
    UINT32 devices_ptr;
    UINT32 num_devices;
    UINT32 cb_size;
    UINT32 captured_devices;
    UINT32 input_readable;
    UINT32 result;
    UINT32 last_error;
    UINT32 reserved[4];
};

struct RegDevicePayloadV5 {
    UINT32 call_id;
    UINT32 index;
    UINT32 total_devices;
    UINT32 usage_page;
    UINT32 usage;
    UINT32 flags;
    UINT32 hwnd_target;
    UINT32 result;
    UINT32 last_error;
    UINT32 reserved[3];
};

struct RawReadPayloadV5 {
    UINT32 call_id;
    UINT32 hrawinput;
    UINT32 ui_command;
    UINT32 pdata;
    UINT32 pcb_size_ptr;
    UINT32 size_before;
    UINT32 size_after;
    UINT32 cb_size_header;
    UINT32 result;
    UINT32 last_error;
    UINT32 buffer_readable;
    UINT32 valid_mouse;
    UINT32 dw_type;
    UINT32 dw_size;
    UINT32 hdevice;
    UINT32 raw_wparam;
    UINT32 us_flags;
    UINT32 button_flags;
    UINT32 button_data;
    INT32 dx;
    INT32 dy;
    UINT32 bytes_returned;
    UINT32 reserved[5];
};

struct RawBufferCallPayloadV5 {
    UINT32 call_id;
    UINT32 pdata;
    UINT32 pcb_size_ptr;
    UINT32 size_before;
    UINT32 size_after;
    UINT32 cb_size_header;
    UINT32 result_count;
    UINT32 last_error;
    UINT32 buffer_readable;
    UINT32 parsed_packets;
    UINT32 mouse_packets;
    UINT32 nonzero_mouse_packets;
    UINT32 wow64_layout;
    UINT32 parse_errors;
    UINT32 reserved[6];
};

struct RawBufferMousePayloadV5 {
    UINT32 call_id;
    UINT32 packet_index;
    UINT32 packet_offset;
    UINT32 packet_size;
    UINT32 layout_payload_offset;
    UINT32 hdevice;
    UINT32 raw_wparam;
    UINT32 us_flags;
    UINT32 button_flags;
    UINT32 button_data;
    INT32 dx;
    INT32 dy;
    UINT32 valid_mouse;
    UINT32 reserved[7];
};

struct ProcessorPayloadV5 {
    UINT32 self_ptr;
    UINT32 object_word_00;
    UINT32 object_word_04;
    UINT32 dt_bits;
    float dt;
    float in_x;
    float in_y;
    float out_x;
    float out_y;
    StateSnapshotV5 pre;
    StateSnapshotV5 post;
    INT32 result;
    UINT32 reserved[6];
};

struct StatsPayloadV5 {
    UINT32 enqueued_total;
    UINT32 dropped_total;
    UINT32 dropped_api;
    UINT32 dropped_processor;
    UINT32 queue_highwater;
    UINT32 write_errors;
    UINT32 reg_calls;
    UINT32 getraw_calls;
    UINT32 getraw_query_calls;
    UINT32 getraw_actual_calls;
    UINT32 raw_mouse_packets;
    UINT32 raw_mouse_nonzero;
    UINT32 raw_read_errors;
    UINT32 d820_records;
    UINT32 modules_seen;
    UINT32 iat_reg_slots;
    UINT32 iat_raw_slots;
    UINT32 iat_buffer_slots;
    UINT32 iat_conflicts;
    UINT32 getbuffer_calls;
    UINT32 getbuffer_query_calls;
    UINT32 getbuffer_actual_calls;
    UINT32 buffer_mouse_packets;
    UINT32 buffer_mouse_nonzero;
    UINT32 buffer_parse_errors;
    UINT32 reserved[1];
};

struct MetaPayloadV5 {
    UINT32 processor_rva;
    UINT32 signature_matches;
    UINT32 callsite_count;
    UINT32 d820_hook_installed;
    UINT32 reserved[12];
};

struct IatPayloadV5 {
    UINT32 module_base;
    UINT32 register_slots_new;
    UINT32 raw_slots_new;
    UINT32 buffer_slots_new;
    UINT32 conflicts_new;
    UINT32 reserved[11];
};

union PayloadV5 {
    ModulePayloadV5 module;
    RegCallPayloadV5 reg_call;
    RegDevicePayloadV5 reg_device;
    RawReadPayloadV5 raw;
    RawBufferCallPayloadV5 raw_buffer_call;
    RawBufferMousePayloadV5 raw_buffer_mouse;
    ProcessorPayloadV5 processor;
    StatsPayloadV5 stats;
    MetaPayloadV5 meta;
    IatPayloadV5 iat;
    BYTE pad[344];
};

struct LogRecordV5 { CommonV5 c; PayloadV5 p; };
#pragma pack(pop)

static_assert(sizeof(LogHeaderV5)==88,"LogHeaderV5 size changed");
static_assert(sizeof(CommonV5)==40,"CommonV5 size changed");
static_assert(sizeof(StateSnapshotV5)==48,"StateSnapshotV5 size changed");
static_assert(sizeof(ModulePayloadV5)==312,"ModulePayloadV5 size changed");
static_assert(sizeof(ProcessorPayloadV5)<=344,"Processor payload too large");
static_assert(sizeof(LogRecordV5)==384,"LogRecordV5 size changed");

// -----------------------------------------------------------------------------
// Global state
// -----------------------------------------------------------------------------
static HMODULE g_module=0;
static BYTE* g_base=0;
static HANDLE g_log=INVALID_HANDLE_VALUE;
static HANDLE g_status=INVALID_HANDLE_VALUE;
static long long g_qpc_frequency=0;
static volatile LONG g_status_lock=0;
static volatile LONG g_queue_lock=0;
static volatile LONG g_sequence=0;
static volatile LONG g_api_call_id=0;
static volatile LONG g_capture_ready=0;

static volatile LONG g_enqueued_total=0;
static volatile LONG g_dropped_total=0;
static volatile LONG g_dropped_api=0;
static volatile LONG g_dropped_processor=0;
static volatile LONG g_queue_highwater=0;
static volatile LONG g_write_errors=0;
static volatile LONG g_reg_calls=0;
static volatile LONG g_getraw_calls=0;
static volatile LONG g_getraw_query_calls=0;
static volatile LONG g_getraw_actual_calls=0;
static volatile LONG g_getbuffer_calls=0;
static volatile LONG g_getbuffer_query_calls=0;
static volatile LONG g_getbuffer_actual_calls=0;
static volatile LONG g_raw_mouse_packets=0;
static volatile LONG g_raw_mouse_nonzero=0;
static volatile LONG g_raw_read_errors=0;
static volatile LONG g_buffer_mouse_packets=0;
static volatile LONG g_buffer_mouse_nonzero=0;
static volatile LONG g_buffer_parse_errors=0;
static volatile LONG g_d820_records=0;
static volatile LONG g_iat_reg_slots=0;
static volatile LONG g_iat_raw_slots=0;
static volatile LONG g_iat_buffer_slots=0;
static volatile LONG g_iat_conflicts=0;
static volatile LONG g_status_game_raw_confirmed=0;
static volatile LONG g_status_processor_confirmed=0;
static volatile LONG g_status_drop_reported=0;
static volatile LONG g_status_buffer_seen=0;

static const UINT32 kQueueCapacity=4096;
static const UINT32 kFlushIntervalMs=25;
static const UINT32 kModuleRescanMs=250;

enum BlockState { BLOCK_FREE=0, BLOCK_ACTIVE=1, BLOCK_READY=2, BLOCK_WRITING=3 };
static LogRecordV5* g_blocks[2]={0,0};
static UINT32 g_block_count[2]={0,0};
static LONG g_block_state[2]={BLOCK_FREE,BLOCK_FREE};
static UINT32 g_active_block=0;

struct ModuleInfoV5 {
    UINT32 base;
    UINT32 size;
    char name[128];
    char path[176];
};
static ModuleInfoV5 g_modules[128];
static volatile LONG g_module_count=0;
static volatile LONG g_module_lock=0;

struct RawSummaryV5 {
    UINT32 module_base;
    UINT32 caller_rva;
    UINT32 packets;
    UINT32 nonzero;
};
static RawSummaryV5 g_raw_summaries[32];
static UINT32 g_raw_summary_count=0;
static long long g_last_summary_qpc=0;

// D820 calling convention: ECX=self, stack args: float deltaTime, float* x, float* y.
typedef BOOL (__attribute__((thiscall)) *ProcessorFn)(void*,float,float*,float*);
static ProcessorFn g_processor=0;

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------
static DWORD cstrlen(const char* s){DWORD n=0;while(s&&s[n])++n;return n;}
static BOOL streq(const char* a,const char* b){if(!a||!b)return FALSE;while(*a&&*b){if(*a!=*b)return FALSE;++a;++b;}return *a==*b;}
static BOOL nameeq8(const BYTE* p,const char* s){for(int i=0;i<8;++i){char c=(char)p[i];char w=s[i];if(c!=w)return FALSE;if(!w)return TRUE;}return s[8]==0;}
static UINT32 read_u32(const void* p,DWORD off){return p?*(const UINT32*)((const BYTE*)p+off):0;}
static UINT32 float_bits(float v){union{float f;UINT32 u;}x;x.f=v;return x.u;}
static long long qpc_now(){LARGE_INTEGER_X q;q.QuadPart=0;if(pQueryPerformanceCounter)pQueryPerformanceCounter(&q);return q.QuadPart;}
static void tiny_lock(volatile LONG* lock){while(__sync_val_compare_and_swap(lock,0,1)!=0){}}
static void tiny_unlock(volatile LONG* lock){__sync_lock_release(lock);}
static void status_lock(){while(__sync_val_compare_and_swap(&g_status_lock,0,1)!=0){if(pSleep)pSleep(0);}}
static void status_unlock(){__sync_lock_release(&g_status_lock);}
static void zero_bytes(void* p,DWORD n){BYTE* b=(BYTE*)p;for(DWORD i=0;i<n;++i)b[i]=0;}
static void copy_string(char* dst,DWORD cap,const char* src){if(!dst||!cap)return;DWORD i=0;if(src)for(;src[i]&&i+1<cap;++i)dst[i]=src[i];dst[i]=0;}
static void build_path(char* out,DWORD cap,const char* name){DWORD n=GetModuleFileNameA((HMODULE)0,out,cap-1);if(!n||n>=cap-1){out[0]=0;return;}while(n&&out[n-1]!='\\'&&out[n-1]!='/')--n;DWORD j=0;while(name[j]&&n+j+1<cap){out[n+j]=name[j];++j;}out[n+j]=0;}
static void write_text_unlocked(HANDLE h,const char* s){DWORD w=0;if(h&&h!=INVALID_HANDLE_VALUE)WriteFile(h,s,cstrlen(s),&w,0);}
static void status_text(const char* s){if(g_status==INVALID_HANDLE_VALUE)return;status_lock();write_text_unlocked(g_status,s);status_unlock();}
static char hexn(UINT32 v){return (char)(v<10?'0'+v:'A'+(v-10));}
static void status_hex32(UINT32 v){char b[11];b[0]='0';b[1]='x';for(int i=0;i<8;++i)b[2+i]=hexn((v>>(28-4*i))&15);b[10]=0;status_text(b);}
static void status_dec(UINT32 v){char b[16];int n=0;if(v==0){status_text("0");return;}while(v&&n<15){b[n++]=(char)('0'+(v%10));v/=10;}char o[16];int j=0;while(n)o[j++]=b[--n];o[j]=0;status_text(o);}

static void snapshot_state(void* self,StateSnapshotV5* s){
    s->bits_04=read_u32(self,0x04);s->bits_08=read_u32(self,0x08);s->bits_0c=read_u32(self,0x0C);
    s->bits_10=read_u32(self,0x10);s->bits_14=read_u32(self,0x14);s->bits_18=read_u32(self,0x18);
    s->bits_1c=read_u32(self,0x1C);s->bits_20=read_u32(self,0x20);s->bits_24=read_u32(self,0x24);
    s->bits_28=read_u32(self,0x28);s->bits_2c=read_u32(self,0x2C);s->flag_30=self?*((const BYTE*)self+0x30):0;
    s->reserved[0]=s->reserved[1]=s->reserved[2]=0;
}

// -----------------------------------------------------------------------------
// Export resolver (follows forwarded exports)
// -----------------------------------------------------------------------------
static LPVOID resolve_export_depth(HMODULE module,const char* wanted,DWORD depth){
    if(!module||!wanted||depth>6)return 0;BYTE* base=(BYTE*)module;
    DWORD e_lfanew=*(DWORD*)(base+0x3C);BYTE* pe=base+e_lfanew;if(*(DWORD*)pe!=0x00004550UL)return 0;
    BYTE* opt=pe+24;DWORD er=*(DWORD*)(opt+0x60);DWORD es=*(DWORD*)(opt+0x64);if(!er)return 0;
    BYTE* exp=base+er;DWORD nn=*(DWORD*)(exp+0x18);DWORD* funcs=(DWORD*)(base+*(DWORD*)(exp+0x1C));
    DWORD* names=(DWORD*)(base+*(DWORD*)(exp+0x20));WORD* ords=(WORD*)(base+*(DWORD*)(exp+0x24));
    for(DWORD i=0;i<nn;++i){
        const char* name=(const char*)(base+names[i]);if(!streq(name,wanted))continue;DWORD fr=funcs[ords[i]];
        if(fr>=er&&fr<er+es){
            const char* f=(const char*)(base+fr);char mn[96];char fn[128];DWORD m=0,n=0;
            while(*f&&*f!='.'&&m+5<sizeof(mn))mn[m++]=*f++;if(*f!='.')return 0;++f;
            while(*f&&n+1<sizeof(fn))fn[n++]=*f++;fn[n]=0;
            mn[m++]='.';mn[m++]='d';mn[m++]='l';mn[m++]='l';mn[m]=0;
            HMODULE next=GetModuleHandleA(mn);if(!next&&pLoadLibraryA)next=pLoadLibraryA(mn);
            if(!next||fn[0]=='#')return 0;return resolve_export_depth(next,fn,depth+1);
        }
        return (LPVOID)(base+fr);
    }
    return 0;
}
static LPVOID resolve_export(HMODULE m,const char* n){return resolve_export_depth(m,n,0);}

static BOOL resolve_apis(){
    HMODULE k=GetModuleHandleA("kernel32.dll");if(!k)return FALSE;
    pLoadLibraryA=(LoadLibraryAFn)resolve_export(k,"LoadLibraryA");
    pVirtualAlloc=(VirtualAllocFn)resolve_export(k,"VirtualAlloc");
    pVirtualFree=(VirtualFreeFn)resolve_export(k,"VirtualFree");
    pVirtualProtect=(VirtualProtectFn)resolve_export(k,"VirtualProtect");
    pVirtualQuery=(VirtualQueryFn)resolve_export(k,"VirtualQuery");
    pFlushInstructionCache=(FlushInstructionCacheFn)resolve_export(k,"FlushInstructionCache");
    pGetCurrentProcess=(GetCurrentProcessFn)resolve_export(k,"GetCurrentProcess");
    pGetCurrentProcessId=(GetCurrentProcessIdFn)resolve_export(k,"GetCurrentProcessId");
    pGetCurrentThreadId=(GetCurrentThreadIdFn)resolve_export(k,"GetCurrentThreadId");
    pSleep=(SleepFn)resolve_export(k,"Sleep");
    pQueryPerformanceCounter=(QueryPerformanceCounterFn)resolve_export(k,"QueryPerformanceCounter");
    pQueryPerformanceFrequency=(QueryPerformanceFrequencyFn)resolve_export(k,"QueryPerformanceFrequency");
    pGetLastError=(GetLastErrorFn)resolve_export(k,"GetLastError");
    pSetLastError=(SetLastErrorFn)resolve_export(k,"SetLastError");
    pCreateToolhelp32Snapshot=(CreateToolhelp32SnapshotFn)resolve_export(k,"CreateToolhelp32Snapshot");
    pModule32FirstA=(Module32FirstAFn)resolve_export(k,"Module32FirstA");
    pModule32NextA=(Module32NextAFn)resolve_export(k,"Module32NextA");
    pIsWow64Process=(IsWow64ProcessFn)resolve_export(k,"IsWow64Process");
    if(!pLoadLibraryA||!pVirtualAlloc||!pVirtualFree||!pVirtualProtect||!pVirtualQuery||!pFlushInstructionCache||
       !pGetCurrentProcess||!pGetCurrentProcessId||!pGetCurrentThreadId||!pSleep||!pQueryPerformanceCounter||
       !pQueryPerformanceFrequency||!pGetLastError||!pSetLastError||!pCreateToolhelp32Snapshot||!pModule32FirstA||!pModule32NextA)return FALSE;
    HMODULE u=GetModuleHandleA("user32.dll");if(!u)u=pLoadLibraryA("user32.dll");if(!u)return FALSE;
    pRealRegisterRawInputDevices=(RegisterRawInputDevicesFn)resolve_export(u,"RegisterRawInputDevices");
    pRealGetRawInputData=(GetRawInputDataFn)resolve_export(u,"GetRawInputData");
    pRealGetRawInputBuffer=(GetRawInputBufferFn)resolve_export(u,"GetRawInputBuffer");
    pCallRegisterRawInputDevices=pRealRegisterRawInputDevices;
    pCallGetRawInputData=pRealGetRawInputData;
    pCallGetRawInputBuffer=pRealGetRawInputBuffer;
    return pRealRegisterRawInputDevices&&pRealGetRawInputData&&pRealGetRawInputBuffer;
}

// -----------------------------------------------------------------------------
// PE fingerprint + D820 signature/callsites
// -----------------------------------------------------------------------------
static BOOL pe_info(BYTE* base,DWORD* timestamp,DWORD* image_size,BYTE** text,DWORD* text_rva,DWORD* text_size){
    if(!base||base[0]!='M'||base[1]!='Z')return FALSE;DWORD e=*(DWORD*)(base+0x3C);BYTE* pe=base+e;
    if(*(DWORD*)pe!=0x00004550UL||*(WORD*)(pe+4)!=0x014C)return FALSE;WORD ns=*(WORD*)(pe+6);*timestamp=*(DWORD*)(pe+8);
    BYTE* opt=pe+24;if(*(WORD*)opt!=0x010B)return FALSE;*image_size=*(DWORD*)(opt+0x38);WORD optsz=*(WORD*)(pe+20);BYTE* sh=opt+optsz;
    for(WORD i=0;i<ns;++i,sh+=40){if(nameeq8(sh,".text")){DWORD vs=*(DWORD*)(sh+8);DWORD rva=*(DWORD*)(sh+12);*text=base+rva;*text_rva=rva;*text_size=vs;return TRUE;}}
    return FALSE;
}
static BOOL match_mask(const BYTE* p,const BYTE* pat,const char* mask,DWORD n){for(DWORD i=0;i<n;++i)if(mask[i]=='x'&&p[i]!=pat[i])return FALSE;return TRUE;}
static BYTE* find_processor(BYTE* text,DWORD size,DWORD* matches){
    static const BYTE pat[]={0x55,0x8B,0xEC,0x83,0xEC,0x14,0xD9,0xEE,0x53,0x56,0x8B,0xF1,0x8B,0x0D,0,0,0,0,0x8A,0x81,0x74,0x05,0x00,0x00,0x33,0xDB};
    static const char mask[]="xxxxxxxxxxxxxx????xxxxxxxx";BYTE* found=0;*matches=0;
    for(DWORD i=0;i+sizeof(pat)<=size;++i)if(text[i]==0x55&&match_mask(text+i,pat,mask,sizeof(pat))){found=text+i;++*matches;}return found;
}
static DWORD collect_direct_call_returns(BYTE* text,DWORD size,BYTE* target,UINT32* out,DWORD cap){DWORD count=0;for(DWORD i=0;i+5<=size;++i){if(text[i]!=0xE8)continue;INT32 rel=*(const INT32*)(text+i+1);BYTE* dst=text+i+5+rel;if(dst==target){if(count<cap)out[count]=(UINT32)((text+i+5)-g_base);++count;}}return count;}
static BOOL expected_callsites(const UINT32* got,DWORD count){static const UINT32 expected[]={0x00149F2E,0x0014F806,0x00190725,0x00247892,0x002788AE,0x0027932C,0x0027A4BA,0x0033F407,0x0034E6A5};if(count!=sizeof(expected)/sizeof(expected[0]))return FALSE;for(DWORD i=0;i<count;++i)if(got[i]!=expected[i])return FALSE;return TRUE;}
static BOOL bytes_equal(const BYTE* p,const BYTE* q,DWORD n){for(DWORD i=0;i<n;++i)if(p[i]!=q[i])return FALSE;return TRUE;}
static LPVOID install_hook(BYTE* target,const BYTE* expected,DWORD n,LPVOID hook){
    if(n<5||!bytes_equal(target,expected,n))return 0;BYTE* tramp=(BYTE*)pVirtualAlloc(0,n+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tramp)return 0;
    for(DWORD i=0;i<n;++i)tramp[i]=target[i];tramp[n]=0xE9;*(INT32*)(tramp+n+1)=(INT32)((target+n)-(tramp+n+5));
    DWORD oldp=0;if(!pVirtualProtect(target,n,PAGE_EXECUTE_READWRITE,&oldp)){pVirtualFree(tramp,0,MEM_RELEASE);return 0;}
    target[0]=0xE9;*(INT32*)(target+1)=(INT32)((BYTE*)hook-(target+5));for(DWORD i=5;i<n;++i)target[i]=0x90;
    pFlushInstructionCache(pGetCurrentProcess(),target,n);DWORD ignored=0;pVirtualProtect(target,n,oldp,&ignored);return tramp;
}

// -----------------------------------------------------------------------------
// Buffered logging
// -----------------------------------------------------------------------------
static void update_highwater(UINT32 value){LONG old=g_queue_highwater;while((UINT32)old<value){LONG prev=__sync_val_compare_and_swap(&g_queue_highwater,old,(LONG)value);if(prev==old)break;old=prev;}}
static void count_drop(UINT32 type){__sync_add_and_fetch(&g_dropped_total,1);if(type==RECORD_PROCESSOR)__sync_add_and_fetch(&g_dropped_processor,1);else __sync_add_and_fetch(&g_dropped_api,1);}
static void copy_record(LogRecordV5* dst,const LogRecordV5* src){volatile UINT32* d=(volatile UINT32*)dst;const volatile UINT32* s=(const volatile UINT32*)src;for(UINT32 i=0;i<(UINT32)(sizeof(LogRecordV5)/4);++i)d[i]=s[i];}
static BOOL enqueue_record(LogRecordV5* r){
    if(!g_capture_ready)return FALSE;tiny_lock(&g_queue_lock);UINT32 b=g_active_block;
    if(g_block_state[b]!=BLOCK_ACTIVE){count_drop(r->c.type);tiny_unlock(&g_queue_lock);return FALSE;}
    if(g_block_count[b]>=kQueueCapacity){UINT32 other=1U-b;if(g_block_state[other]==BLOCK_FREE){g_block_state[b]=BLOCK_READY;g_block_state[other]=BLOCK_ACTIVE;g_block_count[other]=0;g_active_block=other;b=other;}else{count_drop(r->c.type);tiny_unlock(&g_queue_lock);return FALSE;}}
    r->c.sequence=(UINT32)__sync_add_and_fetch(&g_sequence,1);copy_record(&g_blocks[b][g_block_count[b]],r);++g_block_count[b];update_highwater(g_block_count[b]);__sync_add_and_fetch(&g_enqueued_total,1);
    if(g_block_count[b]>=kQueueCapacity){UINT32 other=1U-b;if(g_block_state[other]==BLOCK_FREE){g_block_state[b]=BLOCK_READY;g_block_state[other]=BLOCK_ACTIVE;g_block_count[other]=0;g_active_block=other;}}
    tiny_unlock(&g_queue_lock);return TRUE;
}
static BOOL write_exact(const void* data,DWORD bytes){DWORD wrote=0;BOOL ok=WriteFile(g_log,data,bytes,&wrote,0);if(!ok||wrote!=bytes){__sync_add_and_fetch(&g_write_errors,1);return FALSE;}return TRUE;}

// -----------------------------------------------------------------------------
// Module cache + caller classification
// -----------------------------------------------------------------------------
static int find_module_index(UINT32 base){LONG count=g_module_count;for(LONG i=0;i<count;++i)if(g_modules[i].base==base)return (int)i;return -1;}
static void common_fill(LogRecordV5* r,UINT32 type,void* return_address){
    zero_bytes(r,sizeof(*r));r->c.qpc_begin=qpc_now();r->c.type=type;r->c.thread_id=pGetCurrentThreadId?pGetCurrentThreadId():0;r->c.caller_address=(UINT32)(ULONG_PTR)return_address;
    UINT32 a=r->c.caller_address;LONG count=g_module_count;for(LONG i=0;i<count;++i){UINT32 b=g_modules[i].base,s=g_modules[i].size;if(a>=b&&a-b<s){r->c.caller_module_base=b;r->c.caller_rva=a-b;break;}}
}
static void emit_module_record(const ModuleInfoV5* m){LogRecordV5 r;common_fill(&r,RECORD_MODULE,0);r.c.caller_address=0;r.c.caller_module_base=0;r.c.caller_rva=0;r.p.module.module_base=m->base;r.p.module.module_size=m->size;copy_string(r.p.module.module_name,sizeof(r.p.module.module_name),m->name);copy_string(r.p.module.module_path,sizeof(r.p.module.module_path),m->path);r.c.qpc_end=qpc_now();enqueue_record(&r);}
static void cache_module(UINT32 base,UINT32 size,const char* name,const char* path){
    if(!base||!size)return;tiny_lock(&g_module_lock);if(find_module_index(base)>=0){tiny_unlock(&g_module_lock);return;}LONG n=g_module_count;if(n>=128){tiny_unlock(&g_module_lock);return;}
    g_modules[n].base=base;g_modules[n].size=size;copy_string(g_modules[n].name,sizeof(g_modules[n].name),name);copy_string(g_modules[n].path,sizeof(g_modules[n].path),path);__sync_synchronize();g_module_count=n+1;ModuleInfoV5 copy=g_modules[n];tiny_unlock(&g_module_lock);emit_module_record(&copy);
}
static const char* module_name_for_base(UINT32 base){int i=find_module_index(base);return i>=0?g_modules[i].name:"unknown";}

// -----------------------------------------------------------------------------
// Safe-ish user-buffer inspection. Never writes caller buffers.
// -----------------------------------------------------------------------------
static BOOL readable_span(const void* ptr,SIZE_T bytes){
    if(!ptr||bytes==0)return FALSE;const BYTE* p=(const BYTE*)ptr;SIZE_T remain=bytes;
    while(remain){MEMORY_BASIC_INFORMATION_X mbi;SIZE_T got=pVirtualQuery(p,&mbi,sizeof(mbi));if(got<sizeof(mbi)||mbi.State!=MEM_COMMIT||(mbi.Protect&PAGE_NOACCESS)||(mbi.Protect&PAGE_GUARD))return FALSE;
        const BYTE* end=(const BYTE*)mbi.BaseAddress+mbi.RegionSize;if(p>=end)return FALSE;SIZE_T avail=(SIZE_T)(end-p);SIZE_T step=avail<remain?avail:remain;p+=step;remain-=step;}
    return TRUE;
}

// -----------------------------------------------------------------------------
// USER32 observer hooks. They call the real API first/once and preserve LastError.
// -----------------------------------------------------------------------------
extern "C" BOOL WINAPI hook_RegisterRawInputDevices(const RAWINPUTDEVICE_X* devices,UINT num,UINT cb){
    DWORD entry_error=pGetLastError();void* ret=__builtin_return_address(0);LogRecordV5 base;common_fill(&base,RECORD_REG_CALL,ret);UINT32 call_id=(UINT32)__sync_add_and_fetch(&g_api_call_id,1);
    RAWINPUTDEVICE_X snap[32];UINT cap=num<32?num:32;BOOL readable=FALSE;
    if(devices&&cb==sizeof(RAWINPUTDEVICE_X)&&cap>0&&readable_span(devices,(SIZE_T)cap*sizeof(RAWINPUTDEVICE_X))){readable=TRUE;for(UINT i=0;i<cap;++i)snap[i]=devices[i];}
    pSetLastError(entry_error);BOOL result=pCallRegisterRawInputDevices(devices,num,cb);DWORD after_error=pGetLastError();
    __sync_add_and_fetch(&g_reg_calls,1);
    base.c.qpc_end=qpc_now();base.p.reg_call.call_id=call_id;base.p.reg_call.devices_ptr=(UINT32)(ULONG_PTR)devices;base.p.reg_call.num_devices=num;base.p.reg_call.cb_size=cb;base.p.reg_call.captured_devices=readable?cap:0;base.p.reg_call.input_readable=readable?1U:0U;base.p.reg_call.result=result?1U:0U;base.p.reg_call.last_error=after_error;enqueue_record(&base);
    if(readable){for(UINT i=0;i<cap;++i){LogRecordV5 r;common_fill(&r,RECORD_REG_DEVICE,ret);r.c.qpc_begin=base.c.qpc_begin;r.c.qpc_end=base.c.qpc_end;r.p.reg_device.call_id=call_id;r.p.reg_device.index=i;r.p.reg_device.total_devices=num;r.p.reg_device.usage_page=snap[i].usUsagePage;r.p.reg_device.usage=snap[i].usUsage;r.p.reg_device.flags=snap[i].dwFlags;r.p.reg_device.hwnd_target=(UINT32)(ULONG_PTR)snap[i].hwndTarget;r.p.reg_device.result=result?1U:0U;r.p.reg_device.last_error=after_error;enqueue_record(&r);}}
    pSetLastError(after_error);return result;
}

extern "C" UINT WINAPI hook_GetRawInputData(HRAWINPUT hraw,UINT cmd,LPVOID data,UINT* pcb,UINT cb_header){
    DWORD entry_error=pGetLastError();void* ret=__builtin_return_address(0);UINT size_before=0;BOOL pcb_ok=pcb&&readable_span(pcb,sizeof(UINT));if(pcb_ok)size_before=*pcb;
    UINT32 call_id=(UINT32)__sync_add_and_fetch(&g_api_call_id,1);__sync_add_and_fetch(&g_getraw_calls,1);if(!data)__sync_add_and_fetch(&g_getraw_query_calls,1);else __sync_add_and_fetch(&g_getraw_actual_calls,1);
    long long begin=qpc_now();pSetLastError(entry_error);UINT result=pCallGetRawInputData(hraw,cmd,data,pcb,cb_header);DWORD after_error=pGetLastError();long long end=qpc_now();UINT size_after=pcb_ok?*pcb:0;
    if(cmd==RID_INPUT&&data){
        LogRecordV5 r;common_fill(&r,RECORD_RAW_READ,ret);r.c.qpc_begin=begin;r.c.qpc_end=end;r.p.raw.call_id=call_id;r.p.raw.hrawinput=(UINT32)(ULONG_PTR)hraw;r.p.raw.ui_command=cmd;r.p.raw.pdata=(UINT32)(ULONG_PTR)data;r.p.raw.pcb_size_ptr=(UINT32)(ULONG_PTR)pcb;r.p.raw.size_before=size_before;r.p.raw.size_after=size_after;r.p.raw.cb_size_header=cb_header;r.p.raw.result=result;r.p.raw.last_error=after_error;r.p.raw.bytes_returned=result==UINT_ERROR?0:result;
        const UINT mouse_packet_min=(UINT)(sizeof(RAWINPUTHEADER_X)+sizeof(RAWMOUSE_X));
        BOOL valid_buffer=FALSE;if(result!=UINT_ERROR&&result>=sizeof(RAWINPUTHEADER_X)&&readable_span(data,sizeof(RAWINPUTHEADER_X))){valid_buffer=TRUE;r.p.raw.buffer_readable=1;RAWINPUTHEADER_X* hdr=(RAWINPUTHEADER_X*)data;r.p.raw.dw_type=hdr->dwType;r.p.raw.dw_size=hdr->dwSize;r.p.raw.hdevice=(UINT32)(ULONG_PTR)hdr->hDevice;r.p.raw.raw_wparam=(UINT32)hdr->wParam;
            if(hdr->dwType==RIM_TYPEMOUSE&&result>=mouse_packet_min&&hdr->dwSize>=mouse_packet_min&&readable_span(data,mouse_packet_min)){const RAWMOUSE_X* mouse=(const RAWMOUSE_X*)((const BYTE*)data+sizeof(RAWINPUTHEADER_X));r.p.raw.valid_mouse=1;r.p.raw.us_flags=mouse->usFlags;r.p.raw.button_flags=mouse->usButtonFlags;r.p.raw.button_data=mouse->usButtonData;r.p.raw.dx=mouse->lLastX;r.p.raw.dy=mouse->lLastY;__sync_add_and_fetch(&g_raw_mouse_packets,1);if(!(mouse->usFlags&MOUSE_MOVE_ABSOLUTE)&&(mouse->lLastX||mouse->lLastY))__sync_add_and_fetch(&g_raw_mouse_nonzero,1);}}
        if(result==UINT_ERROR||!valid_buffer)__sync_add_and_fetch(&g_raw_read_errors,1);enqueue_record(&r);
    }
    pSetLastError(after_error);return result;
}

static UINT32 align_up_u32(UINT32 value,UINT32 alignment){return (value+(alignment-1U))&~(alignment-1U);}

extern "C" UINT WINAPI hook_GetRawInputBuffer(LPVOID data,UINT* pcb,UINT cb_header){
    DWORD entry_error=pGetLastError();void* ret=__builtin_return_address(0);UINT size_before=0;BOOL pcb_ok=pcb&&readable_span(pcb,sizeof(UINT));if(pcb_ok)size_before=*pcb;
    UINT32 call_id=(UINT32)__sync_add_and_fetch(&g_api_call_id,1);__sync_add_and_fetch(&g_getbuffer_calls,1);if(!data)__sync_add_and_fetch(&g_getbuffer_query_calls,1);else __sync_add_and_fetch(&g_getbuffer_actual_calls,1);
    long long begin=qpc_now();pSetLastError(entry_error);UINT result=pCallGetRawInputBuffer(data,pcb,cb_header);DWORD after_error=pGetLastError();long long end=qpc_now();UINT size_after=pcb_ok?*pcb:0;
    LogRecordV5 call;common_fill(&call,RECORD_RAW_BUFFER_CALL,ret);call.c.qpc_begin=begin;call.c.qpc_end=end;call.p.raw_buffer_call.call_id=call_id;call.p.raw_buffer_call.pdata=(UINT32)(ULONG_PTR)data;call.p.raw_buffer_call.pcb_size_ptr=(UINT32)(ULONG_PTR)pcb;call.p.raw_buffer_call.size_before=size_before;call.p.raw_buffer_call.size_after=size_after;call.p.raw_buffer_call.cb_size_header=cb_header;call.p.raw_buffer_call.result_count=result;call.p.raw_buffer_call.last_error=after_error;call.p.raw_buffer_call.wow64_layout=g_is_wow64?1U:0U;
    if(data&&result!=UINT_ERROR&&result>0&&size_before>=sizeof(RAWINPUTHEADER_X)&&readable_span(data,sizeof(RAWINPUTHEADER_X))){
        call.p.raw_buffer_call.buffer_readable=1;UINT32 offset=0;UINT32 align=g_is_wow64?8U:4U;UINT32 payload_offset=g_is_wow64?24U:16U;
        for(UINT32 i=0;i<result;++i){
            if(offset>size_before||size_before-offset<sizeof(RAWINPUTHEADER_X)){++call.p.raw_buffer_call.parse_errors;break;}
            const BYTE* packet=(const BYTE*)data+offset;if(!readable_span(packet,sizeof(RAWINPUTHEADER_X))){++call.p.raw_buffer_call.parse_errors;break;}const RAWINPUTHEADER_X* hdr=(const RAWINPUTHEADER_X*)packet;UINT32 packet_size=hdr->dwSize;
            if(packet_size<sizeof(RAWINPUTHEADER_X)||packet_size>size_before-offset){++call.p.raw_buffer_call.parse_errors;break;}
            ++call.p.raw_buffer_call.parsed_packets;
            if(hdr->dwType==RIM_TYPEMOUSE){
                ++call.p.raw_buffer_call.mouse_packets;
                const UINT32 mouse_min=payload_offset+(UINT32)sizeof(RAWMOUSE_X);
                if(packet_size>=mouse_min&&readable_span(packet,mouse_min)){
                    const RAWMOUSE_X* mouse=(const RAWMOUSE_X*)(packet+payload_offset);LogRecordV5 mr;common_fill(&mr,RECORD_RAW_BUFFER_MOUSE,ret);mr.c.qpc_begin=begin;mr.c.qpc_end=end;mr.p.raw_buffer_mouse.call_id=call_id;mr.p.raw_buffer_mouse.packet_index=i;mr.p.raw_buffer_mouse.packet_offset=offset;mr.p.raw_buffer_mouse.packet_size=packet_size;mr.p.raw_buffer_mouse.layout_payload_offset=payload_offset;mr.p.raw_buffer_mouse.hdevice=(UINT32)(ULONG_PTR)hdr->hDevice;mr.p.raw_buffer_mouse.raw_wparam=(UINT32)hdr->wParam;mr.p.raw_buffer_mouse.us_flags=mouse->usFlags;mr.p.raw_buffer_mouse.button_flags=mouse->usButtonFlags;mr.p.raw_buffer_mouse.button_data=mouse->usButtonData;mr.p.raw_buffer_mouse.dx=mouse->lLastX;mr.p.raw_buffer_mouse.dy=mouse->lLastY;mr.p.raw_buffer_mouse.valid_mouse=1;enqueue_record(&mr);__sync_add_and_fetch(&g_buffer_mouse_packets,1);if(!(mouse->usFlags&MOUSE_MOVE_ABSOLUTE)&&(mouse->lLastX||mouse->lLastY)){++call.p.raw_buffer_call.nonzero_mouse_packets;__sync_add_and_fetch(&g_buffer_mouse_nonzero,1);}
                }else{++call.p.raw_buffer_call.parse_errors;}
            }
            UINT32 next=align_up_u32(offset+packet_size,align);if(next<=offset||next>size_before){if(i+1<result)++call.p.raw_buffer_call.parse_errors;break;}offset=next;
        }
    }
    if(call.p.raw_buffer_call.parse_errors)__sync_add_and_fetch(&g_buffer_parse_errors,(LONG)call.p.raw_buffer_call.parse_errors);enqueue_record(&call);pSetLastError(after_error);return result;
}

// -----------------------------------------------------------------------------
// Preferred process-wide API detour: classic x86 hotpatch entry when available.
// We require the documented hotpatch shape (5 bytes padding + "mov edi,edi").
// If the system DLL does not expose this shape, we do not guess; IAT observation
// remains as the conservative fallback.
// -----------------------------------------------------------------------------
static BOOL install_hotpatch(void* target,void* hook,void** call_original){
    if(!target||!hook||!call_original)return FALSE;BYTE* p=(BYTE*)target;
    if(!readable_span(p-5,7))return FALSE;
    if(p[0]!=0x8B||p[1]!=0xFF)return FALSE;
    for(int i=-5;i<0;++i)if(p[i]!=0x90&&p[i]!=0xCC)return FALSE;
    DWORD oldp=0;if(!pVirtualProtect(p-5,7,PAGE_EXECUTE_READWRITE,&oldp))return FALSE;
    p[-5]=0xE9;*(INT32*)(p-4)=(INT32)((BYTE*)hook-p);
    p[0]=0xEB;p[1]=0xF9;
    pFlushInstructionCache(pGetCurrentProcess(),p-5,7);DWORD ignored=0;pVirtualProtect(p-5,7,oldp,&ignored);
    *call_original=(void*)(p+2);return TRUE;
}

// -----------------------------------------------------------------------------
// IAT observer installation. We patch only intact USER32/API-set import slots.
// If another component already owns a slot, we count a conflict and do not overwrite it.
// -----------------------------------------------------------------------------
static BOOL patch_slot(UINT32* slot,UINT32 expected,UINT32 replacement){DWORD oldp=0;if(!pVirtualProtect(slot,sizeof(UINT32),PAGE_READWRITE,&oldp))return FALSE;if(*slot!=expected){DWORD ignored=0;pVirtualProtect(slot,sizeof(UINT32),oldp,&ignored);return FALSE;}*slot=replacement;DWORD ignored=0;pVirtualProtect(slot,sizeof(UINT32),oldp,&ignored);return TRUE;}
static BOOL is_ordinal(UINT32 thunk){return (thunk&0x80000000UL)!=0;}
static void patch_iat_module(BYTE* base,UINT32* reg_new,UINT32* raw_new,UINT32* buffer_new,UINT32* conflicts){
    *reg_new=*raw_new=*buffer_new=*conflicts=0;if(!base||base[0]!='M'||base[1]!='Z')return;DWORD e=*(DWORD*)(base+0x3C);BYTE* pe=base+e;if(*(DWORD*)pe!=0x00004550UL||*(WORD*)(pe+4)!=0x014C)return;BYTE* opt=pe+24;if(*(WORD*)opt!=0x010B)return;
    DWORD import_rva=*(DWORD*)(opt+0x68);DWORD import_size=*(DWORD*)(opt+0x6C);if(!import_rva||!import_size)return;BYTE* d=base+import_rva;BYTE* dend=d+import_size;
    for(;d+20<=dend;d+=20){DWORD oft=*(DWORD*)(d+0);DWORD name=*(DWORD*)(d+12);DWORD ft=*(DWORD*)(d+16);if(!oft&&!name&&!ft)break;if(!ft)continue;UINT32* iat=(UINT32*)(base+ft);UINT32* names=oft?(UINT32*)(base+oft):0;
        for(UINT32 i=0;i<4096;++i){UINT32 cur=iat[i];if(!cur)break;const char* fn=0;if(names&&names[i]&&!is_ordinal(names[i]))fn=(const char*)(base+names[i]+2);
            BOOL want_reg=fn&&streq(fn,"RegisterRawInputDevices");BOOL want_raw=fn&&streq(fn,"GetRawInputData");BOOL want_buffer=fn&&streq(fn,"GetRawInputBuffer");
            if(!fn){if(cur==(UINT32)(ULONG_PTR)pRealRegisterRawInputDevices)want_reg=TRUE;if(cur==(UINT32)(ULONG_PTR)pRealGetRawInputData)want_raw=TRUE;if(cur==(UINT32)(ULONG_PTR)pRealGetRawInputBuffer)want_buffer=TRUE;}
            if(want_reg&&!g_hotpatch_reg){UINT32 hook=(UINT32)(ULONG_PTR)hook_RegisterRawInputDevices;UINT32 real=(UINT32)(ULONG_PTR)pRealRegisterRawInputDevices;if(cur==real){if(patch_slot(&iat[i],real,hook)){++*reg_new;__sync_add_and_fetch(&g_iat_reg_slots,1);}}else if(cur!=hook){++*conflicts;__sync_add_and_fetch(&g_iat_conflicts,1);}}
            if(want_raw&&!g_hotpatch_raw){UINT32 hook=(UINT32)(ULONG_PTR)hook_GetRawInputData;UINT32 real=(UINT32)(ULONG_PTR)pRealGetRawInputData;if(cur==real){if(patch_slot(&iat[i],real,hook)){++*raw_new;__sync_add_and_fetch(&g_iat_raw_slots,1);}}else if(cur!=hook){++*conflicts;__sync_add_and_fetch(&g_iat_conflicts,1);}}
            if(want_buffer&&!g_hotpatch_buffer){UINT32 hook=(UINT32)(ULONG_PTR)hook_GetRawInputBuffer;UINT32 real=(UINT32)(ULONG_PTR)pRealGetRawInputBuffer;if(cur==real){if(patch_slot(&iat[i],real,hook)){++*buffer_new;__sync_add_and_fetch(&g_iat_buffer_slots,1);}}else if(cur!=hook){++*conflicts;__sync_add_and_fetch(&g_iat_conflicts,1);}}
        }
    }
}

static void emit_iat_record(UINT32 base,UINT32 rn,UINT32 gn,UINT32 bn,UINT32 cn){if(!rn&&!gn&&!bn&&!cn)return;LogRecordV5 r;common_fill(&r,RECORD_IAT,0);r.c.caller_address=0;r.c.caller_module_base=base;r.c.caller_rva=0;r.p.iat.module_base=base;r.p.iat.register_slots_new=rn;r.p.iat.raw_slots_new=gn;r.p.iat.buffer_slots_new=bn;r.p.iat.conflicts_new=cn;r.c.qpc_end=qpc_now();enqueue_record(&r);}

static void scan_modules_and_patch(){
    HANDLE snap=pCreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pGetCurrentProcessId());if(snap==INVALID_HANDLE_VALUE)return;MODULEENTRY32A_X me;zero_bytes(&me,sizeof(me));me.dwSize=sizeof(me);
    if(pModule32FirstA(snap,&me)){do{UINT32 base=(UINT32)(ULONG_PTR)me.modBaseAddr;cache_module(base,me.modBaseSize,me.szModule,me.szExePath);if((HMODULE)(ULONG_PTR)base!=g_module){UINT32 rn=0,gn=0,bn=0,cn=0;patch_iat_module(me.modBaseAddr,&rn,&gn,&bn,&cn);emit_iat_record(base,rn,gn,bn,cn);}}while(pModule32NextA(snap,&me));}
    CloseHandle(snap);
}
static DWORD WINAPI module_scanner_thread(LPVOID){while(TRUE){scan_modules_and_patch();pSleep(kModuleRescanMs);}return 0;}

// -----------------------------------------------------------------------------
// D820 hook: unchanged observation semantics.
// -----------------------------------------------------------------------------
extern "C" BOOL __attribute__((fastcall)) hook_processor(void* self,void*,float dt,float* x,float* y){
    void* ret=__builtin_return_address(0);LogRecordV5 r;common_fill(&r,RECORD_PROCESSOR,ret);r.p.processor.self_ptr=(UINT32)(ULONG_PTR)self;r.p.processor.object_word_00=read_u32(self,0);r.p.processor.object_word_04=read_u32(self,4);r.p.processor.dt_bits=float_bits(dt);r.p.processor.dt=dt;r.p.processor.in_x=x?*x:0.0f;r.p.processor.in_y=y?*y:0.0f;snapshot_state(self,&r.p.processor.pre);
    BOOL result=g_processor(self,dt,x,y);
    r.c.qpc_end=qpc_now();r.p.processor.out_x=x?*x:0.0f;r.p.processor.out_y=y?*y:0.0f;snapshot_state(self,&r.p.processor.post);r.p.processor.result=result;__sync_add_and_fetch(&g_d820_records,1);enqueue_record(&r);return result;
}

// -----------------------------------------------------------------------------
// Writer-side status summaries
// -----------------------------------------------------------------------------
static void status_module_plus_rva(UINT32 base,UINT32 rva){const char* n=module_name_for_base(base);status_text(n);status_text("+");status_hex32(rva);}
static void writer_process_record(const LogRecordV5* r){
    if(r->c.type==RECORD_REG_DEVICE){status_text("REG caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" tid=");status_dec(r->c.thread_id);status_text(" usagePage=");status_dec(r->p.reg_device.usage_page);status_text(" usage=");status_dec(r->p.reg_device.usage);status_text(" target=");status_hex32(r->p.reg_device.hwnd_target);status_text(" flags=");status_hex32(r->p.reg_device.flags);status_text(" result=");status_dec(r->p.reg_device.result);if(!r->p.reg_device.result){status_text(" error=");status_dec(r->p.reg_device.last_error);}status_text("\r\n");}
    else if(r->c.type==RECORD_REG_CALL&&r->p.reg_call.captured_devices==0){status_text("REG caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" tid=");status_dec(r->c.thread_id);status_text(" entries=");status_dec(r->p.reg_call.num_devices);status_text(" input_readable=");status_dec(r->p.reg_call.input_readable);status_text(" result=");status_dec(r->p.reg_call.result);if(!r->p.reg_call.result){status_text(" error=");status_dec(r->p.reg_call.last_error);}status_text("\r\n");}
    else if(r->c.type==RECORD_RAW_READ&&r->p.raw.valid_mouse){
        UINT32 mb=r->c.caller_module_base,rv=r->c.caller_rva;UINT32 i=0;for(;i<g_raw_summary_count;++i)if(g_raw_summaries[i].module_base==mb&&g_raw_summaries[i].caller_rva==rv)break;if(i==g_raw_summary_count&&i<32){g_raw_summaries[i].module_base=mb;g_raw_summaries[i].caller_rva=rv;g_raw_summaries[i].packets=0;g_raw_summaries[i].nonzero=0;++g_raw_summary_count;}if(i<32){++g_raw_summaries[i].packets;if(!(r->p.raw.us_flags&MOUSE_MOVE_ABSOLUTE)&&(r->p.raw.dx||r->p.raw.dy))++g_raw_summaries[i].nonzero;}
        if(!(r->p.raw.us_flags&MOUSE_MOVE_ABSOLUTE)&&(r->p.raw.dx||r->p.raw.dy)&&__sync_val_compare_and_swap(&g_status_game_raw_confirmed,0,1)==0)status_text("GAME_RAW_INPUT_CONFIRMED: valid non-zero mouse data was returned through GetRawInputData/GetRawInputBuffer.\r\n");
    }
    else if(r->c.type==RECORD_RAW_BUFFER_CALL&&__sync_val_compare_and_swap(&g_status_buffer_seen,0,1)==0){status_text("RAW_BUFFER_USED caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" tid=");status_dec(r->c.thread_id);status_text(" result_count=");status_dec(r->p.raw_buffer_call.result_count);status_text(" size_before=");status_dec(r->p.raw_buffer_call.size_before);status_text(" wow64_layout=");status_dec(r->p.raw_buffer_call.wow64_layout);status_text(" parse_errors=");status_dec(r->p.raw_buffer_call.parse_errors);status_text("\r\n");}
    else if(r->c.type==RECORD_RAW_BUFFER_MOUSE&&r->p.raw_buffer_mouse.valid_mouse){
        UINT32 mb=r->c.caller_module_base,rv=r->c.caller_rva;UINT32 i=0;for(;i<g_raw_summary_count;++i)if(g_raw_summaries[i].module_base==mb&&g_raw_summaries[i].caller_rva==rv)break;if(i==g_raw_summary_count&&i<32){g_raw_summaries[i].module_base=mb;g_raw_summaries[i].caller_rva=rv;g_raw_summaries[i].packets=0;g_raw_summaries[i].nonzero=0;++g_raw_summary_count;}if(i<32){++g_raw_summaries[i].packets;if(!(r->p.raw_buffer_mouse.us_flags&MOUSE_MOVE_ABSOLUTE)&&(r->p.raw_buffer_mouse.dx||r->p.raw_buffer_mouse.dy))++g_raw_summaries[i].nonzero;}
        if(!(r->p.raw_buffer_mouse.us_flags&MOUSE_MOVE_ABSOLUTE)&&(r->p.raw_buffer_mouse.dx||r->p.raw_buffer_mouse.dy)&&__sync_val_compare_and_swap(&g_status_game_raw_confirmed,0,1)==0)status_text("GAME_RAW_INPUT_CONFIRMED: valid non-zero mouse data was returned through GetRawInputData/GetRawInputBuffer.\r\n");
    }
    else if(r->c.type==RECORD_PROCESSOR&&__sync_val_compare_and_swap(&g_status_processor_confirmed,0,1)==0)status_text("PROCESSOR_HOOK_CONFIRMED: D820 processor records are being captured.\r\n");
}
static void writer_periodic_summary(){long long now=qpc_now();if(!g_last_summary_qpc)g_last_summary_qpc=now;if(g_qpc_frequency>0&&now-g_last_summary_qpc<g_qpc_frequency*2)return;g_last_summary_qpc=now;for(UINT32 i=0;i<g_raw_summary_count;++i){status_text("RAW_MOUSE caller=");status_module_plus_rva(g_raw_summaries[i].module_base,g_raw_summaries[i].caller_rva);status_text(" packets=");status_dec(g_raw_summaries[i].packets);status_text(" nonzero=");status_dec(g_raw_summaries[i].nonzero);status_text("\r\n");}status_text("RAW_API GetRawInputData_calls=");status_dec((UINT32)g_getraw_calls);status_text(" GetRawInputBuffer_calls=");status_dec((UINT32)g_getbuffer_calls);status_text(" buffer_mouse=");status_dec((UINT32)g_buffer_mouse_packets);status_text(" buffer_nonzero=");status_dec((UINT32)g_buffer_mouse_nonzero);status_text(" buffer_parse_errors=");status_dec((UINT32)g_buffer_parse_errors);status_text("\r\nD820 records=");status_dec((UINT32)g_d820_records);status_text("\r\n");if(g_dropped_total>0&&__sync_val_compare_and_swap(&g_status_drop_reported,0,1)==0)status_text("WARNING: logger dropped records; decoder will report exact counters.\r\n");}
static void fill_stats_record(LogRecordV5* r){common_fill(r,RECORD_STATS,0);r->c.caller_address=r->c.caller_module_base=r->c.caller_rva=0;r->p.stats.enqueued_total=(UINT32)g_enqueued_total;r->p.stats.dropped_total=(UINT32)g_dropped_total;r->p.stats.dropped_api=(UINT32)g_dropped_api;r->p.stats.dropped_processor=(UINT32)g_dropped_processor;r->p.stats.queue_highwater=(UINT32)g_queue_highwater;r->p.stats.write_errors=(UINT32)g_write_errors;r->p.stats.reg_calls=(UINT32)g_reg_calls;r->p.stats.getraw_calls=(UINT32)g_getraw_calls;r->p.stats.getraw_query_calls=(UINT32)g_getraw_query_calls;r->p.stats.getraw_actual_calls=(UINT32)g_getraw_actual_calls;r->p.stats.raw_mouse_packets=(UINT32)g_raw_mouse_packets;r->p.stats.raw_mouse_nonzero=(UINT32)g_raw_mouse_nonzero;r->p.stats.raw_read_errors=(UINT32)g_raw_read_errors;r->p.stats.d820_records=(UINT32)g_d820_records;r->p.stats.modules_seen=(UINT32)g_module_count;r->p.stats.iat_reg_slots=(UINT32)g_iat_reg_slots;r->p.stats.iat_raw_slots=(UINT32)g_iat_raw_slots;r->p.stats.iat_buffer_slots=(UINT32)g_iat_buffer_slots;r->p.stats.iat_conflicts=(UINT32)g_iat_conflicts;r->p.stats.getbuffer_calls=(UINT32)g_getbuffer_calls;r->p.stats.getbuffer_query_calls=(UINT32)g_getbuffer_query_calls;r->p.stats.getbuffer_actual_calls=(UINT32)g_getbuffer_actual_calls;r->p.stats.buffer_mouse_packets=(UINT32)g_buffer_mouse_packets;r->p.stats.buffer_mouse_nonzero=(UINT32)g_buffer_mouse_nonzero;r->p.stats.buffer_parse_errors=(UINT32)g_buffer_parse_errors;r->c.qpc_end=qpc_now();}
static DWORD WINAPI writer_thread(LPVOID){
    while(TRUE){UINT32 ready=2,count=0;tiny_lock(&g_queue_lock);for(UINT32 i=0;i<2;++i)if(g_block_state[i]==BLOCK_READY){ready=i;break;}if(ready<2){g_block_state[ready]=BLOCK_WRITING;count=g_block_count[ready];}tiny_unlock(&g_queue_lock);
        if(ready==2){pSleep(kFlushIntervalMs);tiny_lock(&g_queue_lock);for(UINT32 i=0;i<2;++i)if(g_block_state[i]==BLOCK_READY){ready=i;break;}if(ready==2){UINT32 a=g_active_block,other=1U-a;if(g_block_state[a]==BLOCK_ACTIVE&&g_block_count[a]>0&&g_block_state[other]==BLOCK_FREE){g_block_state[a]=BLOCK_READY;g_block_state[other]=BLOCK_ACTIVE;g_block_count[other]=0;g_active_block=other;ready=a;}}if(ready<2){g_block_state[ready]=BLOCK_WRITING;count=g_block_count[ready];}tiny_unlock(&g_queue_lock);}
        if(ready<2&&count){for(UINT32 i=0;i<count;++i)writer_process_record(&g_blocks[ready][i]);write_exact(g_blocks[ready],(DWORD)(count*sizeof(LogRecordV5)));LogRecordV5 s;fill_stats_record(&s);write_exact(&s,sizeof(s));tiny_lock(&g_queue_lock);g_block_count[ready]=0;g_block_state[ready]=BLOCK_FREE;tiny_unlock(&g_queue_lock);writer_periodic_summary();}
    }
}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------
static DWORD WINAPI init_thread(LPVOID){
    char status_path[1024],log_path[1024];build_path(status_path,1024,"DS3_RawMouseProbe.status.txt");build_path(log_path,1024,"DS3_RawMouseProbe.bin");
    g_status=CreateFileA(status_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);status_text("Dead Space 3 Raw Mouse Probe v0.5.1 API Observer\r\n");status_text("MODE: diagnostic only; no Raw Input registration, no WndProc subclassing, no mouse/camera value replacement.\r\n");status_text("LOGGER: API/D820 producer paths are memory-only; disk I/O is on a separate writer thread.\r\n");
    if(!resolve_apis()){status_text("REFUSED: required Win32 API resolution failed. Nothing hooked.\r\n");return 0;}
    if(pIsWow64Process){BOOL wow=FALSE;if(pIsWow64Process(pGetCurrentProcess(),&wow))g_is_wow64=wow;}
    status_text("WOW64: ");status_dec(g_is_wow64?1U:0U);status_text(" (GetRawInputBuffer parser uses ");status_dec(g_is_wow64?8U:4U);status_text("-byte block alignment and mouse payload offset ");status_dec(g_is_wow64?24U:16U);status_text(")\r\n");
    g_base=(BYTE*)GetModuleHandleA(0);DWORD ts=0,image_size=0,text_rva=0,text_size=0;BYTE* text=0;if(!pe_info(g_base,&ts,&image_size,&text,&text_rva,&text_size)){status_text("REFUSED: invalid/non-x86 PE image. Nothing hooked.\r\n");return 0;}
    status_text("PE timestamp: ");status_hex32(ts);status_text("\r\nImage size: ");status_hex32(image_size);status_text("\r\n.text RVA: ");status_hex32(text_rva);status_text("\r\n");if(ts!=0x511E9327UL||(image_size!=0x012D4000UL&&image_size!=0x012D6000UL)){status_text("REFUSED: DS3 version/fingerprint mismatch. Nothing hooked.\r\n");return 0;}
    LARGE_INTEGER_X fq;fq.QuadPart=0;if(!pQueryPerformanceFrequency(&fq)||fq.QuadPart<=0){status_text("REFUSED: QueryPerformanceFrequency failed. Nothing hooked.\r\n");return 0;}g_qpc_frequency=fq.QuadPart;
    g_blocks[0]=(LogRecordV5*)pVirtualAlloc(0,kQueueCapacity*sizeof(LogRecordV5),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);g_blocks[1]=(LogRecordV5*)pVirtualAlloc(0,kQueueCapacity*sizeof(LogRecordV5),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!g_blocks[0]||!g_blocks[1]){status_text("REFUSED: cannot allocate buffered log blocks. Nothing hooked.\r\n");return 0;}g_block_state[0]=BLOCK_ACTIVE;g_block_state[1]=BLOCK_FREE;g_active_block=0;
    g_log=CreateFileA(log_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(g_log==INVALID_HANDLE_VALUE){status_text("REFUSED: cannot create DS3_RawMouseProbe.bin. Nothing hooked.\r\n");return 0;}
    LogHeaderV5 h;zero_bytes(&h,sizeof(h));h.magic[0]='D';h.magic[1]='S';h.magic[2]='3';h.magic[3]='R';h.magic[4]='M';h.magic[5]='5';h.magic[6]='1';h.version=51;h.header_size=sizeof(h);h.record_size=sizeof(LogRecordV5);h.flags=3;h.qpc_frequency=fq.QuadPart;h.image_timestamp=ts;h.image_size=image_size;h.text_rva=text_rva;h.queue_capacity_records=kQueueCapacity;h.flush_interval_ms=kFlushIntervalMs;h.module_rescan_ms=kModuleRescanMs;DWORD wrote=0;if(!WriteFile(g_log,&h,sizeof(h),&wrote,0)||wrote!=sizeof(h)){status_text("REFUSED: failed to write binary header.\r\n");return 0;}
    g_capture_ready=1;HANDLE wt=CreateThread(0,0,writer_thread,0,0,0);if(!wt){status_text("REFUSED: could not create writer thread.\r\n");return 0;}CloseHandle(wt);

    // Cache/patch currently loaded import tables first so caller modules are known
    // before the preferred process-wide hotpatch is armed. No Raw Input API is called.
    scan_modules_and_patch();
    // Install API observation as early as possible. No RegisterRawInputDevices call is made here.
    void* call_reg=0;void* call_raw=0;void* call_buffer=0;
    if(install_hotpatch((void*)pRealRegisterRawInputDevices,(void*)hook_RegisterRawInputDevices,&call_reg)){pCallRegisterRawInputDevices=(RegisterRawInputDevicesFn)call_reg;g_hotpatch_reg=1;status_text("API_HOTPATCH RegisterRawInputDevices=OK (process-wide).\r\n");}
    else status_text("API_HOTPATCH RegisterRawInputDevices=UNAVAILABLE; using loaded-module IAT fallback.\r\n");
    if(install_hotpatch((void*)pRealGetRawInputData,(void*)hook_GetRawInputData,&call_raw)){pCallGetRawInputData=(GetRawInputDataFn)call_raw;g_hotpatch_raw=1;status_text("API_HOTPATCH GetRawInputData=OK (process-wide).\r\n");}
    else status_text("API_HOTPATCH GetRawInputData=UNAVAILABLE; using loaded-module IAT fallback.\r\n");
    if(install_hotpatch((void*)pRealGetRawInputBuffer,(void*)hook_GetRawInputBuffer,&call_buffer)){pCallGetRawInputBuffer=(GetRawInputBufferFn)call_buffer;g_hotpatch_buffer=1;status_text("API_HOTPATCH GetRawInputBuffer=OK (process-wide).\r\n");}
    else status_text("API_HOTPATCH GetRawInputBuffer=UNAVAILABLE; using loaded-module IAT fallback.\r\n");
    HANDLE mt=CreateThread(0,0,module_scanner_thread,0,0,0);if(mt)CloseHandle(mt);status_text("API_OBSERVER_ACTIVE: RegisterRawInputDevices + GetRawInputData + GetRawInputBuffer observed; no Raw Input receiver was registered by this probe.\r\n");

    // Keep the already-proven D820 diagnostic hook in parallel.
    status_text("WAITING: scanning runtime-unpacked .text for D820 signature.\r\n");BYTE* target=0;DWORD matches=0;for(DWORD attempt=0;attempt<240;++attempt){target=find_processor(text,text_size,&matches);if(matches==1&&target)break;pSleep(250);}status_text("Processor signature matches: ");status_dec(matches);status_text("\r\n");
    UINT32 calls[32];DWORD call_count=0;UINT32 processor_rva=0;BOOL d820_ok=FALSE;if(matches==1&&target){processor_rva=(UINT32)(target-g_base);call_count=collect_direct_call_returns(text,text_size,target,calls,32);status_text("Processor RVA found by signature: ");status_hex32(processor_rva);status_text("\r\nDirect caller return RVAs found: ");status_dec(call_count);status_text("\r\n");static const BYTE pro[]={0x55,0x8B,0xEC,0x83,0xEC,0x14};if(expected_callsites(calls,call_count)&&bytes_equal(target,pro,sizeof(pro))){g_processor=(ProcessorFn)install_hook(target,pro,sizeof(pro),(LPVOID)hook_processor);d820_ok=g_processor!=0;}}
    LogRecordV5 meta;common_fill(&meta,RECORD_META,0);meta.c.caller_address=meta.c.caller_module_base=meta.c.caller_rva=0;meta.p.meta.processor_rva=processor_rva;meta.p.meta.signature_matches=matches;meta.p.meta.callsite_count=call_count;meta.p.meta.d820_hook_installed=d820_ok?1U:0U;meta.c.qpc_end=qpc_now();enqueue_record(&meta);
    if(d820_ok)status_text("ACTIVE: D820 hook verified and installed. API observer remains active.\r\n");else status_text("WARNING: D820 hook was not installed; API observer remains active for Raw Input ownership diagnosis.\r\n");
    status_text("EXPECTED: REG lines identify registration ownership; GAME_RAW_INPUT_CONFIRMED appears only after real non-zero mouse data is returned through GetRawInputData or GetRawInputBuffer.\r\n");
    return 0;
}

extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){g_module=module;DisableThreadLibraryCalls(module);HANDLE t=CreateThread(0,0,init_thread,0,0,0);if(t)CloseHandle(t);}return TRUE;}
