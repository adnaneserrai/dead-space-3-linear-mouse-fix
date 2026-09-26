// Dead Space 3 DirectInput + D820 + AB9830 Probe v0.7
// x86 / no CRT / diagnostic only.
//
// Observes the game's existing DirectInput8 mouse path, the already verified
// D820 processor, and the AB9830 radial-processing function on the camera path.
// No input values are changed. No Raw Input device is registered. No WndProc is
// subclassed. deadspace3.exe is never modified on disk.

// -----------------------------------------------------------------------------
// Minimal Win32 / COM / DirectInput declarations
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
typedef unsigned short USHORT;
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
struct RAWINPUTDEVICE_X { USHORT usUsagePage; USHORT usUsage; DWORD dwFlags; HWND hwndTarget; };
struct DIOBJECTDATAFORMAT_X { const GUID_X* pguid; DWORD dwOfs; DWORD dwType; DWORD dwFlags; };
struct DIDATAFORMAT_X { DWORD dwSize; DWORD dwObjSize; DWORD dwFlags; DWORD dwDataSize; DWORD dwNumObjs; DIOBJECTDATAFORMAT_X* rgodf; };
struct DIMOUSESTATE_X { LONG lX; LONG lY; LONG lZ; BYTE rgbButtons[4]; };
struct DIMOUSESTATE2_X { LONG lX; LONG lY; LONG lZ; BYTE rgbButtons[8]; };
struct DIDEVICEOBJECTDATA_X { DWORD dwOfs; DWORD dwData; DWORD dwTimeStamp; DWORD dwSequence; ULONG_PTR uAppData; };

static_assert(sizeof(void*)==4,"DS3 probe must be compiled x86");
static_assert(sizeof(GUID_X)==16,"GUID ABI mismatch");
static_assert(sizeof(RAWINPUTDEVICE_X)==12,"RAWINPUTDEVICE x86 ABI mismatch");
static_assert(sizeof(DIOBJECTDATAFORMAT_X)==16,"DIOBJECTDATAFORMAT x86 ABI mismatch");
static_assert(sizeof(DIDATAFORMAT_X)==24,"DIDATAFORMAT x86 ABI mismatch");
static_assert(sizeof(DIMOUSESTATE_X)==16,"DIMOUSESTATE ABI mismatch");
static_assert(sizeof(DIMOUSESTATE2_X)==20,"DIMOUSESTATE2 ABI mismatch");
static_assert(sizeof(DIDEVICEOBJECTDATA_X)==20,"DIDEVICEOBJECTDATA x86 ABI mismatch");
static_assert(sizeof(MEMORY_BASIC_INFORMATION_X)==28,"x86 MBI layout mismatch");
static_assert(sizeof(MODULEENTRY32A_X)==548,"x86 MODULEENTRY32 layout mismatch");

