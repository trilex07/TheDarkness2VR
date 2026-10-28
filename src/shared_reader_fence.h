#pragma once
#include <cstdint>

// One query, owned by the existing shared texture lifetime. No COM ownership or
// worker thread: Ops is the same immediate context/render thread as the draw.
namespace d2vr_shared_reader {
enum class Poll { Ready, Pending, Failed };
struct Fence {
    bool pending=false, fault=false;
    const void* context=nullptr;
    const void* query=nullptr;
    uint32_t thread=0;
    template<class Ops> bool Matches(const Ops& ops) const {
        return !context || (context==ops.Context() && query==ops.Query() && thread==ops.Thread());
    }
    template<class Ops> bool CanRead(const Ops& ops) const {
        return ops.Valid() && !fault && !pending && Matches(ops);
    }
    template<class Ops> bool SealRead(Ops& ops) {
        // An unexpected second consumer must fail closed, never re-End a query
        // whose old result is still the required overwrite dependency.
        if(!CanRead(ops)) {fault=true;return false;}
        ops.End();
        context=ops.Context();query=ops.Query();thread=ops.Thread();pending=true;
        return true;
    }
    // A joined transfer worker has issued (or will issue before returning its
    // context lease) this exact query. Record its dependency without re-End.
    // Caller must join the worker before polling or reusing the context.
    template<class Ops> bool TrackIssuedRead(const Ops& ops) {
        if(!CanRead(ops)) {fault=true;return false;}
        context=ops.Context();query=ops.Query();thread=ops.Thread();pending=true;
        return true;
    }
    template<class Ops> static bool PollIssued(Ops& ops) {
        ops.Flush();
        const uint64_t start=ops.Now();
        Poll status;
        while((status=ops.GetData())==Poll::Pending) {
            if(ops.Now()-start>2000) return false;
            ops.PauseThread();
        }
        return status==Poll::Ready;
    }
    template<class Ops> bool Wait(bool lastConsumer,Ops& ops) {
        if(!ops.Valid()) return false;
        if(!lastConsumer) {
            // Original mode: issue at overwrite time, then Flush/poll exactly as
            // before. No new reader bookkeeping is consulted in this mode.
            ops.End();return PollIssued(ops);
        }
        if(fault || !Matches(ops)) return false;
        if(!pending) return true; // no shared read since creation/last completion
        if(!PollIssued(ops)) return false; // failure/timeout retain the dependency
        pending=false; // only successful completion permits another overwrite
        return true;
    }
    // Called after releasing the old query/texture lifetime (including Reset),
    // never to bypass a failed wait while keeping the old resource writable.
    void Reset() { *this=Fence{}; }
};
}
