#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../src/sampler_caps_cache.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <type_traits>
#include <vector>

static unsigned checks;
static void Check(bool ok, const char* text) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
// Only GetDeviceCaps is exercised through the SDK's public slot 7. This is
// an owned test object with eight slots, never a production COM wrapper.
struct RuntimeDevice {
    void** table;
    unsigned queries=0,setters=0;
    DWORD maximum=16;
    HRESULT capsResult=D3D_OK,setResult=D3D_OK;
    DWORD sampler=0,value=0;D3DSAMPLERSTATETYPE type=D3DSAMP_MINFILTER;
};
static HRESULT STDMETHODCALLTYPE QueryRuntimeCaps(IDirect3DDevice9* dev,D3DCAPS9* caps) {
    auto& state=*reinterpret_cast<RuntimeDevice*>(dev);
    ++state.queries;caps->MaxAnisotropy=state.maximum;return state.capsResult;
}
static HRESULT STDMETHODCALLTYPE SetRuntimeSampler(IDirect3DDevice9* dev,DWORD sampler,D3DSAMPLERSTATETYPE type,DWORD value) {
    auto& state=*reinterpret_cast<RuntimeDevice*>(dev);
    ++state.setters;state.sampler=sampler;state.type=type;state.value=value;return state.setResult;
}
static int g_submitMode=4,g_textureAnisotropy=16;
static bool g_samplerCapsCacheEnabled=false,g_recActive=true;
static struct { decltype(&SetRuntimeSampler) SetSamplerState=&SetRuntimeSampler; } g_orig;
enum { OP_SetSamplerState=1 };
struct SamplerRecord {unsigned op,unused;DWORD sampler;D3DSAMPLERSTATETYPE type;DWORD value;};
static std::vector<SamplerRecord> records;
static void RecEmit(unsigned op,unsigned unused,DWORD sampler,D3DSAMPLERSTATETYPE type,DWORD value) {
    records.push_back({op,unused,sampler,type,value});
}
#include "actual_sampler_hook.inl"