// DirectInput predefined GUIDs used only for classification, never passed to the game.
static const GUID_X kGuidSysMouse   ={0x6F1D2B60UL,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidSysMouseEm ={0x6F1D2B80UL,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidSysMouseEm2={0x6F1D2B81UL,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidXAxis={0xA36D02E0UL,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidYAxis={0xA36D02E1UL,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID_X kGuidZAxis={0xA36D02E2UL,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};

// -----------------------------------------------------------------------------
// Dynamically resolved Win32 APIs
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
typedef UINT (WINAPI *GetRegisteredRawInputDevicesFn)(RAWINPUTDEVICE_X*,UINT*,UINT);

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
static GetRegisteredRawInputDevicesFn pGetRegisteredRawInputDevices=0;

// -----------------------------------------------------------------------------
// DirectInput function pointer declarations
// -----------------------------------------------------------------------------
typedef HRESULT (WINAPI *DirectInput8CreateFn)(HINSTANCE,DWORD,const GUID_X*,LPVOID*,LPVOID);
typedef HRESULT (WINAPI *DI8CreateDeviceFn)(LPVOID,const GUID_X*,LPVOID*,LPVOID);
typedef HRESULT (WINAPI *DIDSetDataFormatFn)(LPVOID,const DIDATAFORMAT_X*);
typedef HRESULT (WINAPI *DIDSetCooperativeLevelFn)(LPVOID,HWND,DWORD);
typedef HRESULT (WINAPI *DIDAcquireFn)(LPVOID);
typedef HRESULT (WINAPI *DIDUnacquireFn)(LPVOID);
typedef HRESULT (WINAPI *DIDGetDeviceStateFn)(LPVOID,DWORD,LPVOID);
typedef HRESULT (WINAPI *DIDGetDeviceDataFn)(LPVOID,DWORD,DIDEVICEOBJECTDATA_X*,DWORD*,DWORD);

static DirectInput8CreateFn g_real_DirectInput8Create=0;

// -----------------------------------------------------------------------------
// Binary log format v6.1. Every record is 512 bytes.
// -----------------------------------------------------------------------------
enum RecordTypeV6 {
    REC_MODULE=1,
    REC_DI8_CREATE=2,
    REC_DI_CREATE_DEVICE=3,
    REC_DI_SET_DATA_FORMAT=4,
    REC_DI_SET_COOP=5,
    REC_DI_ACQUIRE=6,
    REC_DI_UNACQUIRE=7,
    REC_DI_GET_STATE=8,
    REC_DI_GET_DATA_CALL=9,
    REC_DI_GET_DATA_AXIS=10,
    REC_PROCESSOR=11,
    REC_RAW_REG=12,
    REC_STATS=13,
    REC_META=14,
    REC_IAT=15,
    REC_DI8_VTABLE=16,
    REC_MOUSE_VTABLE=17,
    REC_CREATE_DEVICE_ENTER=18,
    REC_AB9830=19
};

#pragma pack(push,1)
struct LogHeaderV6 {
    BYTE magic[8];              // "DS3DI70\0"
    UINT32 version;             // 70
    UINT32 header_size;
    UINT32 record_size;
    UINT32 flags;               // bit0 buffered writer, bit1 DI observer, bit2 passive raw-reg monitor, bit3 AB9830 observer
    long long qpc_frequency;
    UINT32 image_timestamp;
    UINT32 image_size;
    UINT32 text_rva;
    UINT32 queue_capacity_records;
    UINT32 flush_interval_ms;
    UINT32 module_rescan_ms;
    UINT32 rawreg_poll_ms;
    UINT32 reserved[7];
};
struct CommonV6 {
    long long qpc_begin;
    long long qpc_end;
    UINT32 sequence;
    UINT32 type;
    UINT32 thread_id;
    UINT32 caller_address;
    UINT32 caller_module_base;
    UINT32 caller_rva;
};
struct StateSnapshotV6 {
    UINT32 bits_04,bits_08,bits_0c,bits_10,bits_14,bits_18,bits_1c,bits_20,bits_24,bits_28,bits_2c;
    BYTE flag_30; BYTE reserved[3];
};
struct ModulePayloadV6 { UINT32 module_base,module_size; char module_name[128]; char module_path[300]; };
struct Di8CreatePayloadV6 {
    UINT32 call_id,hinst,version,riid_ptr,out_ptr,punk_outer,result_hr,last_error,object_ptr,slot3_patched;
    GUID_X requested_iid; UINT32 reserved[12];
};
struct CreateDevicePayloadV6 {
    UINT32 call_id,di8_ptr,guid_ptr,out_ptr,punk_outer,result_hr,last_error,device_ptr,is_mouse_guid,vtable_patched;
    GUID_X device_guid; UINT32 reserved[12];
};
struct FormatObjectV6 { UINT32 guid_kind,offset,type,flags; };
struct SetDataFormatPayloadV6 {
    UINT32 device_ptr,format_ptr,dw_size,dw_obj_size,dw_flags,dw_data_size,dw_num_objs,format_readable;
    UINT32 axis_x_offset,axis_y_offset,axis_z_offset,format_kind,captured_objects,result_hr,last_error,reserved0;
    FormatObjectV6 objects[16];
};
struct SetCoopPayloadV6 { UINT32 device_ptr,hwnd,flags,result_hr,last_error,reserved[11]; };
struct AcquirePayloadV6 { UINT32 device_ptr,result_hr,last_error,reserved[13]; };
struct GetStatePayloadV6 {
    UINT32 device_ptr,cb_data,data_ptr,result_hr,last_error,format_kind,x_offset,y_offset,z_offset,buffer_readable,valid_xyz;
    INT32 x,y,z; UINT32 reserved[10];
};
struct GetDataCallPayloadV6 {
    UINT32 call_id,device_ptr,cb_object_data,data_ptr,count_ptr,count_before,count_after,flags,result_hr,last_error;
    UINT32 format_kind,x_offset,y_offset,z_offset,buffer_readable,parsed_objects,axis_events,truncated,reserved[8];
};
struct GetDataAxisPayloadV6 {
    UINT32 call_id,device_ptr,event_index,object_offset,raw_data,time_stamp,sequence,axis_kind,result_hr;
    INT32 delta; UINT32 reserved[10];
};
struct ProcessorPayloadV6 {
    UINT32 self_ptr,object_word_00,object_word_04,dt_bits; float dt,in_x,in_y,out_x,out_y;
    StateSnapshotV6 pre,post; INT32 result; UINT32 reserved[8];
};
struct AB9830PayloadV7 {
    UINT32 self_ptr,x_ptr,y_ptr;
    UINT32 direct_return_rva;        // expected 0x006BC599 (inside ABC510)
    UINT32 parent_return_rva;        // expected 0x0033F3ED (camera path caller of ABC510)
    UINT32 target_d820_return_rva;   // 0x0033F407 for explicit linkage
    UINT32 path_gate_depth;
    UINT32 reserved0;
    float curve_arg0,curve_arg1;
    float pre_x,pre_y;
    float post_x,post_y;
    UINT32 reserved[12];
};
struct RawRegPayloadV6 {
    UINT32 snapshot_id,index,total,usage_page,usage,flags,hwnd_target,result,last_error,changed,reserved[7];
};
struct StatsPayloadV6 {
    UINT32 enqueued_total,dropped_total,dropped_di,dropped_processor,queue_highwater,write_errors;
    UINT32 di8create_calls,create_device_calls,mouse_devices,setformat_calls,setcoop_calls,acquire_calls,unacquire_calls;
    UINT32 getstate_calls,getstate_mouse_packets,getstate_nonzero,getdata_calls,getdata_axis_events,getdata_nonzero;
    UINT32 d820_records,modules_seen,rawreg_polls,rawreg_changes,rawreg_last_entries,di_iat_patched,di_iat_conflict;
    UINT32 dropped_curve,abc510_target_calls,ab9830_calls,ab9830_targeted,ab9830_hook_installed;
};
struct MetaPayloadV6 {
    UINT32 processor_rva,signature_matches,callsite_count,d820_hook_installed,di_iat_patched,raw_monitor_active;
    UINT32 abc510_rva,abc510_signature_matches,abc510_hook_installed;
    UINT32 ab9830_rva,ab9830_signature_matches,ab9830_hook_installed;
    UINT32 target_abc510_return_rva,target_ab9830_return_rva,target_d820_return_rva,reserved0;
};
struct IatPayloadV6 { UINT32 slot_rva,previous_target,previous_target_module,previous_target_rva,patched,conflict,reserved[10]; };
struct VtablePayloadV61 {
    UINT32 object_ptr;
    UINT32 vtable_ptr;
    UINT32 interface_kind;       // 1=IDirectInput8, 2=mouse IDirectInputDevice8
    UINT32 slot_count;
    UINT32 owner_module_base;    // module containing slot 0; conservative conflict heuristic
    UINT32 patched_mask;
    UINT32 conflict_mask;
    UINT32 slots[16];            // original targets before our patch attempt
    UINT32 reserved[8];
};
union PayloadV6 {
    ModulePayloadV6 module; Di8CreatePayloadV6 di8; CreateDevicePayloadV6 create_device; SetDataFormatPayloadV6 setformat;
    SetCoopPayloadV6 coop; AcquirePayloadV6 acquire; GetStatePayloadV6 state; GetDataCallPayloadV6 data_call;
    GetDataAxisPayloadV6 data_axis; ProcessorPayloadV6 processor; RawRegPayloadV6 rawreg; StatsPayloadV6 stats;
    MetaPayloadV6 meta; IatPayloadV6 iat; VtablePayloadV61 vtable; AB9830PayloadV7 ab9830; BYTE pad[472];
};
struct LogRecordV6 { CommonV6 c; PayloadV6 p; };
#pragma pack(pop)

static_assert(sizeof(LogHeaderV6)==88,"LogHeaderV6 size changed");
static_assert(sizeof(CommonV6)==40,"CommonV6 size changed");
static_assert(sizeof(StateSnapshotV6)==48,"StateSnapshotV6 size changed");
static_assert(sizeof(ModulePayloadV6)==436,"ModulePayloadV6 size changed");
static_assert(sizeof(SetDataFormatPayloadV6)<=472,"SetDataFormat payload too large");
static_assert(sizeof(ProcessorPayloadV6)<=472,"Processor payload too large");
static_assert(sizeof(VtablePayloadV61)<=472,"Vtable payload too large");
static_assert(sizeof(AB9830PayloadV7)<=472,"AB9830 payload too large");
static_assert(sizeof(LogRecordV6)==512,"LogRecordV6 size changed");

// -----------------------------------------------------------------------------
// Global state
// -----------------------------------------------------------------------------
static HMODULE g_module=0;
static BYTE* g_base=0;
static HANDLE g_log=INVALID_HANDLE_VALUE;
static HANDLE g_status=INVALID_HANDLE_VALUE;
static long long g_qpc_frequency=0;
static volatile LONG g_status_lock=0,g_queue_lock=0,g_sequence=0,g_call_id=0,g_capture_ready=0;

static volatile LONG g_enqueued_total=0,g_dropped_total=0,g_dropped_di=0,g_dropped_processor=0,g_queue_highwater=0,g_write_errors=0;
static volatile LONG g_di8create_calls=0,g_create_device_calls=0,g_mouse_devices=0,g_setformat_calls=0,g_setcoop_calls=0,g_acquire_calls=0,g_unacquire_calls=0;
static volatile LONG g_getstate_calls=0,g_getstate_mouse_packets=0,g_getstate_nonzero=0,g_getdata_calls=0,g_getdata_axis_events=0,g_getdata_nonzero=0;
static volatile LONG g_d820_records=0,g_rawreg_polls=0,g_rawreg_changes=0,g_rawreg_last_entries=0,g_di_iat_patched=0,g_di_iat_conflict=0;
static volatile LONG g_dropped_curve=0,g_abc510_target_calls=0,g_ab9830_calls=0,g_ab9830_targeted=0,g_ab9830_hook_installed=0;
static volatile LONG g_status_di_confirmed=0,g_status_processor_confirmed=0,g_status_curve_confirmed=0,g_status_drop_reported=0;

static const UINT32 kQueueCapacity=4096;
static const UINT32 kFlushIntervalMs=25;
static const UINT32 kModuleRescanMs=250;
static const UINT32 kRawRegPollMs=500;
enum BlockState { BLOCK_FREE=0,BLOCK_ACTIVE=1,BLOCK_READY=2,BLOCK_WRITING=3 };
static LogRecordV6* g_blocks[2]={0,0};
static UINT32 g_block_count[2]={0,0};
static LONG g_block_state[2]={BLOCK_FREE,BLOCK_FREE};
static UINT32 g_active_block=0;

struct ModuleInfoV6 { UINT32 base,size; char name[128]; char path[300]; };
static ModuleInfoV6 g_modules[128];
static volatile LONG g_module_count=0,g_module_lock=0;
static long long g_last_summary_qpc=0;

// D820 calling convention: ECX=self, stack args: float deltaTime, float* x, float* y.
typedef BOOL (__attribute__((thiscall)) *ProcessorFn)(void*,float,float*,float*);
static ProcessorFn g_processor=0;

// ABC510: thiscall with nine 32-bit stack arguments (ret 0x24). We preserve the
// raw 32-bit argument payload exactly and use this hook only as a per-thread path gate.
typedef UINT32 (__attribute__((thiscall)) *ABC510Fn)(void*,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32,UINT32);
static ABC510Fn g_abc510=0;

// AB9830: ECX=self, stack args float* x, float* y, float curve0, float curve1.
typedef void (__attribute__((thiscall)) *AB9830Fn)(void*,float*,float*,float,float);
static AB9830Fn g_ab9830=0;

static const UINT32 kTargetABC510ReturnRva=0x0033F3EDU;
static const UINT32 kTargetD820ReturnRva=0x0033F407U;
static const UINT32 kTargetAB9830ReturnRva=0x006BC599U;

struct PathGateSlotV7 { volatile LONG thread_id; volatile LONG depth; };
static PathGateSlotV7 g_path_gate[32];

struct VtablePatchV61 {
    LPVOID* vtable;
    UINT32 kind;                 // 1=IDirectInput8, 2=mouse device vtable
    UINT32 slot_count;
    LPVOID original[16];
    UINT32 patched_mask;
    UINT32 conflict_mask;
    UINT32 owner_module_base;
};
struct MouseDeviceV61 {
    LPVOID object;
    LPVOID* vtable;
    UINT32 format_kind,x_offset,y_offset,z_offset,data_size;
};
static VtablePatchV61 g_vtable_patches[16]; static UINT32 g_vtable_patch_count=0;
static MouseDeviceV61 g_mouse_devices_table[16]; static UINT32 g_mouse_device_count=0;
static volatile LONG g_wrap_lock=0;

// -----------------------------------------------------------------------------
// Helpers
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
static long long qpc_now(){LARGE_INTEGER_X q;q.QuadPart=0;if(pQueryPerformanceCounter)pQueryPerformanceCounter(&q);return q.QuadPart;}
static PathGateSlotV7* path_gate_slot(UINT32 tid,BOOL create){
    for(UINT32 i=0;i<32;++i)if((UINT32)g_path_gate[i].thread_id==tid)return &g_path_gate[i];
    if(!create)return 0;
    for(UINT32 i=0;i<32;++i){LONG expected=0;if(__sync_bool_compare_and_swap(&g_path_gate[i].thread_id,expected,(LONG)tid)){g_path_gate[i].depth=0;return &g_path_gate[i];}}
    return 0;
}
static LONG path_gate_depth(UINT32 tid){PathGateSlotV7* s=path_gate_slot(tid,FALSE);return s?s->depth:0;}
static void tiny_lock(volatile LONG* lock){while(__sync_val_compare_and_swap(lock,0,1)!=0){}}
static void tiny_unlock(volatile LONG* lock){__sync_lock_release(lock);}
static void status_lock(){while(__sync_val_compare_and_swap(&g_status_lock,0,1)!=0){if(pSleep)pSleep(0);}}
static void status_unlock(){__sync_lock_release(&g_status_lock);}
static void zero_bytes(void* p,DWORD n){BYTE* b=(BYTE*)p;for(DWORD i=0;i<n;++i)b[i]=0;}
static void copy_string(char* dst,DWORD cap,const char* src){if(!dst||!cap)return;DWORD i=0;if(src)for(;src[i]&&i+1<cap;++i)dst[i]=src[i];dst[i]=0;}
static void build_path(char* out,DWORD cap,const char* name){DWORD n=GetModuleFileNameA((HMODULE)0,out,cap-1);if(!n||n>=cap-1){out[0]=0;return;}while(n&&out[n-1]!='\\'&&out[n-1]!='/')--n;DWORD j=0;while(name[j]&&n+j+1<cap){out[n+j]=name[j];++j;}out[n+j]=0;}
static void write_text_unlocked(HANDLE h,const char* s){DWORD w=0;if(h&&h!=INVALID_HANDLE_VALUE)WriteFile(h,s,cstrlen(s),&w,0);}
static void status_text(const char* s){if(g_status==INVALID_HANDLE_VALUE)return;status_lock();write_text_unlocked(g_status,s);status_unlock();}
static char hexn(UINT32 v){return(char)(v<10?'0'+v:'A'+(v-10));}
static void status_hex32(UINT32 v){char b[11];b[0]='0';b[1]='x';for(int i=0;i<8;++i)b[2+i]=hexn((v>>(28-4*i))&15);b[10]=0;status_text(b);}
static void status_dec(UINT32 v){char b[16];int n=0;if(v==0){status_text("0");return;}while(v&&n<15){b[n++]=(char)('0'+(v%10));v/=10;}char o[16];int j=0;while(n)o[j++]=b[--n];o[j]=0;status_text(o);}
static BOOL hr_success(HRESULT hr){return hr>=0;}
static void snapshot_state(void* self,StateSnapshotV6* s){
    s->bits_04=read_u32(self,0x04);s->bits_08=read_u32(self,0x08);s->bits_0c=read_u32(self,0x0C);s->bits_10=read_u32(self,0x10);
    s->bits_14=read_u32(self,0x14);s->bits_18=read_u32(self,0x18);s->bits_1c=read_u32(self,0x1C);s->bits_20=read_u32(self,0x20);
    s->bits_24=read_u32(self,0x24);s->bits_28=read_u32(self,0x28);s->bits_2c=read_u32(self,0x2C);s->flag_30=self?*((const BYTE*)self+0x30):0;
    s->reserved[0]=s->reserved[1]=s->reserved[2]=0;
}

// -----------------------------------------------------------------------------
// Export resolver
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
static void report_missing_api(const char* name,const void* p){if(p)return;status_text("MISSING_API: ");status_text(name);status_text("\r\n");}
static BOOL resolve_apis(){
    HMODULE k=GetModuleHandleA("kernel32.dll");if(!k)return FALSE;
    pLoadLibraryA=(LoadLibraryAFn)resolve_export(k,"LoadLibraryA");pVirtualAlloc=(VirtualAllocFn)resolve_export(k,"VirtualAlloc");pVirtualFree=(VirtualFreeFn)resolve_export(k,"VirtualFree");
    pVirtualProtect=(VirtualProtectFn)resolve_export(k,"VirtualProtect");pVirtualQuery=(VirtualQueryFn)resolve_export(k,"VirtualQuery");pFlushInstructionCache=(FlushInstructionCacheFn)resolve_export(k,"FlushInstructionCache");
    pGetCurrentProcess=(GetCurrentProcessFn)resolve_export(k,"GetCurrentProcess");pGetCurrentProcessId=(GetCurrentProcessIdFn)resolve_export(k,"GetCurrentProcessId");pGetCurrentThreadId=(GetCurrentThreadIdFn)resolve_export(k,"GetCurrentThreadId");
    pSleep=(SleepFn)resolve_export(k,"Sleep");pQueryPerformanceCounter=(QueryPerformanceCounterFn)resolve_export(k,"QueryPerformanceCounter");pQueryPerformanceFrequency=(QueryPerformanceFrequencyFn)resolve_export(k,"QueryPerformanceFrequency");
    pGetLastError=(GetLastErrorFn)resolve_export(k,"GetLastError");pSetLastError=(SetLastErrorFn)resolve_export(k,"SetLastError");pCreateToolhelp32Snapshot=(CreateToolhelp32SnapshotFn)resolve_export(k,"CreateToolhelp32Snapshot");
    pModule32FirstA=(Module32FirstAFn)resolve_export(k,"Module32First");pModule32NextA=(Module32NextAFn)resolve_export(k,"Module32Next");
    report_missing_api("KERNEL32!LoadLibraryA",(const void*)pLoadLibraryA);report_missing_api("KERNEL32!VirtualAlloc",(const void*)pVirtualAlloc);report_missing_api("KERNEL32!VirtualFree",(const void*)pVirtualFree);
    report_missing_api("KERNEL32!VirtualProtect",(const void*)pVirtualProtect);report_missing_api("KERNEL32!VirtualQuery",(const void*)pVirtualQuery);report_missing_api("KERNEL32!FlushInstructionCache",(const void*)pFlushInstructionCache);
    report_missing_api("KERNEL32!GetCurrentProcess",(const void*)pGetCurrentProcess);report_missing_api("KERNEL32!GetCurrentProcessId",(const void*)pGetCurrentProcessId);report_missing_api("KERNEL32!GetCurrentThreadId",(const void*)pGetCurrentThreadId);
    report_missing_api("KERNEL32!Sleep",(const void*)pSleep);report_missing_api("KERNEL32!QueryPerformanceCounter",(const void*)pQueryPerformanceCounter);report_missing_api("KERNEL32!QueryPerformanceFrequency",(const void*)pQueryPerformanceFrequency);
    report_missing_api("KERNEL32!GetLastError",(const void*)pGetLastError);report_missing_api("KERNEL32!SetLastError",(const void*)pSetLastError);report_missing_api("KERNEL32!CreateToolhelp32Snapshot",(const void*)pCreateToolhelp32Snapshot);
    report_missing_api("KERNEL32!Module32First",(const void*)pModule32FirstA);report_missing_api("KERNEL32!Module32Next",(const void*)pModule32NextA);
    if(!pLoadLibraryA||!pVirtualAlloc||!pVirtualFree||!pVirtualProtect||!pVirtualQuery||!pFlushInstructionCache||!pGetCurrentProcess||!pGetCurrentProcessId||!pGetCurrentThreadId||!pSleep||!pQueryPerformanceCounter||!pQueryPerformanceFrequency||!pGetLastError||!pSetLastError||!pCreateToolhelp32Snapshot||!pModule32FirstA||!pModule32NextA)return FALSE;
    HMODULE u=GetModuleHandleA("user32.dll");if(!u)u=pLoadLibraryA("user32.dll");if(u)pGetRegisteredRawInputDevices=(GetRegisteredRawInputDevicesFn)resolve_export(u,"GetRegisteredRawInputDevices");
    report_missing_api("USER32!GetRegisteredRawInputDevices",(const void*)pGetRegisteredRawInputDevices);
    return pGetRegisteredRawInputDevices!=0;
}

// -----------------------------------------------------------------------------
// PE fingerprint + D820 signature/callsites
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
static LPVOID install_hook(BYTE* target,const BYTE* expected,DWORD n,LPVOID hook){if(n<5||!bytes_equal(target,expected,n))return 0;BYTE* tramp=(BYTE*)pVirtualAlloc(0,n+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tramp)return 0;for(DWORD i=0;i<n;++i)tramp[i]=target[i];tramp[n]=0xE9;*(INT32*)(tramp+n+1)=(INT32)((target+n)-(tramp+n+5));DWORD oldp=0;if(!pVirtualProtect(target,n,PAGE_EXECUTE_READWRITE,&oldp)){pVirtualFree(tramp,0,MEM_RELEASE);return 0;}target[0]=0xE9;*(INT32*)(target+1)=(INT32)((BYTE*)hook-(target+5));for(DWORD i=5;i<n;++i)target[i]=0x90;pFlushInstructionCache(pGetCurrentProcess(),target,n);DWORD ignored=0;pVirtualProtect(target,n,oldp,&ignored);return tramp;}

// -----------------------------------------------------------------------------
// Buffered logging
// -----------------------------------------------------------------------------
static void update_highwater(UINT32 value){LONG old=g_queue_highwater;while((UINT32)old<value){LONG prev=__sync_val_compare_and_swap(&g_queue_highwater,old,(LONG)value);if(prev==old)break;old=prev;}}
static void count_drop(UINT32 type){__sync_add_and_fetch(&g_dropped_total,1);if(type==REC_PROCESSOR)__sync_add_and_fetch(&g_dropped_processor,1);else if(type==REC_AB9830)__sync_add_and_fetch(&g_dropped_curve,1);else __sync_add_and_fetch(&g_dropped_di,1);}
static void copy_record(LogRecordV6* dst,const LogRecordV6* src){volatile UINT32* d=(volatile UINT32*)dst;const volatile UINT32* s=(const volatile UINT32*)src;for(UINT32 i=0;i<(UINT32)(sizeof(LogRecordV6)/4);++i)d[i]=s[i];}
static BOOL enqueue_record(LogRecordV6* r){if(!g_capture_ready)return FALSE;tiny_lock(&g_queue_lock);UINT32 b=g_active_block;if(g_block_state[b]!=BLOCK_ACTIVE){count_drop(r->c.type);tiny_unlock(&g_queue_lock);return FALSE;}if(g_block_count[b]>=kQueueCapacity){UINT32 other=1U-b;if(g_block_state[other]==BLOCK_FREE){g_block_state[b]=BLOCK_READY;g_block_state[other]=BLOCK_ACTIVE;g_block_count[other]=0;g_active_block=other;b=other;}else{count_drop(r->c.type);tiny_unlock(&g_queue_lock);return FALSE;}}r->c.sequence=(UINT32)__sync_add_and_fetch(&g_sequence,1);copy_record(&g_blocks[b][g_block_count[b]],r);++g_block_count[b];update_highwater(g_block_count[b]);__sync_add_and_fetch(&g_enqueued_total,1);if(g_block_count[b]>=kQueueCapacity){UINT32 other=1U-b;if(g_block_state[other]==BLOCK_FREE){g_block_state[b]=BLOCK_READY;g_block_state[other]=BLOCK_ACTIVE;g_block_count[other]=0;g_active_block=other;}}tiny_unlock(&g_queue_lock);return TRUE;}
static BOOL write_exact(const void* data,DWORD bytes){DWORD wrote=0;BOOL ok=WriteFile(g_log,data,bytes,&wrote,0);if(!ok||wrote!=bytes){__sync_add_and_fetch(&g_write_errors,1);return FALSE;}return TRUE;}

// -----------------------------------------------------------------------------
// Module cache + caller classification
// -----------------------------------------------------------------------------
static int find_module_index(UINT32 base){LONG count=g_module_count;for(LONG i=0;i<count;++i)if(g_modules[i].base==base)return(int)i;return-1;}
static void common_fill(LogRecordV6* r,UINT32 type,void* return_address){zero_bytes(r,sizeof(*r));r->c.qpc_begin=qpc_now();r->c.type=type;r->c.thread_id=pGetCurrentThreadId?pGetCurrentThreadId():0;r->c.caller_address=(UINT32)(ULONG_PTR)return_address;UINT32 a=r->c.caller_address;LONG count=g_module_count;for(LONG i=0;i<count;++i){UINT32 b=g_modules[i].base,s=g_modules[i].size;if(a>=b&&a-b<s){r->c.caller_module_base=b;r->c.caller_rva=a-b;break;}}}
static void emit_module_record(const ModuleInfoV6* m){LogRecordV6 r;common_fill(&r,REC_MODULE,0);r.c.caller_address=r.c.caller_module_base=r.c.caller_rva=0;r.p.module.module_base=m->base;r.p.module.module_size=m->size;copy_string(r.p.module.module_name,sizeof(r.p.module.module_name),m->name);copy_string(r.p.module.module_path,sizeof(r.p.module.module_path),m->path);r.c.qpc_end=qpc_now();enqueue_record(&r);}
static void cache_module(UINT32 base,UINT32 size,const char* name,const char* path){if(!base||!size)return;tiny_lock(&g_module_lock);if(find_module_index(base)>=0){tiny_unlock(&g_module_lock);return;}LONG n=g_module_count;if(n>=128){tiny_unlock(&g_module_lock);return;}g_modules[n].base=base;g_modules[n].size=size;copy_string(g_modules[n].name,sizeof(g_modules[n].name),name);copy_string(g_modules[n].path,sizeof(g_modules[n].path),path);__sync_synchronize();g_module_count=n+1;ModuleInfoV6 copy=g_modules[n];tiny_unlock(&g_module_lock);emit_module_record(&copy);}
static const char* module_name_for_base(UINT32 base){int i=find_module_index(base);return i>=0?g_modules[i].name:"unknown";}
static void classify_address(UINT32 addr,UINT32* base,UINT32* rva){*base=0;*rva=0;LONG count=g_module_count;for(LONG i=0;i<count;++i){UINT32 b=g_modules[i].base,s=g_modules[i].size;if(addr>=b&&addr-b<s){*base=b;*rva=addr-b;return;}}}
static void scan_modules(){HANDLE snap=pCreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pGetCurrentProcessId());if(snap==INVALID_HANDLE_VALUE)return;MODULEENTRY32A_X me;zero_bytes(&me,sizeof(me));me.dwSize=sizeof(me);if(pModule32FirstA(snap,&me)){do{cache_module((UINT32)(ULONG_PTR)me.modBaseAddr,me.modBaseSize,me.szModule,me.szExePath);}while(pModule32NextA(snap,&me));}CloseHandle(snap);}

// -----------------------------------------------------------------------------
// Safe read helper
// -----------------------------------------------------------------------------
static BOOL readable_span(const void* ptr,SIZE_T bytes){if(!ptr||bytes==0)return FALSE;const BYTE* p=(const BYTE*)ptr;SIZE_T remain=bytes;while(remain){MEMORY_BASIC_INFORMATION_X mbi;SIZE_T got=pVirtualQuery(p,&mbi,sizeof(mbi));if(got<sizeof(mbi)||mbi.State!=MEM_COMMIT||(mbi.Protect&PAGE_NOACCESS)||(mbi.Protect&PAGE_GUARD))return FALSE;const BYTE* end=(const BYTE*)mbi.BaseAddress+mbi.RegionSize;if(p>=end)return FALSE;SIZE_T avail=(SIZE_T)(end-p),step=avail<remain?avail:remain;p+=step;remain-=step;}return TRUE;}

// -----------------------------------------------------------------------------
// In-place vtable-slot observation helpers. The COM object's vptr is NEVER changed.
// Only selected 4-byte entries in the original vtable are patched after conflict checks.
// -----------------------------------------------------------------------------
extern "C" HRESULT WINAPI hook_DI8_CreateDevice(LPVOID,const GUID_X*,LPVOID*,LPVOID);
extern "C" HRESULT WINAPI hook_Mouse_SetDataFormat(LPVOID,const DIDATAFORMAT_X*);
extern "C" HRESULT WINAPI hook_Mouse_SetCooperativeLevel(LPVOID,HWND,DWORD);
extern "C" HRESULT WINAPI hook_Mouse_Acquire(LPVOID);
extern "C" HRESULT WINAPI hook_Mouse_Unacquire(LPVOID);
extern "C" HRESULT WINAPI hook_Mouse_GetDeviceState(LPVOID,DWORD,LPVOID);
extern "C" HRESULT WINAPI hook_Mouse_GetDeviceData(LPVOID,DWORD,DIDEVICEOBJECTDATA_X*,DWORD*,DWORD);

static VtablePatchV61* find_vtable_patch(LPVOID* vt,UINT32 kind){
    UINT32 n=g_vtable_patch_count;for(UINT32 i=0;i<n;++i)if(g_vtable_patches[i].vtable==vt&&g_vtable_patches[i].kind==kind)return &g_vtable_patches[i];return 0;
}
static VtablePatchV61* find_patch_for_self(LPVOID self,UINT32 kind){
    if(!self||!readable_span(self,sizeof(LPVOID)))return 0;LPVOID* vt=*(LPVOID**)self;if(!vt)return 0;return find_vtable_patch(vt,kind);
}
static MouseDeviceV61* find_mouse_device(LPVOID object){
    UINT32 n=g_mouse_device_count;for(UINT32 i=0;i<n;++i)if(g_mouse_devices_table[i].object==object)return &g_mouse_devices_table[i];return 0;
}
static UINT32 module_base_for_address(LPVOID p){UINT32 b=0,r=0;classify_address((UINT32)(ULONG_PTR)p,&b,&r);return b;}
static BOOL patch_vtable_slot(LPVOID* vt,UINT32 slot,LPVOID expected,LPVOID replacement){
    DWORD oldp=0;if(!vt||!pVirtualProtect(&vt[slot],sizeof(LPVOID),PAGE_READWRITE,&oldp))return FALSE;
    BOOL ok=FALSE;if(vt[slot]==expected){vt[slot]=replacement;__sync_synchronize();ok=(vt[slot]==replacement);}
    DWORD ignored=0;pVirtualProtect(&vt[slot],sizeof(LPVOID),oldp,&ignored);return ok;
}
static void emit_vtable_record(UINT32 rectype,LPVOID object,const VtablePatchV61* vp){
    if(!vp)return;LogRecordV6 r;common_fill(&r,rectype,0);r.c.caller_address=r.c.caller_module_base=r.c.caller_rva=0;
    r.p.vtable.object_ptr=(UINT32)(ULONG_PTR)object;r.p.vtable.vtable_ptr=(UINT32)(ULONG_PTR)vp->vtable;r.p.vtable.interface_kind=vp->kind;
    r.p.vtable.slot_count=vp->slot_count;r.p.vtable.owner_module_base=vp->owner_module_base;r.p.vtable.patched_mask=vp->patched_mask;r.p.vtable.conflict_mask=vp->conflict_mask;
    UINT32 n=vp->slot_count<16?vp->slot_count:16;for(UINT32 i=0;i<n;++i)r.p.vtable.slots[i]=(UINT32)(ULONG_PTR)vp->original[i];r.c.qpc_end=qpc_now();enqueue_record(&r);
}
static BOOL prepare_di8_vtable(LPVOID object){
    if(!object||!readable_span(object,sizeof(LPVOID)))return FALSE;LPVOID* vt=*(LPVOID**)object;if(!vt||!readable_span(vt,11*sizeof(LPVOID)))return FALSE;
    tiny_lock(&g_wrap_lock);VtablePatchV61* existing=find_vtable_patch(vt,1);if(existing){BOOL ok=(existing->patched_mask&(1U<<3))!=0;tiny_unlock(&g_wrap_lock);emit_vtable_record(REC_DI8_VTABLE,object,existing);return ok;}
    if(g_vtable_patch_count>=16){tiny_unlock(&g_wrap_lock);return FALSE;}UINT32 idx=g_vtable_patch_count;VtablePatchV61* vp=&g_vtable_patches[idx];zero_bytes(vp,sizeof(*vp));vp->vtable=vt;vp->kind=1;vp->slot_count=11;
    for(UINT32 i=0;i<11;++i)vp->original[i]=vt[i];vp->owner_module_base=module_base_for_address(vp->original[0]);__sync_synchronize();g_vtable_patch_count=idx+1;
    UINT32 target_module=module_base_for_address(vp->original[3]);if(!vp->owner_module_base||target_module!=vp->owner_module_base){vp->conflict_mask|=(1U<<3);}else if(patch_vtable_slot(vt,3,vp->original[3],(LPVOID)hook_DI8_CreateDevice)){vp->patched_mask|=(1U<<3);}else{vp->conflict_mask|=(1U<<3);}BOOL ok=(vp->patched_mask&(1U<<3))!=0;tiny_unlock(&g_wrap_lock);emit_vtable_record(REC_DI8_VTABLE,object,vp);return ok;
}
static MouseDeviceV61* add_mouse_device(LPVOID object){
    if(!object||!readable_span(object,sizeof(LPVOID)))return 0;LPVOID* vt=*(LPVOID**)object;if(!vt||!readable_span(vt,14*sizeof(LPVOID)))return 0;
    tiny_lock(&g_wrap_lock);MouseDeviceV61* md=find_mouse_device(object);if(!md){if(g_mouse_device_count>=16){tiny_unlock(&g_wrap_lock);return 0;}UINT32 mi=g_mouse_device_count;md=&g_mouse_devices_table[mi];zero_bytes(md,sizeof(*md));md->object=object;md->vtable=vt;md->x_offset=md->y_offset=md->z_offset=UINT_ERROR;__sync_synchronize();g_mouse_device_count=mi+1;__sync_add_and_fetch(&g_mouse_devices,1);}
    VtablePatchV61* vp=find_vtable_patch(vt,2);if(!vp){if(g_vtable_patch_count>=16){tiny_unlock(&g_wrap_lock);return md;}UINT32 idx=g_vtable_patch_count;vp=&g_vtable_patches[idx];zero_bytes(vp,sizeof(*vp));vp->vtable=vt;vp->kind=2;vp->slot_count=14;for(UINT32 i=0;i<14;++i)vp->original[i]=vt[i];vp->owner_module_base=module_base_for_address(vp->original[0]);__sync_synchronize();g_vtable_patch_count=idx+1;
        const UINT32 slots[6]={7,8,9,10,11,13};LPVOID hooks[6]={(LPVOID)hook_Mouse_Acquire,(LPVOID)hook_Mouse_Unacquire,(LPVOID)hook_Mouse_GetDeviceState,(LPVOID)hook_Mouse_GetDeviceData,(LPVOID)hook_Mouse_SetDataFormat,(LPVOID)hook_Mouse_SetCooperativeLevel};
        for(UINT32 k=0;k<6;++k){UINT32 slot=slots[k],target_module=module_base_for_address(vp->original[slot]);if(!vp->owner_module_base||target_module!=vp->owner_module_base){vp->conflict_mask|=(1U<<slot);continue;}if(patch_vtable_slot(vt,slot,vp->original[slot],hooks[k]))vp->patched_mask|=(1U<<slot);else vp->conflict_mask|=(1U<<slot);}
    }
    tiny_unlock(&g_wrap_lock);emit_vtable_record(REC_MOUSE_VTABLE,object,vp);return md;
}
static LPVOID original_slot_for_self(LPVOID self,UINT32 kind,UINT32 slot){VtablePatchV61* vp=find_patch_for_self(self,kind);if(!vp||slot>=vp->slot_count)return 0;return vp->original[slot];}

// -----------------------------------------------------------------------------
// DirectInput hooks
// -----------------------------------------------------------------------------
extern "C" HRESULT WINAPI hook_DirectInput8Create(HINSTANCE hinst,DWORD version,const GUID_X* riid,LPVOID* out,LPVOID punk){
    void* ret=__builtin_return_address(0);DWORD incoming_error=pGetLastError();LogRecordV6 r;common_fill(&r,REC_DI8_CREATE,ret);r.p.di8.call_id=(UINT32)__sync_add_and_fetch(&g_call_id,1);r.p.di8.hinst=(UINT32)(ULONG_PTR)hinst;r.p.di8.version=version;r.p.di8.riid_ptr=(UINT32)(ULONG_PTR)riid;r.p.di8.out_ptr=(UINT32)(ULONG_PTR)out;r.p.di8.punk_outer=(UINT32)(ULONG_PTR)punk;if(riid&&readable_span(riid,sizeof(GUID_X)))r.p.di8.requested_iid=*riid;
    pSetLastError(incoming_error);HRESULT hr=g_real_DirectInput8Create(hinst,version,riid,out,punk);DWORD api_error=pGetLastError();r.p.di8.result_hr=(UINT32)hr;r.p.di8.last_error=api_error;LPVOID object=(hr_success(hr)&&out&&readable_span(out,sizeof(LPVOID)))?*out:0;r.p.di8.object_ptr=(UINT32)(ULONG_PTR)object;r.p.di8.slot3_patched=object&&prepare_di8_vtable(object)?1U:0U;r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_di8create_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;
}

extern "C" HRESULT WINAPI hook_DI8_CreateDevice(LPVOID self,const GUID_X* rguid,LPVOID* out,LPVOID punk){
    void* ret=__builtin_return_address(0);DWORD incoming_error=pGetLastError();DI8CreateDeviceFn original=(DI8CreateDeviceFn)original_slot_for_self(self,1,3);if(!original){pSetLastError(incoming_error);return (HRESULT)0x80004005L;}
    UINT32 call_id=(UINT32)__sync_add_and_fetch(&g_call_id,1);LogRecordV6 enter;common_fill(&enter,REC_CREATE_DEVICE_ENTER,ret);enter.p.create_device.call_id=call_id;enter.p.create_device.di8_ptr=(UINT32)(ULONG_PTR)self;enter.p.create_device.guid_ptr=(UINT32)(ULONG_PTR)rguid;enter.p.create_device.out_ptr=(UINT32)(ULONG_PTR)out;enter.p.create_device.punk_outer=(UINT32)(ULONG_PTR)punk;BOOL mouse=FALSE;if(rguid&&readable_span(rguid,sizeof(GUID_X))){enter.p.create_device.device_guid=*rguid;mouse=is_mouse_guid(rguid);}enter.p.create_device.is_mouse_guid=mouse?1U:0U;enter.c.qpc_end=qpc_now();enqueue_record(&enter);
    LogRecordV6 r=enter;r.c.type=REC_DI_CREATE_DEVICE;r.c.qpc_begin=qpc_now();pSetLastError(incoming_error);HRESULT hr=original(self,rguid,out,punk);DWORD api_error=pGetLastError();LPVOID dev=(hr_success(hr)&&out&&readable_span(out,sizeof(LPVOID)))?*out:0;r.p.create_device.result_hr=(UINT32)hr;r.p.create_device.last_error=api_error;r.p.create_device.device_ptr=(UINT32)(ULONG_PTR)dev;r.p.create_device.vtable_patched=0;if(mouse&&dev){MouseDeviceV61* md=add_mouse_device(dev);if(md){VtablePatchV61* mvp=find_vtable_patch(md->vtable,2);const UINT32 need=(1U<<7)|(1U<<8)|(1U<<9)|(1U<<10)|(1U<<11)|(1U<<13);if(mvp&&(mvp->patched_mask&need)==need)r.p.create_device.vtable_patched=1;}}r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_create_device_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;
}

extern "C" HRESULT WINAPI hook_Mouse_SetDataFormat(LPVOID self,const DIDATAFORMAT_X* fmt){
    DWORD incoming_error=pGetLastError();DIDSetDataFormatFn original=(DIDSetDataFormatFn)original_slot_for_self(self,2,11);MouseDeviceV61* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return (HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self,fmt);}void* ret=__builtin_return_address(0);LogRecordV6 r;common_fill(&r,REC_DI_SET_DATA_FORMAT,ret);r.p.setformat.device_ptr=(UINT32)(ULONG_PTR)self;r.p.setformat.format_ptr=(UINT32)(ULONG_PTR)fmt;UINT32 x=UINT_ERROR,y=UINT_ERROR,z=UINT_ERROR,kind=0,captured=0;BOOL readable=fmt&&readable_span(fmt,sizeof(DIDATAFORMAT_X));if(readable){r.p.setformat.dw_size=fmt->dwSize;r.p.setformat.dw_obj_size=fmt->dwObjSize;r.p.setformat.dw_flags=fmt->dwFlags;r.p.setformat.dw_data_size=fmt->dwDataSize;r.p.setformat.dw_num_objs=fmt->dwNumObjs;r.p.setformat.format_readable=1;if(fmt->rgodf&&fmt->dwObjSize>=sizeof(DIOBJECTDATAFORMAT_X)&&fmt->dwNumObjs<=128&&readable_span(fmt->rgodf,(SIZE_T)fmt->dwObjSize*fmt->dwNumObjs)){for(DWORD i=0;i<fmt->dwNumObjs;++i){const DIOBJECTDATAFORMAT_X* o=(const DIOBJECTDATAFORMAT_X*)((const BYTE*)fmt->rgodf+(SIZE_T)i*fmt->dwObjSize);UINT32 gk=0;if(o->pguid&&readable_span(o->pguid,sizeof(GUID_X)))gk=guid_kind(o->pguid);if(gk==1)x=o->dwOfs;else if(gk==2)y=o->dwOfs;else if(gk==3)z=o->dwOfs;if(captured<16){r.p.setformat.objects[captured].guid_kind=gk;r.p.setformat.objects[captured].offset=o->dwOfs;r.p.setformat.objects[captured].type=o->dwType;r.p.setformat.objects[captured].flags=o->dwFlags;++captured;}}}}
    if(readable){if(fmt->dwDataSize==sizeof(DIMOUSESTATE_X)&&x==0&&y==4&&z==8)kind=1;else if(fmt->dwDataSize==sizeof(DIMOUSESTATE2_X)&&x==0&&y==4&&z==8)kind=2;else if(x!=UINT_ERROR&&y!=UINT_ERROR&&x+4<=fmt->dwDataSize&&y+4<=fmt->dwDataSize)kind=3;}
    r.p.setformat.axis_x_offset=x;r.p.setformat.axis_y_offset=y;r.p.setformat.axis_z_offset=z;r.p.setformat.format_kind=kind;r.p.setformat.captured_objects=captured;pSetLastError(incoming_error);HRESULT hr=original(self,fmt);DWORD api_error=pGetLastError();r.p.setformat.result_hr=(UINT32)hr;r.p.setformat.last_error=api_error;if(hr_success(hr)&&readable){md->format_kind=kind;md->x_offset=x;md->y_offset=y;md->z_offset=z;md->data_size=fmt->dwDataSize;}r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_setformat_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;
}
extern "C" HRESULT WINAPI hook_Mouse_SetCooperativeLevel(LPVOID self,HWND hwnd,DWORD flags){DWORD incoming_error=pGetLastError();DIDSetCooperativeLevelFn original=(DIDSetCooperativeLevelFn)original_slot_for_self(self,2,13);MouseDeviceV61* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self,hwnd,flags);}void* ret=__builtin_return_address(0);LogRecordV6 r;common_fill(&r,REC_DI_SET_COOP,ret);r.p.coop.device_ptr=(UINT32)(ULONG_PTR)self;r.p.coop.hwnd=(UINT32)(ULONG_PTR)hwnd;r.p.coop.flags=flags;pSetLastError(incoming_error);HRESULT hr=original(self,hwnd,flags);DWORD api_error=pGetLastError();r.p.coop.result_hr=(UINT32)hr;r.p.coop.last_error=api_error;r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_setcoop_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;}
extern "C" HRESULT WINAPI hook_Mouse_Acquire(LPVOID self){DWORD incoming_error=pGetLastError();DIDAcquireFn original=(DIDAcquireFn)original_slot_for_self(self,2,7);MouseDeviceV61* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self);}void* ret=__builtin_return_address(0);LogRecordV6 r;common_fill(&r,REC_DI_ACQUIRE,ret);r.p.acquire.device_ptr=(UINT32)(ULONG_PTR)self;pSetLastError(incoming_error);HRESULT hr=original(self);DWORD api_error=pGetLastError();r.p.acquire.result_hr=(UINT32)hr;r.p.acquire.last_error=api_error;r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_acquire_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;}
extern "C" HRESULT WINAPI hook_Mouse_Unacquire(LPVOID self){DWORD incoming_error=pGetLastError();DIDUnacquireFn original=(DIDUnacquireFn)original_slot_for_self(self,2,8);MouseDeviceV61* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self);}void* ret=__builtin_return_address(0);LogRecordV6 r;common_fill(&r,REC_DI_UNACQUIRE,ret);r.p.acquire.device_ptr=(UINT32)(ULONG_PTR)self;pSetLastError(incoming_error);HRESULT hr=original(self);DWORD api_error=pGetLastError();r.p.acquire.result_hr=(UINT32)hr;r.p.acquire.last_error=api_error;r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_unacquire_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;}
extern "C" HRESULT WINAPI hook_Mouse_GetDeviceState(LPVOID self,DWORD cb,LPVOID data){DWORD incoming_error=pGetLastError();DIDGetDeviceStateFn original=(DIDGetDeviceStateFn)original_slot_for_self(self,2,9);MouseDeviceV61* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self,cb,data);}void* ret=__builtin_return_address(0);LogRecordV6 r;common_fill(&r,REC_DI_GET_STATE,ret);r.p.state.device_ptr=(UINT32)(ULONG_PTR)self;r.p.state.cb_data=cb;r.p.state.data_ptr=(UINT32)(ULONG_PTR)data;r.p.state.format_kind=md->format_kind;r.p.state.x_offset=md->x_offset;r.p.state.y_offset=md->y_offset;r.p.state.z_offset=md->z_offset;pSetLastError(incoming_error);HRESULT hr=original(self,cb,data);DWORD api_error=pGetLastError();r.p.state.result_hr=(UINT32)hr;r.p.state.last_error=api_error;if(hr_success(hr)&&data&&md->x_offset!=UINT_ERROR&&md->y_offset!=UINT_ERROR&&md->x_offset+4<=cb&&md->y_offset+4<=cb&&readable_span(data,cb)){r.p.state.buffer_readable=1;r.p.state.x=*(const INT32*)((const BYTE*)data+md->x_offset);r.p.state.y=*(const INT32*)((const BYTE*)data+md->y_offset);if(md->z_offset!=UINT_ERROR&&md->z_offset+4<=cb)r.p.state.z=*(const INT32*)((const BYTE*)data+md->z_offset);r.p.state.valid_xyz=1;__sync_add_and_fetch(&g_getstate_mouse_packets,1);if(r.p.state.x||r.p.state.y)__sync_add_and_fetch(&g_getstate_nonzero,1);}r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_getstate_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;}
extern "C" HRESULT WINAPI hook_Mouse_GetDeviceData(LPVOID self,DWORD cbobj,DIDEVICEOBJECTDATA_X* data,DWORD* count,DWORD flags){DWORD incoming_error=pGetLastError();DIDGetDeviceDataFn original=(DIDGetDeviceDataFn)original_slot_for_self(self,2,10);MouseDeviceV61* md=find_mouse_device(self);if(!original){pSetLastError(incoming_error);return(HRESULT)0x80004005L;}if(!md){pSetLastError(incoming_error);return original(self,cbobj,data,count,flags);}void* ret=__builtin_return_address(0);LogRecordV6 r;common_fill(&r,REC_DI_GET_DATA_CALL,ret);UINT32 call_id=(UINT32)__sync_add_and_fetch(&g_call_id,1);r.p.data_call.call_id=call_id;r.p.data_call.device_ptr=(UINT32)(ULONG_PTR)self;r.p.data_call.cb_object_data=cbobj;r.p.data_call.data_ptr=(UINT32)(ULONG_PTR)data;r.p.data_call.count_ptr=(UINT32)(ULONG_PTR)count;r.p.data_call.flags=flags;r.p.data_call.format_kind=md->format_kind;r.p.data_call.x_offset=md->x_offset;r.p.data_call.y_offset=md->y_offset;r.p.data_call.z_offset=md->z_offset;if(count&&readable_span(count,sizeof(DWORD)))r.p.data_call.count_before=*count;pSetLastError(incoming_error);HRESULT hr=original(self,cbobj,data,count,flags);DWORD api_error=pGetLastError();r.p.data_call.result_hr=(UINT32)hr;r.p.data_call.last_error=api_error;DWORD n=0;if(count&&readable_span(count,sizeof(DWORD))){n=*count;r.p.data_call.count_after=n;}DWORD parse_n=n;if(parse_n>512){parse_n=512;r.p.data_call.truncated=1;}if(hr_success(hr)&&data&&cbobj>=16&&parse_n&&readable_span(data,(SIZE_T)cbobj*parse_n)){r.p.data_call.buffer_readable=1;for(DWORD i=0;i<parse_n;++i){const BYTE* pp=(const BYTE*)data+(SIZE_T)i*cbobj;UINT32 ofs=*(const UINT32*)(pp+0),raw=*(const UINT32*)(pp+4),ts=*(const UINT32*)(pp+8),seq=*(const UINT32*)(pp+12);UINT32 axis=0;if(ofs==md->x_offset)axis=1;else if(ofs==md->y_offset)axis=2;else if(ofs==md->z_offset)axis=3;if(axis){LogRecordV6 e;common_fill(&e,REC_DI_GET_DATA_AXIS,ret);e.p.data_axis.call_id=call_id;e.p.data_axis.device_ptr=(UINT32)(ULONG_PTR)self;e.p.data_axis.event_index=i;e.p.data_axis.object_offset=ofs;e.p.data_axis.raw_data=raw;e.p.data_axis.time_stamp=ts;e.p.data_axis.sequence=seq;e.p.data_axis.axis_kind=axis;e.p.data_axis.result_hr=(UINT32)hr;e.p.data_axis.delta=(INT32)raw;e.c.qpc_end=qpc_now();enqueue_record(&e);++r.p.data_call.axis_events;__sync_add_and_fetch(&g_getdata_axis_events,1);if((axis==1||axis==2)&&raw!=0)__sync_add_and_fetch(&g_getdata_nonzero,1);}++r.p.data_call.parsed_objects;}}
    r.c.qpc_end=qpc_now();__sync_add_and_fetch(&g_getdata_calls,1);enqueue_record(&r);pSetLastError(api_error);return hr;}

