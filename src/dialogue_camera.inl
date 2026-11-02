#ifdef D2VR_MATCHED_CAPTURE
#include "dialogue_camera.h"
using DialogueAngleSetter=void(__thiscall*)(void*,const float*);
static std::atomic<DWORD> g_dialogueCamera{0};
static std::atomic<unsigned long long> g_dialogueBlocks{0};
static bool PrepareDialogueCamera() {
    if(!g_dialogueCameraComfort)return true;
    return d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xAF78F0),0x403,
        "d10d49e8c6fcdb00d221498649d19e57d5ecf4cb45038a933e58d68e799db46b") &&
        d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x9F2770),0x42,
        "59c42e1eb0d7939200e5e029ddd13cd99c85230d481751af7797cf7a50a338e7") &&
        d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x9D9120),0x9a,
        "9fc2b97094c608861c2672f4c73aa517fa70438f4e353b8d73ff98f8d3945a58");
}
static __declspec(noinline) void __fastcall DialogueCameraAngles(void* self,void*,const float* angles) {
    const auto direct=reinterpret_cast<uintptr_t>(_ReturnAddress());
    uintptr_t outer=0;
    // Read the additional stack word only for the pinned native wrapper.
    if(direct==0x9f278e)outer=reinterpret_cast<const uintptr_t*>(_AddressOfReturnAddress())[3];
    if(g_dialogueCameraComfort && d2vr_dialogue::ConversationCaller(direct,outer) &&
       g_dialogueCamera.load(std::memory_order_acquire)==DWORD(reinterpret_cast<uintptr_t>(self)) &&
       NativeAimOwnerMatches(self)) {
        const auto error=GetLastError();
        const auto n=++g_dialogueBlocks;
        if(n<=3 || n%600==0)
            Log("[DialogueCamera] blocked=%llu camera=%p caller=%08lX; native conversation rotation suppressed before composition",n,self,DWORD(outer));
        SetLastError(error);
        return;
    }
    reinterpret_cast<DialogueAngleSetter>(0x9D9120)(self,angles);
}
#endif
