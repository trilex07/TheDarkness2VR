#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../src/small_record_copy.h"
#include "../../src/native_constant_source.h"
#include "../../src/shader_register_history.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <initializer_list>
#include <thread>
#include <unknwn.h>
#include "../../src/frame_resource_refs.h"
using namespace d2vr_record_copy;
static_assert(sizeof(void*) == 4, "Exercise the production x86 ABI");
static unsigned readCalls = 0, probeCalls = 0, cases = 0;
static BOOL WINAPI CountRead(HANDLE p, LPCVOID src, LPVOID dst, SIZE_T n, SIZE_T* read) {
    ++readCalls;
    return ReadProcessMemory(p, src, dst, n, read);
}
static BOOL WINAPI WrongCount(HANDLE, LPCVOID, LPVOID dst, SIZE_T n, SIZE_T* read) {
    ++readCalls; memset(dst, 0xee, n); *read = n - 1; return TRUE;
}
static BOOL WINAPI FailedOutput(HANDLE, LPCVOID, LPVOID dst, SIZE_T n, SIZE_T* read) {
    ++readCalls; memset(dst, 0xee, n); *read = 1; return FALSE;
}
static bool PassProbe() { ++probeCalls; return true; }
static bool FailProbe() { ++probeCalls; return false; }
static DWORD Protection(const void* p) {
    MEMORY_BASIC_INFORMATION m{};
    assert(VirtualQuery(p, &m, sizeof(m)) == sizeof(m));
    return m.Protect;
}
static void Equal(const void* a, const void* b, size_t n) { assert(!memcmp(a, b, n)); ++cases; }
static void Filled(const uint8_t* p, size_t n, uint8_t value) {
    for (size_t i = 0; i < n; ++i) assert(p[i] == value);
    ++cases;
}
// Bind the extracted production RecPush and lock to this owned record arena.
#include "../../src/record_payload_storage.h"
static d2vr_record_payload::Storage g_recData;
static d2vr_shader_registers::History g_vsConstantHistory;
static std::atomic<LONG> g_recLock{0};
static bool g_smallRecordCopyAdmitted=false;
static std::atomic<bool> g_wideRecordCopy{false};
static RuntimeControl g_smallRecordCopyControl;
#include "actual_record_types.inl"
static std::vector<Rec> g_rec;
static bool g_recActive=true,g_recOverflow=false;
static uint64_t g_recCounts[40]{};
static d2vr_replay::FrameResourceRefs<IUnknown> g_recordRefs;
static void NoteRecordedWorldDraw(){} // draw classification tested by actual projection fixture
#include "actual_rec_push.inl"