// -----------------------------------------------------------------------------
// Patch only deadspace3.exe's static DINPUT8!DirectInput8Create import.
// The current IAT target is preserved and called, so proxy loaders remain chained.
// -----------------------------------------------------------------------------
static BOOL patch_directinput8_import(){BYTE* base=g_base;DWORD e=*(DWORD*)(base+0x3C);BYTE* pe=base+e;BYTE* opt=pe+24;DWORD import_rva=*(DWORD*)(opt+0x68);if(!import_rva)return FALSE;BYTE* desc=base+import_rva;
    for(;*(DWORD*)desc||*(DWORD*)(desc+12)||*(DWORD*)(desc+16);desc+=20){DWORD oft=*(DWORD*)(desc+0),name_rva=*(DWORD*)(desc+12),ft=*(DWORD*)(desc+16);if(!name_rva||!ft)continue;const char* dll=(const char*)(base+name_rva);if(!streqi(dll,"DINPUT8.dll"))continue;DWORD* names=(DWORD*)(base+(oft?oft:ft));DWORD* iat=(DWORD*)(base+ft);for(DWORD i=0;names[i];++i){if(names[i]&0x80000000UL)continue;const char* fn=(const char*)(base+names[i]+2);if(!streq(fn,"DirectInput8Create"))continue;DWORD old=iat[i];if(!old)return FALSE;if(old==(DWORD)(ULONG_PTR)hook_DirectInput8Create){g_di_iat_patched=1;return TRUE;}DWORD oldp=0;if(!pVirtualProtect(&iat[i],sizeof(DWORD),PAGE_READWRITE,&oldp))return FALSE;g_real_DirectInput8Create=(DirectInput8CreateFn)(ULONG_PTR)old;iat[i]=(DWORD)(ULONG_PTR)hook_DirectInput8Create;DWORD ignored=0;pVirtualProtect(&iat[i],sizeof(DWORD),oldp,&ignored);g_di_iat_patched=1;LogRecordV6 r;common_fill(&r,REC_IAT,0);r.c.caller_address=r.c.caller_module_base=r.c.caller_rva=0;r.p.iat.slot_rva=(UINT32)((BYTE*)&iat[i]-base);r.p.iat.previous_target=old;classify_address(old,&r.p.iat.previous_target_module,&r.p.iat.previous_target_rva);r.p.iat.patched=1;r.c.qpc_end=qpc_now();enqueue_record(&r);return TRUE;}}
    return FALSE;
}

