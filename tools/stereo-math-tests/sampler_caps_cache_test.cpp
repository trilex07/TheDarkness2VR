#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../src/sampler_caps_cache.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <type_traits>

static unsigned checks;
static void Check(bool ok, const char* text) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
int main() {
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
    std::printf("PASS: %u checks; actual caps cache, clamp, retry, lifetime generation and thread isolation\n", checks);
}