static void PushRecord(const void* source,size_t bytes,const std::vector<uint8_t>& expected) {
    const auto previous=g_recData;
    const uint32_t offset=RecPush(source,bytes);
    assert(offset==previous.size() && g_recData.size()==previous.size()+expected.size());
    assert(g_recLock==0); // actual RecLockGuard must release on zero/clamped copies too
    Equal(g_recData.data(),previous.data(),previous.size());
    if(!expected.empty()) Equal(g_recData.data()+offset,expected.data(),expected.size());
    ++cases;
}
static void TestActualRecPush(uint8_t* memory,size_t page) {
    for(bool admitted:{false,true}) {
        g_smallRecordCopyAdmitted=admitted;
        g_recData.assign(37,0xa5);g_recData.shrink_to_fit();
        for(size_t bytes:{1u,16u,64u,191u,192u,193u,1088u}) {
            std::vector<uint8_t> expected(memory+64,memory+64+bytes);
            PushRecord(memory+64,bytes,expected);
        }
        // No dereference or append for empty/sanity-clamped native records.
        PushRecord(nullptr,0,{});
        PushRecord(reinterpret_cast<void*>(1),size_t(8)*1024*1024+1,{});
        PushRecord(reinterpret_cast<void*>(1),SIZE_MAX,{});
        PushRecord(nullptr,64,std::vector<uint8_t>(64,0));
        DWORD old=0;
        for(DWORD protection:{PAGE_NOACCESS,PAGE_READWRITE|PAGE_GUARD}) {
            std::vector<uint8_t> prefix(memory+page-16,memory+page);
            prefix.resize(64,0);
            assert(VirtualProtect(memory+page,page,protection,&old));
            PushRecord(memory+page,64,std::vector<uint8_t>(64,0));
            assert(Protection(memory+page)==protection);
            PushRecord(memory+page-16,64,prefix);
            assert(Protection(memory+page)==protection);
            PushRecord(memory+page,0,{});
            assert(Protection(memory+page)==protection);
            assert(VirtualProtect(memory+page,page,PAGE_READWRITE,&old));
        }
        // Exactly 8 MiB remains valid; only greater sizes trigger the clamp.
        std::vector<uint8_t> maximum(size_t(8)*1024*1024,0x5e);
        PushRecord(maximum.data(),maximum.size(),maximum);
        // Reuse dirty capacity across frames: a failed source must never expose
        // the preceding frame's bytes when resize no longer pre-clears them.
        for(unsigned frame=0;frame<32;++frame) {
            g_recData.clear();
            PushRecord(memory+64,1088,std::vector<uint8_t>(memory+64,memory+64+1088));
            g_recData.clear();
            PushRecord(nullptr,1088,std::vector<uint8_t>(1088,0));
        }
        g_recData.clear();g_recData.shrink_to_fit();
    }
}
static void TestAtomicPayloadRecords() {
    g_rec.clear();g_recData.clear();g_recOverflow=false;g_recActive=true;
    g_smallRecordCopyAdmitted=false;
    std::thread writers[4];
    for(unsigned t=0;t<4;++t) writers[t]=std::thread([t] {
        for(unsigned i=0;i<1000;++i) {
            const uint32_t id=t*1000+i;
            uint32_t payload[16];
            for(unsigned j=0;j<16;++j)payload[j]=id*31+j;
            RecEmitData(OP_SetPSConstF,payload,sizeof(payload),id,4);
        }
    });
    for(auto& writer:writers)writer.join();
    assert(g_rec.size()==4000 && g_recData.size()==4000*64 && !g_recOverflow);
    bool seen[4000]{};
    for(const auto& record:g_rec) {
        assert(record.op==OP_SetPSConstF && record.n==16 && record.a[1]==4);
        const auto id=record.a[0];assert(id<4000 && !seen[id]);seen[id]=true;
        assert(record.a[4]<=g_recData.size()-64);
        uint32_t payload[16];memcpy(payload,g_recData.data()+record.a[4],64);
        for(unsigned j=0;j<16;++j)assert(payload[j]==id*31+j);
        ++cases;
    }
    const auto bytes=g_recData.size();
    g_recActive=false;RecEmitData(OP_SetPSConstF,nullptr,64);
    assert(g_recData.size()==bytes && g_rec.size()==4000);
    g_recActive=true;g_rec.resize(400000);
    RecEmitData(OP_SetPSConstF,reinterpret_cast<void*>(1),64);
    assert(g_recOverflow && g_recData.size()==bytes && g_rec.size()==400000 && g_recLock==0);
    g_rec.clear();g_recData.clear();g_recOverflow=false;cases+=2;
    // Preserve the legacy command layouts while publishing each payload and
    // record under one lock. Prefix bytes make offset-zero mistakes visible.
    g_recData.resize(13);
    const uint32_t data[16]={1,2,3,4};
    RecEmitData(OP_Clear,data,32,11,22,33,44,{},4,2);
    RecEmitData(OP_SetTransform,data,64,7,0,0,0,{},1,0);
    RecEmitData(OP_SetViewport,data,24,0,0,0,0,{},0,0);
    RecEmitData(OP_SetScissorRect,data,16,0,0,0,0,{},0,1);
    assert(g_rec.size()==4 && g_recData.size()==149);
    assert(g_rec[0].op==OP_Clear && g_rec[0].n==2 && g_rec[0].a[4]==13);
    for(unsigned j=0;j<4;++j)assert(g_rec[0].a[j]==11*(j+1));
    assert(g_rec[1].op==OP_SetTransform && !g_rec[1].n && g_rec[1].a[0]==7 && g_rec[1].a[1]==45);
    assert(g_rec[2].op==OP_SetViewport && !g_rec[2].n && g_rec[2].a[0]==109);
    assert(g_rec[3].op==OP_SetScissorRect && g_rec[3].n==1 && g_rec[3].a[0]==133);
    for(unsigned i=0;i<4;++i) {
        const unsigned slots[]={4,1,0,0},sizes[]={32,64,24,16};
        assert(!memcmp(g_recData.data()+g_rec[i].a[slots[i]],data,sizes[i]));
    }
    g_rec.clear();g_recData.clear();cases+=9;
    // Actual production append publishes the shared raw register bank and its
    // reference receipt from the OWNED captured bytes under this same lock.
    g_vsConstantHistory.Clear();g_recActive=true;
    uint32_t matrix[16];for(unsigned i=0;i<16;++i)matrix[i]=i+101;
    d2vr_shader_registers::Reference reference;
    RecEmitData(OP_SetVSConstF,matrix,sizeof(matrix),0,4,0,0,{},4,UINT32_MAX,0,&reference);
    assert(reference.valid && !memcmp(reference.values,matrix,sizeof(matrix)));++cases;
    g_vsConstantHistory.BeginRecord();
    for(unsigned i=0;i<16;++i)matrix[i]+=20;
    RecEmitData(OP_SetVSConstF,matrix,sizeof(matrix),0,4,0,0,{},4,UINT32_MAX,0,&reference);
    assert(reference.valid && !memcmp(reference.values,matrix,sizeof(matrix)));++cases;
    g_vsConstantHistory.BeginReplay();float observed[16];
    assert(g_vsConstantHistory.replay.Matrix(0,observed));
    assert(memcmp(observed,matrix,sizeof(matrix))!=0);++cases;
    for(unsigned row=0;row<4;++row)RecEmitData(OP_SetVSConstF,matrix+row*4,16,row,1,0,0,{},4,UINT32_MAX,0,&reference);
    assert(reference.valid && !memcmp(reference.values,matrix,sizeof(matrix)));++cases;
    g_vsConstantHistory.live.Invalidate(0,1);
    RecEmitData(OP_SetVSConstF,matrix+4,16,1,1,0,0,{},4,UINT32_MAX,0,&reference);
    assert(!reference.valid);++cases;
    RecEmitData(OP_SetVSConstF,matrix,sizeof(matrix),0,4,0,0,{},4,UINT32_MAX,0,&reference);
    assert(reference.valid);++cases;
    g_recOverflow=true;
    RecEmitData(OP_SetVSConstF,matrix,16,0,1,0,0,{},4,UINT32_MAX,0,&reference);
    assert(!reference.valid && !g_vsConstantHistory.live.Matrix(0,observed));++cases;
    g_recOverflow=false;
    g_vsConstantHistory.Clear();
    float screen[16];for(unsigned i=0;i<16;++i)screen[i]=float(50+i);
    RecEmitData(OP_SetVSConstF,screen,sizeof(screen),61,4,0,0,{},4,UINT32_MAX,0,&reference,61);
    assert(reference.partialBase<0);++cases; // complete writes need no second snapshot
    float partial[4]={101,102,103,104};
    memcpy(screen+12,partial,sizeof(partial));
    const auto recordedAt=g_rec.size();
    RecEmitData(OP_SetVSConstF,partial,sizeof(partial),64,1,0,0,{},4,UINT32_MAX,0,&reference,61);
    assert(!reference.valid && reference.partialBase==61 && !memcmp(reference.partialValues,screen,sizeof(screen)));++cases;
    const auto& record=g_rec[recordedAt];
    assert(record.a[0]==64 && record.a[1]==1 && !memcmp(g_recData.data()+record.a[4],partial,sizeof(partial)));++cases;
    g_vsConstantHistory.live.Clear();
    assert(!memcmp(reference.partialValues,screen,sizeof(screen)));++cases; // owned per-call data
    RecEmitData(OP_SetVSConstF,partial,sizeof(partial),64,1,0,0,{},4,UINT32_MAX,0,&reference,61);
    assert(reference.partialBase<0);++cases; // no stale rows from a former upload
    g_recActive=false;reference.partialBase=61;
    RecEmitData(OP_SetVSConstF,partial,sizeof(partial),64,1,0,0,{},4,UINT32_MAX,0,&reference,61);
    assert(!reference.valid && reference.partialBase<0);++cases;
    g_rec.clear();g_recData.clear();
}
int main(int argc, char** argv) {
    assert(argc == 2);
    TestAtomicPayloadRecords();
    assert(GetFileAttributesA(argv[1]) != INVALID_FILE_ATTRIBUTES);
    // Use the production default, including an INI whose encoding is not recognized.
    assert(GetPrivateProfileIntA("Render", "SmallRecordCopy", 0, argv[1]) == 0);
    Admission off;
    assert(!off.Admit(false, PassProbe) && !off.Tested() && probeCalls == 0);
    Admission admitted;
    assert(admitted.Admit(true, PassProbe) && admitted.Tested() && probeCalls == 1);
    assert(admitted.Admit(true, FailProbe) && probeCalls == 1);
    assert(!admitted.Admit(false, FailProbe) && probeCalls == 1);
    Admission rejected;
    assert(!rejected.Admit(true, FailProbe) && probeCalls == 2);
    assert(!rejected.Admit(true, PassProbe) && probeCalls == 2); // failed probe stays rejected
    Admission native;
    assert(native.Admit(true)); // real own-page direct + cross-page guard probe on this test host

    SYSTEM_INFO info{}; GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    auto memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 3,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(memory && page > 1088);
    for (size_t i = 0; i < page * 3; ++i) memory[i] = uint8_t(i * 37 + 13);
    uint8_t output[1088]{}, expected[1088]{};
    Copy(nullptr, nullptr, 0, true, CountRead);
    Copy(output, nullptr, 0, false, CountRead);
    assert(readCalls == 0);

    for (bool enabled : {false, true}) for (size_t bytes : {1u, 16u, 64u, 191u, 192u, 193u, 1088u}) {
        memset(output, 0xcc, sizeof(output));
        const unsigned before = readCalls;
        Copy(output, memory + 64, bytes, enabled, CountRead);
        Equal(output, memory + 64, bytes);
        Filled(output + bytes, sizeof(output) - bytes, 0xcc);
        assert(readCalls == before + unsigned(enabled && bytes <= kSmallLimit));
    }
    const auto beforeNull = readCalls;
    memset(output, 0xcc, 64);
    Copy(output, nullptr, 64, true, CountRead);
    Filled(output, 64, 0);
    assert(readCalls == beforeNull + 1);

    // A success BOOL with a short count must never admit corrupted output.
    Copy(output, memory, 64, true, WrongCount);
    Equal(output, memory, 64);
    Copy(output, memory, 64, true, FailedOutput);
    Equal(output, memory, 64);
    auto rejectedCalls = readCalls;
    Copy(output, memory, 64, rejected.Admit(true, PassProbe), CountRead);
    Equal(output, memory, 64);
    assert(readCalls == rejectedCalls);

    DWORD old = 0;
    for (DWORD mode : {PAGE_READONLY, PAGE_EXECUTE, PAGE_EXECUTE_READ}) {
        memcpy(expected, memory, 64);
        assert(VirtualProtect(memory, page, mode, &old));
        Copy(output, memory, 64, true, CountRead);
        Equal(output, expected, 64);
        assert(Protection(memory) == mode);
        assert(VirtualProtect(memory, page, PAGE_READWRITE, &old));
    }
    for (DWORD mode : {PAGE_NOACCESS, PAGE_READWRITE | PAGE_GUARD}) {
        memcpy(expected, memory + page - 16, 16);
        memset(expected + 16, 0, 48);
        assert(VirtualProtect(memory + page, page, mode, &old));
        for (bool enabled : {false, true}) {
            memset(output, 0xcc, 64);
            auto before = readCalls;
            Copy(output, memory + page, 64, enabled, CountRead);
            Filled(output, 64, 0);
            assert(readCalls == before + unsigned(enabled));
            assert(Protection(memory + page) == mode);
            Copy(output, memory + page - 16, 64, enabled, CountRead);
            Equal(output, expected, 64);
            assert(Protection(memory + page) == mode);
        }
        // Failed RPM may have modified output: original readable-prefix policy still wins.
        Copy(output, memory + page - 16, 64, true, FailedOutput);
        Equal(output, expected, 64);
        assert(Protection(memory + page) == mode);
        assert(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    }

    // Copy only the requested arena slice; never change previous records or neighboring bytes.
    std::vector<uint8_t> arena(4096, 0xa5);
    const auto arenaSize = arena.size();
    const size_t offset = 2048;
    for (unsigned iteration = 0; iteration < 32; ++iteration) {
        for (size_t i = 0; i < 192; ++i) memory[i] = uint8_t(i * 17 + iteration);
        Copy(arena.data() + offset, memory, 192, true, CountRead);
        Equal(arena.data() + offset, memory, 192);
        assert(arena.size() == arenaSize);
        Filled(arena.data(), offset, 0xa5);
        Filled(arena.data() + offset + 192, arena.size() - offset - 192, 0xa5);
    }
    RuntimeControl control;
    assert(!control.Armed() && control.Effective(true) && !control.Effective(false));
    assert(!control.Request(0) && control.Effective(true));
    control.Configure(true);
    assert(control.Request(0) && !control.Effective(true));
    assert(!control.Request(0) && !control.Request(-1) && !control.Request(2));
    assert(control.Request(1) && control.Effective(true) && !control.Effective(false));
    TestActualRecPush(memory,page); // unarmed: identical legacy integration
    g_smallRecordCopyControl.Configure(true);
    g_smallRecordCopyControl.Request(0);
    TestActualRecPush(memory,page); // OFF, even when startup guard admitted ON
    g_smallRecordCopyControl.Request(1);
    TestActualRecPush(memory,page); // ON still cannot bypass failed admission
    g_wideRecordCopy=true;
    TestActualRecPush(memory,page); // actual integrated wide-copy mode
    for(size_t bytes:{193u,1088u,4096u,65535u,65536u,65537u}) {
        std::vector<uint8_t> src(bytes),dst(bytes+16,0xa5);
        for(size_t i=0;i<bytes;++i)src[i]=uint8_t(i*19);
        auto before=readCalls;
        Copy(dst.data(),src.data(),bytes,true,CountRead,kWideLimit);
        Equal(dst.data(),src.data(),bytes);
        Filled(dst.data()+bytes,16,0xa5);
        assert(readCalls==before+unsigned(bytes<=kWideLimit));
    }
    for(DWORD mode:{PAGE_NOACCESS,PAGE_READWRITE|PAGE_GUARD}) {
        assert(VirtualProtect(memory+page,page,mode,&old));
        std::vector<uint8_t> dst(page*3,0xa5),prefix(memory,memory+page);
        prefix.resize(page*3,0);
        Copy(dst.data(),memory,dst.size(),true,CountRead,kWideLimit);
        Equal(dst.data(),prefix.data(),prefix.size());
        assert(Protection(memory+page)==mode);
        Copy(dst.data(),memory,dst.size(),true,FailedOutput,kWideLimit);
        Equal(dst.data(),prefix.data(),prefix.size());
        assert(Protection(memory+page)==mode);
        assert(VirtualProtect(memory+page,page,PAGE_READWRITE,&old));
    }
    VirtualFree(memory, 0, MEM_RELEASE);
    std::printf("PASS small-record-copy: %u byte/range checks; x86 guard admission, exact counts, legacy fallback, actual RecPush offsets/append/bounds/sanity, and default OFF\n", cases);
}