// -----------------------------------------------------------------------------
// Passive Raw Input registration monitor. Never registers or removes anything.
// -----------------------------------------------------------------------------
static UINT32 rawreg_hash(const RAWINPUTDEVICE_X* d,UINT32 n){UINT32 h=2166136261U;for(UINT32 i=0;i<n;++i){const UINT32 vals[4]={(UINT32)d[i].usUsagePage|((UINT32)d[i].usUsage<<16),d[i].dwFlags,(UINT32)(ULONG_PTR)d[i].hwndTarget,i};for(UINT32 j=0;j<4;++j){UINT32 v=vals[j];for(int b=0;b<4;++b){h^=(BYTE)(v>>(b*8));h*=16777619U;}}}return h;}
static void poll_raw_registration(){RAWINPUTDEVICE_X regs[64];UINT count=64;DWORD incoming=pGetLastError();UINT result=pGetRegisteredRawInputDevices(regs,&count,sizeof(RAWINPUTDEVICE_X));DWORD err=pGetLastError();pSetLastError(incoming);__sync_add_and_fetch(&g_rawreg_polls,1);UINT32 n=(result==UINT_ERROR)?0U:result;static UINT32 last_hash=0xFFFFFFFFU,last_n=0xFFFFFFFFU;UINT32 h=(result==UINT_ERROR)?0U:rawreg_hash(regs,n);BOOL changed=(h!=last_hash||n!=last_n);if(changed){last_hash=h;last_n=n;__sync_add_and_fetch(&g_rawreg_changes,1);g_rawreg_last_entries=(LONG)n;if(result==UINT_ERROR){LogRecordV6 r;common_fill(&r,REC_RAW_REG,0);r.c.caller_address=r.c.caller_module_base=r.c.caller_rva=0;r.p.rawreg.snapshot_id=(UINT32)g_rawreg_changes;r.p.rawreg.total=0;r.p.rawreg.result=result;r.p.rawreg.last_error=err;r.p.rawreg.changed=1;r.c.qpc_end=qpc_now();enqueue_record(&r);}else if(n==0){LogRecordV6 r;common_fill(&r,REC_RAW_REG,0);r.c.caller_address=r.c.caller_module_base=r.c.caller_rva=0;r.p.rawreg.snapshot_id=(UINT32)g_rawreg_changes;r.p.rawreg.total=0;r.p.rawreg.result=result;r.p.rawreg.changed=1;r.c.qpc_end=qpc_now();enqueue_record(&r);}else{for(UINT32 i=0;i<n&&i<64;++i){LogRecordV6 r;common_fill(&r,REC_RAW_REG,0);r.c.caller_address=r.c.caller_module_base=r.c.caller_rva=0;r.p.rawreg.snapshot_id=(UINT32)g_rawreg_changes;r.p.rawreg.index=i;r.p.rawreg.total=n;r.p.rawreg.usage_page=regs[i].usUsagePage;r.p.rawreg.usage=regs[i].usUsage;r.p.rawreg.flags=regs[i].dwFlags;r.p.rawreg.hwnd_target=(UINT32)(ULONG_PTR)regs[i].hwndTarget;r.p.rawreg.result=result;r.p.rawreg.changed=1;r.c.qpc_end=qpc_now();enqueue_record(&r);}}}}
static DWORD WINAPI monitor_thread(LPVOID){UINT32 raw_accum=0;while(TRUE){scan_modules();raw_accum+=kModuleRescanMs;if(raw_accum>=kRawRegPollMs){raw_accum=0;poll_raw_registration();}pSleep(kModuleRescanMs);}return 0;}