static void TestActualABHook() {
    void* slots[8]{};slots[7]=reinterpret_cast<void*>(&QueryRuntimeCaps);
    RuntimeDevice state{slots};auto* dev=reinterpret_cast<IDirect3DDevice9*>(&state);
    auto call=[&](DWORD original,DWORD expected,D3DSAMPLERSTATETYPE type=D3DSAMP_MAXANISOTROPY) {
        records.clear();const unsigned prior=state.setters;
        const HRESULT hr=Hook_SetSamplerState(dev,5,type,original);
        Check(hr==state.setResult && state.setters==prior+1,"actual sampler setter forwarded once with original HRESULT");
        Check(state.sampler==5 && state.type==type && state.value==expected,"actual resolved sampler arguments unchanged");
        Check(records.size()==(g_recActive?1u:0u),"actual recording admission retained");
        if(g_recActive) Check(records[0].op==OP_SetSamplerState && records[0].unused==0 &&
            records[0].sampler==5 && records[0].type==type && records[0].value==expected,
            "recorded sampler value is exactly the native forwarded value");
    };
    // OFF uses the legacy query every time, including changes/failures; changing
    // the requested anisotropy still affects each individual setter.
    g_samplerCapsCacheEnabled=false;g_textureAnisotropy=4;
    for(unsigned i=0;i<128;++i) call(2,4);
    Check(state.queries==128,"OFF performs one caps query per eligible call");
    g_textureAnisotropy=12;call(2,12);
    state.maximum=4;call(2,4);
    state.capsResult=E_FAIL;state.setResult=D3DERR_INVALIDCALL;call(7,7);
    state.capsResult=D3D_OK;state.maximum=0;call(11,11);
    Check(state.queries==132,"OFF preserves failed/zero query behavior and frequency");
    g_submitMode=3;call(2,2);g_submitMode=4;
    call(D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DSAMP_MINFILTER);
    Check(state.queries==132,"OFF unrelated mode/state cannot query caps");

    g_samplerCapsCacheEnabled=true;state.maximum=16;state.setResult=S_FALSE;
    d2vr_sampling::InvalidateSamplerCaps();
    g_textureAnisotropy=4;call(2,4);
    g_textureAnisotropy=12;call(2,12);
    g_textureAnisotropy=2;call(8,8);
    for(unsigned i=0;i<128;++i) call(2,2);
    Check(state.queries==133,"ON queries once and applies current requested value on every call");
    state.maximum=4;d2vr_sampling::InvalidateSamplerCaps();call(2,2);
    g_textureAnisotropy=16;call(2,4);
    Check(state.queries==134,"ON lifecycle generation refreshes once, then reclamps current target");
    state.capsResult=E_FAIL;d2vr_sampling::InvalidateSamplerCaps();call(7,7);call(9,9);
    Check(state.queries==136,"ON failed caps retry without negative caching");
    state.capsResult=D3D_OK;state.maximum=0;call(6,6);call(7,7);
    Check(state.queries==138,"ON zero caps retry without caching");
    state.capsResult=S_FALSE;state.maximum=8;call(2,8);call(2,8);
    Check(state.queries==139,"ON successful positive nonzero HRESULT is cached");
    const auto beforeBypass=state.queries;g_submitMode=3;call(2,2);g_submitMode=4;
    call(D3DTEXF_POINT,D3DTEXF_POINT,D3DSAMP_MAGFILTER);
    Check(state.queries==beforeBypass,"ON unrelated mode/state cannot query caps");
    g_recActive=false;call(2,8);g_recActive=true;
    // Actual runtime overload (not a mirrored resolver) uses a separate TLS
    // entry on each device thread while retaining setter/recording semantics.
    d2vr_sampling::InvalidateSamplerCaps();const auto beforeThreads=state.queries;
    for(unsigned i=0;i<2;++i) {
        std::thread worker([&] {for(unsigned n=0;n<64;++n) call(2,8);});worker.join();
    }
    Check(state.queries==beforeThreads+2,"actual ON hook queries once per thread/device/generation");
    const auto beforeToggle=state.queries;g_samplerCapsCacheEnabled=false;
    call(2,8);call(2,8);
    Check(state.queries==beforeToggle+2,"switching OFF restores exact per-call queries");
}

