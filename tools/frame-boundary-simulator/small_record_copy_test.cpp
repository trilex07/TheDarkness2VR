#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../src/small_record_copy.h"
#include "../../src/native_constant_source.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <initializer_list>
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
static volatile LONG g_recLock=0;
static bool g_smallRecordCopyAdmitted=false;
static std::atomic<bool> g_wideRecordCopy{false};
static RuntimeControl g_smallRecordCopyControl;
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
int main(int argc, char** argv) {
    assert(argc == 2);
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