// -----------------------------------------------------------------------------
// Camera-path gate + AB9830 diagnostic hook. No values are modified.
// ABC510 is hooked only to mark calls originating from return RVA 0x0033F3ED;
// AB9830 records are emitted only while that per-thread gate is active and the
// direct AB9830 caller is the verified ABC510 callsite return RVA 0x006BC599.
// -----------------------------------------------------------------------------
extern "C" UINT32 __attribute__((fastcall)) hook_abc510(void* self,void*,UINT32 a1,UINT32 a2,UINT32 a3,UINT32 a4,UINT32 a5,UINT32 a6,UINT32 a7,UINT32 a8,UINT32 a9){
    UINT32 tid=pGetCurrentThreadId?pGetCurrentThreadId():0;
    UINT32 ret=(UINT32)(ULONG_PTR)__builtin_return_address(0);
    BOOL target=(g_base&&ret==(UINT32)(ULONG_PTR)(g_base+kTargetABC510ReturnRva));
    PathGateSlotV7* slot=0;
    if(target){slot=path_gate_slot(tid,TRUE);if(slot){__sync_add_and_fetch(&slot->depth,1);__sync_add_and_fetch(&g_abc510_target_calls,1);}}
    UINT32 result=g_abc510(self,a1,a2,a3,a4,a5,a6,a7,a8,a9);
    if(slot)__sync_sub_and_fetch(&slot->depth,1);
    return result;
}