int main() {
    TestActualABHook();
    using namespace d2vr_sampling;
    // Identity-only keys: the test query never dereferences these addresses.
    int identityA = 0, identityB = 0;
    auto* a = reinterpret_cast<IDirect3DDevice9*>(&identityA);
    auto* b = reinterpret_cast<IDirect3DDevice9*>(&identityB);
    unsigned queries = 0;
    DWORD cap = 16;
    HRESULT status = D3D_OK;
    IDirect3DDevice9* queried = nullptr;
    auto query = [&](IDirect3DDevice9* device, D3DCAPS9* caps) {
        ++queries; queried = device; caps->MaxAnisotropy = cap; return status;
    };
    static_assert(std::is_trivially_destructible<SamplerCapsCache>::value,
                  "the cache must not own or release a COM reference");
    SamplerCapsCache cache;
    for (unsigned i = 0; i < 2048; ++i)
        Check(cache.Resolve(a, 2, 8, query) == 8, "repeated values use positive cached cap");
    Check(queries == 1 && queried == a, "one query per device/thread/generation");
    Check(cache.Resolve(a, 32, 8, query) == 16, "original value capped at hardware limit");
    Check(cache.Resolve(a, 2, 32, query) == 16, "requested value capped at hardware limit");
    Check(cache.Resolve(a, 12, 8, query) == 12, "larger original budget preserved");
    Check(cache.Resolve(a, 0, 0, query) == 0, "zero budget preserved");
    Check(queries == 1, "clamping does not query again");

    cap = 4;
    InvalidateSamplerCaps();
    Check(cache.Resolve(a, 2, 8, query) == 4 && queries == 2,
          "generation invalidation refreshes same/reused pointer");
    cap = 2;
    Check(cache.Resolve(b, 2, 8, query) == 2 && queries == 3 && queried == b,
          "different device queries its own cap");
    cap = 8;
    Check(cache.Resolve(a, 2, 8, query) == 8 && queries == 4,
          "switching back never reuses another device cap");

    InvalidateSamplerCaps(); status = E_FAIL; cap = 64;
    Check(cache.Resolve(a, 3, 8, query) == 3 && queries == 5,
          "failed query preserves original value despite output bytes");
    status = D3D_OK; cap = 4;
    Check(cache.Resolve(a, 2, 8, query) == 4 && queries == 6,
          "failure is not cached; next call retries");
    InvalidateSamplerCaps(); cap = 0;
    Check(cache.Resolve(a, 3, 8, query) == 3 && queries == 7,
          "zero cap preserves original value");
    Check(cache.Resolve(a, 5, 8, query) == 5 && queries == 8, "zero cap is not cached");
    cap = 1;
    Check(cache.Resolve(a, 3, 8, query) == 1 && queries == 9, "cap one uses exact previous clamp");
    Check(cache.Resolve(a, 3, 8, query) == 1 && queries == 9, "positive cap one is cached");
    InvalidateSamplerCaps(); status = S_FALSE; cap = 8;
    Check(cache.Resolve(a, 2, 4, query) == 4 && queries == 10,
          "successful nonzero HRESULT preserves previous SUCCEEDED semantics");
    Check(cache.Resolve(nullptr, 7, 8, query) == 7 && queries == 10,
          "null identity cannot invoke query or reuse positive entry");

    SamplerCapsCache racing;
    unsigned raceQueries = 0;
    auto invalidatingQuery = [&](IDirect3DDevice9*, D3DCAPS9* caps) {
        caps->MaxAnisotropy = ++raceQueries == 1 ? 16 : 4;
        if (raceQueries == 1) InvalidateSamplerCaps();
        return D3D_OK;
    };
    Check(racing.Resolve(a, 2, 8, invalidatingQuery) == 8, "in-flight query preserves this call's clamp");
    Check(racing.Resolve(a, 2, 8, invalidatingQuery) == 4 && raceQueries == 2,
          "query crossing invalidation cannot publish stale cap");
    Check(racing.Resolve(a, 2, 8, invalidatingQuery) == 4 && raceQueries == 2,
          "subsequent stable query is cached");

    unsigned threadQueries[2] = {};
    DWORD threadResults[2] = {};
    for (unsigned i = 0; i < 2; ++i) {
        std::thread worker([&, i] {
            auto perThreadQuery = [&](IDirect3DDevice9*, D3DCAPS9* caps) {
                ++threadQueries[i]; caps->MaxAnisotropy = 4 + 4 * i; return D3D_OK;
            };
            for (unsigned n = 0; n < 1000; ++n)
                threadResults[i] = g_samplerCapsCache.Resolve(a, 2, 16, perThreadQuery);
        });
        worker.join();
    }
    Check(threadQueries[0] == 1 && threadQueries[1] == 1,
          "same identity is queried once independently on each thread");
    Check(threadResults[0] == 4 && threadResults[1] == 8, "thread entries do not alias");

    // A populated entry must observe another thread's invalidation.
    unsigned crossQueries = 0;
    DWORD before = 0, after = 0;
    std::thread owner([&] {
        auto perThreadQuery = [&](IDirect3DDevice9*, D3DCAPS9* caps) {
            caps->MaxAnisotropy = ++crossQueries == 1 ? 16 : 4; return D3D_OK;
        };
        before = g_samplerCapsCache.Resolve(a, 2, 8, perThreadQuery);
        std::thread invalidator([] { InvalidateSamplerCaps(); }); invalidator.join();
        after = g_samplerCapsCache.Resolve(a, 2, 8, perThreadQuery);
    });
    owner.join();
    Check(before == 8 && after == 4 && crossQueries == 2, "global generation invalidates every thread entry");
    std::printf("PASS: %u checks; actual OFF/ON sampler hook, caps cache, clamp, HRESULT, retry, lifetime generation and thread isolation\n", checks);
}