extern "C" void __attribute__((fastcall)) hook_ab9830(void* self,void*,float* x,float* y,float curve0,float curve1){
    UINT32 tid=pGetCurrentThreadId?pGetCurrentThreadId():0;
    UINT32 ret=(UINT32)(ULONG_PTR)__builtin_return_address(0);
    __sync_add_and_fetch(&g_ab9830_calls,1);
    LONG depth=path_gate_depth(tid);
    BOOL targeted=(g_base&&depth>0&&ret==(UINT32)(ULONG_PTR)(g_base+kTargetAB9830ReturnRva));
    LogRecordV6 r;
    if(targeted){
        common_fill(&r,REC_AB9830,(void*)(ULONG_PTR)ret);
        r.p.ab9830.self_ptr=(UINT32)(ULONG_PTR)self;r.p.ab9830.x_ptr=(UINT32)(ULONG_PTR)x;r.p.ab9830.y_ptr=(UINT32)(ULONG_PTR)y;
        r.p.ab9830.direct_return_rva=kTargetAB9830ReturnRva;r.p.ab9830.parent_return_rva=kTargetABC510ReturnRva;r.p.ab9830.target_d820_return_rva=kTargetD820ReturnRva;r.p.ab9830.path_gate_depth=(UINT32)depth;
        r.p.ab9830.curve_arg0=curve0;r.p.ab9830.curve_arg1=curve1;r.p.ab9830.pre_x=x?*x:0.0f;r.p.ab9830.pre_y=y?*y:0.0f;
    }
    g_ab9830(self,x,y,curve0,curve1);
    if(targeted){r.c.qpc_end=qpc_now();r.p.ab9830.post_x=x?*x:0.0f;r.p.ab9830.post_y=y?*y:0.0f;__sync_add_and_fetch(&g_ab9830_targeted,1);enqueue_record(&r);}
}

// -----------------------------------------------------------------------------
// D820 hook unchanged
// -----------------------------------------------------------------------------
extern "C" BOOL __attribute__((fastcall)) hook_processor(void* self,void*,float dt,float* x,float* y){void* ret=__builtin_return_address(0);LogRecordV6 r;common_fill(&r,REC_PROCESSOR,ret);r.p.processor.self_ptr=(UINT32)(ULONG_PTR)self;r.p.processor.object_word_00=read_u32(self,0);r.p.processor.object_word_04=read_u32(self,4);r.p.processor.dt_bits=float_bits(dt);r.p.processor.dt=dt;r.p.processor.in_x=x?*x:0.0f;r.p.processor.in_y=y?*y:0.0f;snapshot_state(self,&r.p.processor.pre);BOOL result=g_processor(self,dt,x,y);r.c.qpc_end=qpc_now();r.p.processor.out_x=x?*x:0.0f;r.p.processor.out_y=y?*y:0.0f;snapshot_state(self,&r.p.processor.post);r.p.processor.result=result;__sync_add_and_fetch(&g_d820_records,1);enqueue_record(&r);return result;}

// -----------------------------------------------------------------------------
// Writer-side summaries
// -----------------------------------------------------------------------------
static void status_module_plus_rva(UINT32 base,UINT32 rva){status_text(module_name_for_base(base));status_text("+");status_hex32(rva);} 
static void status_address(UINT32 addr){UINT32 b=0,r=0;classify_address(addr,&b,&r);if(b)status_module_plus_rva(b,r);else status_hex32(addr);}
static void writer_process_record(const LogRecordV6* r){
    if(r->c.type==REC_IAT){status_text("DI8_IAT slot=deadspace3.exe+");status_hex32(r->p.iat.slot_rva);status_text(" previous=");status_module_plus_rva(r->p.iat.previous_target_module,r->p.iat.previous_target_rva);status_text(" patched=1\r\n");}
    else if(r->c.type==REC_DI8_CREATE){status_text("DirectInput8Create caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" hr=");status_hex32(r->p.di8.result_hr);status_text(" object=");status_hex32(r->p.di8.object_ptr);status_text(" slot3_patched=");status_dec(r->p.di8.slot3_patched);status_text("\r\n");}
    else if(r->c.type==REC_CREATE_DEVICE_ENTER){status_text("CREATEDEVICE_ENTER caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" self=");status_hex32(r->p.create_device.di8_ptr);status_text(" mouse_guid=");status_dec(r->p.create_device.is_mouse_guid);status_text("\r\n");}
    else if(r->c.type==REC_DI_CREATE_DEVICE){status_text("CreateDevice caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" mouse_guid=");status_dec(r->p.create_device.is_mouse_guid);status_text(" hr=");status_hex32(r->p.create_device.result_hr);status_text(" device=");status_hex32(r->p.create_device.device_ptr);status_text(" vtable_patched=");status_dec(r->p.create_device.vtable_patched);status_text("\r\n");}
    else if(r->c.type==REC_DI8_VTABLE){status_text("DI8_VTABLE object=");status_hex32(r->p.vtable.object_ptr);status_text(" vtable=");status_hex32(r->p.vtable.vtable_ptr);status_text(" owner=");status_text(module_name_for_base(r->p.vtable.owner_module_base));status_text("\r\nDI8_SLOT3 original=");status_address(r->p.vtable.slots[3]);status_text(" patched=");status_dec((r->p.vtable.patched_mask>>3)&1U);status_text(" conflict=");status_dec((r->p.vtable.conflict_mask>>3)&1U);status_text("\r\nDI8_SLOT4 EnumDevices=");status_address(r->p.vtable.slots[4]);status_text("\r\nDI8_SLOTS");for(UINT32 i=0;i<r->p.vtable.slot_count&&i<11;++i){status_text(" [");status_dec(i);status_text("]=");status_address(r->p.vtable.slots[i]);}status_text("\r\n");}
    else if(r->c.type==REC_MOUSE_VTABLE){status_text("MOUSE_VTABLE object=");status_hex32(r->p.vtable.object_ptr);status_text(" vtable=");status_hex32(r->p.vtable.vtable_ptr);status_text(" owner=");status_text(module_name_for_base(r->p.vtable.owner_module_base));status_text(" patched_mask=");status_hex32(r->p.vtable.patched_mask);status_text(" conflict_mask=");status_hex32(r->p.vtable.conflict_mask);status_text("\r\n");}
    else if(r->c.type==REC_DI_SET_DATA_FORMAT){status_text("MOUSE SetDataFormat caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" data_size=");status_dec(r->p.setformat.dw_data_size);status_text(" objs=");status_dec(r->p.setformat.dw_num_objs);status_text(" kind=");status_dec(r->p.setformat.format_kind);status_text(" xofs=");status_hex32(r->p.setformat.axis_x_offset);status_text(" yofs=");status_hex32(r->p.setformat.axis_y_offset);status_text(" hr=");status_hex32(r->p.setformat.result_hr);status_text("\r\n");}
    else if(r->c.type==REC_DI_GET_STATE&&r->p.state.valid_xyz&&(r->p.state.x||r->p.state.y)&&__sync_val_compare_and_swap(&g_status_di_confirmed,0,1)==0){status_text("DIRECTINPUT_MOUSE_CONFIRMED method=GetDeviceState caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text("\r\n");}
    else if(r->c.type==REC_DI_GET_DATA_AXIS&&(r->p.data_axis.axis_kind==1||r->p.data_axis.axis_kind==2)&&r->p.data_axis.delta!=0&&__sync_val_compare_and_swap(&g_status_di_confirmed,0,1)==0){status_text("DIRECTINPUT_MOUSE_CONFIRMED method=GetDeviceData caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text("\r\n");}
    else if(r->c.type==REC_RAW_REG&&r->p.rawreg.index==0){status_text("RAW_REG_PASSIVE entries=");status_dec(r->p.rawreg.total);status_text(" result=");status_hex32(r->p.rawreg.result);if(r->p.rawreg.total){status_text(" first usagePage=");status_dec(r->p.rawreg.usage_page);status_text(" usage=");status_dec(r->p.rawreg.usage);status_text(" flags=");status_hex32(r->p.rawreg.flags);status_text(" target=");status_hex32(r->p.rawreg.hwnd_target);}status_text("\r\n");}
    else if(r->c.type==REC_AB9830&&__sync_val_compare_and_swap(&g_status_curve_confirmed,0,1)==0){status_text("AB9830_CAMERA_PATH_CONFIRMED caller=");status_module_plus_rva(r->c.caller_module_base,r->c.caller_rva);status_text(" parent_return=");status_hex32(r->p.ab9830.parent_return_rva);status_text(" d820_return=");status_hex32(r->p.ab9830.target_d820_return_rva);status_text("\r\n");}
    else if(r->c.type==REC_PROCESSOR&&__sync_val_compare_and_swap(&g_status_processor_confirmed,0,1)==0)status_text("PROCESSOR_HOOK_CONFIRMED: D820 processor records are being captured.\r\n");
}
static void writer_periodic_summary(){long long now=qpc_now();if(!g_last_summary_qpc)g_last_summary_qpc=now;if(g_qpc_frequency>0&&now-g_last_summary_qpc<g_qpc_frequency*2)return;g_last_summary_qpc=now;status_text("DIRECTINPUT calls create8=");status_dec((UINT32)g_di8create_calls);status_text(" createDevice=");status_dec((UINT32)g_create_device_calls);status_text(" mouseDevices=");status_dec((UINT32)g_mouse_devices);status_text(" state=");status_dec((UINT32)g_getstate_calls);status_text(" stateNonzero=");status_dec((UINT32)g_getstate_nonzero);status_text(" data=");status_dec((UINT32)g_getdata_calls);status_text(" dataNonzero=");status_dec((UINT32)g_getdata_nonzero);status_text("\r\nD820 records=");status_dec((UINT32)g_d820_records);status_text(" AB9830 target=");status_dec((UINT32)g_ab9830_targeted);status_text(" RAW_REG entries=");status_dec((UINT32)g_rawreg_last_entries);status_text("\r\n");if(g_dropped_total>0&&__sync_val_compare_and_swap(&g_status_drop_reported,0,1)==0)status_text("WARNING: logger dropped records; decoder will report exact counters.\r\n");}
static void fill_stats_record(LogRecordV6* r){common_fill(r,REC_STATS,0);r->c.caller_address=r->c.caller_module_base=r->c.caller_rva=0;r->p.stats.enqueued_total=(UINT32)g_enqueued_total;r->p.stats.dropped_total=(UINT32)g_dropped_total;r->p.stats.dropped_di=(UINT32)g_dropped_di;r->p.stats.dropped_processor=(UINT32)g_dropped_processor;r->p.stats.queue_highwater=(UINT32)g_queue_highwater;r->p.stats.write_errors=(UINT32)g_write_errors;r->p.stats.di8create_calls=(UINT32)g_di8create_calls;r->p.stats.create_device_calls=(UINT32)g_create_device_calls;r->p.stats.mouse_devices=(UINT32)g_mouse_devices;r->p.stats.setformat_calls=(UINT32)g_setformat_calls;r->p.stats.setcoop_calls=(UINT32)g_setcoop_calls;r->p.stats.acquire_calls=(UINT32)g_acquire_calls;r->p.stats.unacquire_calls=(UINT32)g_unacquire_calls;r->p.stats.getstate_calls=(UINT32)g_getstate_calls;r->p.stats.getstate_mouse_packets=(UINT32)g_getstate_mouse_packets;r->p.stats.getstate_nonzero=(UINT32)g_getstate_nonzero;r->p.stats.getdata_calls=(UINT32)g_getdata_calls;r->p.stats.getdata_axis_events=(UINT32)g_getdata_axis_events;r->p.stats.getdata_nonzero=(UINT32)g_getdata_nonzero;r->p.stats.d820_records=(UINT32)g_d820_records;r->p.stats.modules_seen=(UINT32)g_module_count;r->p.stats.rawreg_polls=(UINT32)g_rawreg_polls;r->p.stats.rawreg_changes=(UINT32)g_rawreg_changes;r->p.stats.rawreg_last_entries=(UINT32)g_rawreg_last_entries;r->p.stats.di_iat_patched=(UINT32)g_di_iat_patched;r->p.stats.di_iat_conflict=(UINT32)g_di_iat_conflict;r->p.stats.dropped_curve=(UINT32)g_dropped_curve;r->p.stats.abc510_target_calls=(UINT32)g_abc510_target_calls;r->p.stats.ab9830_calls=(UINT32)g_ab9830_calls;r->p.stats.ab9830_targeted=(UINT32)g_ab9830_targeted;r->p.stats.ab9830_hook_installed=(UINT32)g_ab9830_hook_installed;r->c.qpc_end=qpc_now();}
static DWORD WINAPI writer_thread(LPVOID){while(TRUE){UINT32 ready=2,count=0;tiny_lock(&g_queue_lock);for(UINT32 i=0;i<2;++i)if(g_block_state[i]==BLOCK_READY){ready=i;break;}if(ready<2){g_block_state[ready]=BLOCK_WRITING;count=g_block_count[ready];}tiny_unlock(&g_queue_lock);if(ready==2){pSleep(kFlushIntervalMs);tiny_lock(&g_queue_lock);for(UINT32 i=0;i<2;++i)if(g_block_state[i]==BLOCK_READY){ready=i;break;}if(ready==2){UINT32 a=g_active_block,other=1U-a;if(g_block_state[a]==BLOCK_ACTIVE&&g_block_count[a]>0&&g_block_state[other]==BLOCK_FREE){g_block_state[a]=BLOCK_READY;g_block_state[other]=BLOCK_ACTIVE;g_block_count[other]=0;g_active_block=other;ready=a;}}if(ready<2){g_block_state[ready]=BLOCK_WRITING;count=g_block_count[ready];}tiny_unlock(&g_queue_lock);}if(ready<2&&count){for(UINT32 i=0;i<count;++i)writer_process_record(&g_blocks[ready][i]);write_exact(g_blocks[ready],(DWORD)(count*sizeof(LogRecordV6)));LogRecordV6 s;fill_stats_record(&s);write_exact(&s,sizeof(s));tiny_lock(&g_queue_lock);g_block_count[ready]=0;g_block_state[ready]=BLOCK_FREE;tiny_unlock(&g_queue_lock);writer_periodic_summary();}}}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------
static DWORD WINAPI init_thread(LPVOID){
    char status_path[1024],log_path[1024];build_path(status_path,1024,"DS3_DirectInputProbe.status.txt");build_path(log_path,1024,"DS3_DirectInputProbe.bin");
    g_status=CreateFileA(status_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);status_text("Dead Space 3 DirectInput + D820 + AB9830 Probe v0.7\r\n");status_text("MODE: diagnostic only; DirectInput, D820 and targeted AB9830 observation; no input replacement, no Raw Input registration, no WndProc subclassing.\r\n");status_text("LOGGER: DirectInput/D820 producer paths are memory-only; disk I/O is on a separate writer thread.\r\n");
    if(!resolve_apis()){status_text("REFUSED: required Win32 API resolution failed. Nothing hooked.\r\n");return 0;}
    g_base=(BYTE*)GetModuleHandleA(0);DWORD ts=0,image_size=0,text_rva=0,text_size=0;BYTE* text=0;if(!pe_info(g_base,&ts,&image_size,&text,&text_rva,&text_size)){status_text("REFUSED: invalid/non-x86 PE image. Nothing hooked.\r\n");return 0;}
    status_text("PE timestamp: ");status_hex32(ts);status_text("\r\nImage size: ");status_hex32(image_size);status_text("\r\n.text RVA: ");status_hex32(text_rva);status_text("\r\n");if(ts!=0x511E9327UL||(image_size!=0x012D4000UL&&image_size!=0x012D6000UL)){status_text("REFUSED: DS3 version/fingerprint mismatch. Nothing hooked.\r\n");return 0;}
    LARGE_INTEGER_X fq;fq.QuadPart=0;if(!pQueryPerformanceFrequency(&fq)||fq.QuadPart<=0){status_text("REFUSED: QueryPerformanceFrequency failed. Nothing hooked.\r\n");return 0;}g_qpc_frequency=fq.QuadPart;
    g_blocks[0]=(LogRecordV6*)pVirtualAlloc(0,kQueueCapacity*sizeof(LogRecordV6),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);g_blocks[1]=(LogRecordV6*)pVirtualAlloc(0,kQueueCapacity*sizeof(LogRecordV6),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!g_blocks[0]||!g_blocks[1]){status_text("REFUSED: cannot allocate buffered log blocks. Nothing hooked.\r\n");return 0;}g_block_state[0]=BLOCK_ACTIVE;g_block_state[1]=BLOCK_FREE;g_active_block=0;
    g_log=CreateFileA(log_path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(g_log==INVALID_HANDLE_VALUE){status_text("REFUSED: cannot create DS3_DirectInputProbe.bin. Nothing hooked.\r\n");return 0;}
    LogHeaderV6 h;zero_bytes(&h,sizeof(h));h.magic[0]='D';h.magic[1]='S';h.magic[2]='3';h.magic[3]='D';h.magic[4]='I';h.magic[5]='7';h.magic[6]='0';h.version=70;h.header_size=sizeof(h);h.record_size=sizeof(LogRecordV6);h.flags=15;h.qpc_frequency=fq.QuadPart;h.image_timestamp=ts;h.image_size=image_size;h.text_rva=text_rva;h.queue_capacity_records=kQueueCapacity;h.flush_interval_ms=kFlushIntervalMs;h.module_rescan_ms=kModuleRescanMs;h.rawreg_poll_ms=kRawRegPollMs;DWORD wrote=0;if(!WriteFile(g_log,&h,sizeof(h),&wrote,0)||wrote!=sizeof(h)){status_text("REFUSED: failed to write binary header.\r\n");return 0;}
    g_capture_ready=1;HANDLE wt=CreateThread(0,0,writer_thread,0,0,0);if(!wt){status_text("REFUSED: could not create writer thread.\r\n");return 0;}CloseHandle(wt);
    scan_modules();
    if(patch_directinput8_import()){status_text("DIRECTINPUT_IAT_ACTIVE: deadspace3.exe!DirectInput8Create import patched; original target is chained.\r\n");}else{g_di_iat_conflict=1;status_text("REFUSED_DIRECTINPUT: deadspace3.exe DINPUT8!DirectInput8Create import was not found/patched. D820 monitoring will still run.\r\n");}
    HANDLE mt=CreateThread(0,0,monitor_thread,0,0,0);if(mt)CloseHandle(mt);status_text("RAW_REG_MONITOR_ACTIVE: passive GetRegisteredRawInputDevices polling only; probe does not register Raw Input.\r\n");

    status_text("WAITING: scanning runtime-unpacked .text for D820 signature.\r\n");BYTE* target=0;DWORD matches=0;for(DWORD attempt=0;attempt<240;++attempt){target=find_processor(text,text_size,&matches);if(matches==1&&target)break;pSleep(250);}status_text("Processor signature matches: ");status_dec(matches);status_text("\r\n");
    UINT32 calls[32];DWORD call_count=0;UINT32 processor_rva=0;BOOL d820_ok=FALSE;if(matches==1&&target){processor_rva=(UINT32)(target-g_base);call_count=collect_direct_call_returns(text,text_size,target,calls,32);status_text("Processor RVA found by signature: ");status_hex32(processor_rva);status_text("\r\nDirect caller return RVAs found: ");status_dec(call_count);status_text("\r\n");static const BYTE pro[]={0x55,0x8B,0xEC,0x83,0xEC,0x14};if(expected_callsites(calls,call_count)&&bytes_equal(target,pro,sizeof(pro))){g_processor=(ProcessorFn)install_hook(target,pro,sizeof(pro),(LPVOID)hook_processor);d820_ok=g_processor!=0;}}

    DWORD abc_matches=0,curve_matches=0;BYTE* abc_target=find_abc510(text,text_size,&abc_matches);BYTE* curve_target=find_ab9830(text,text_size,&curve_matches);UINT32 abc_rva=abc_target?(UINT32)(abc_target-g_base):0;UINT32 curve_rva=curve_target?(UINT32)(curve_target-g_base):0;BOOL abc_ok=FALSE,curve_ok=FALSE;
    status_text("ABC510 signature matches: ");status_dec(abc_matches);status_text(" rva=");status_hex32(abc_rva);status_text("\r\nAB9830 signature matches: ");status_dec(curve_matches);status_text(" rva=");status_hex32(curve_rva);status_text("\r\n");
    if(abc_matches==1&&curve_matches==1&&abc_target&&curve_target){
        BYTE* camera_call=g_base+0x0033F3E8U;BYTE* curve_call=g_base+0x006BC594U;BOOL links_ok=(rel32_call_target(camera_call)==abc_target&&rel32_call_target(curve_call)==curve_target);
        status_text("CAMERA_PATH_LINKS verified=");status_dec(links_ok?1U:0U);status_text(" abc510_return=");status_hex32(kTargetABC510ReturnRva);status_text(" ab9830_return=");status_hex32(kTargetAB9830ReturnRva);status_text(" d820_return=");status_hex32(kTargetD820ReturnRva);status_text("\r\n");
        static const BYTE abc_pro[]={0x55,0x8B,0xEC,0x53,0x8B,0xD9};static const BYTE curve_pro[]={0x55,0x8B,0xEC,0xD9,0x45,0x10};
        if(links_ok&&bytes_equal(abc_target,abc_pro,sizeof(abc_pro))&&bytes_equal(curve_target,curve_pro,sizeof(curve_pro))){
            g_abc510=(ABC510Fn)install_hook(abc_target,abc_pro,sizeof(abc_pro),(LPVOID)hook_abc510);abc_ok=g_abc510!=0;
            if(abc_ok){g_ab9830=(AB9830Fn)install_hook(curve_target,curve_pro,sizeof(curve_pro),(LPVOID)hook_ab9830);curve_ok=g_ab9830!=0;}
        }
    }
    g_ab9830_hook_installed=curve_ok?1:0;
    status_text("ABC510_PATH_GATE ");status_text(abc_ok?"ACTIVE":"NOT_INSTALLED");status_text("\r\nAB9830_OBSERVER ");status_text(curve_ok?"ACTIVE":"NOT_INSTALLED");status_text("\r\n");

    LogRecordV6 meta;common_fill(&meta,REC_META,0);meta.c.caller_address=meta.c.caller_module_base=meta.c.caller_rva=0;meta.p.meta.processor_rva=processor_rva;meta.p.meta.signature_matches=matches;meta.p.meta.callsite_count=call_count;meta.p.meta.d820_hook_installed=d820_ok?1U:0U;meta.p.meta.di_iat_patched=(UINT32)g_di_iat_patched;meta.p.meta.raw_monitor_active=1;meta.p.meta.abc510_rva=abc_rva;meta.p.meta.abc510_signature_matches=abc_matches;meta.p.meta.abc510_hook_installed=abc_ok?1U:0U;meta.p.meta.ab9830_rva=curve_rva;meta.p.meta.ab9830_signature_matches=curve_matches;meta.p.meta.ab9830_hook_installed=curve_ok?1U:0U;meta.p.meta.target_abc510_return_rva=kTargetABC510ReturnRva;meta.p.meta.target_ab9830_return_rva=kTargetAB9830ReturnRva;meta.p.meta.target_d820_return_rva=kTargetD820ReturnRva;meta.c.qpc_end=qpc_now();enqueue_record(&meta);
    if(d820_ok)status_text("ACTIVE: D820 hook verified and installed. DirectInput observer remains active.\r\n");else status_text("WARNING: D820 hook was not installed; DirectInput observer remains active.\r\n");
    status_text("EXPECTED: DIRECTINPUT_MOUSE_CONFIRMED plus AB9830_CAMERA_PATH_CONFIRMED after real gameplay mouse movement.\r\n");return 0;
}

extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){g_module=module;DisableThreadLibraryCalls(module);HANDLE t=CreateThread(0,0,init_thread,0,0,0);if(t)CloseHandle(t);}return TRUE;}
