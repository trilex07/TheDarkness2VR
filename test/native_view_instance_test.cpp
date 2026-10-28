#define NOMINMAX
#include <cstdio>
#include <cstdlib>
#include "../src/native_view_instance.h"
#include "../src/native_camera_cover.h"
#define CHECK(x) do {if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static unsigned calls=0;
static void* observed=nullptr;
static void __fastcall Before(void* view,void*) {observed=view;++calls;SetLastError(123);}
static unsigned __declspec(naked) Original() {__asm mov eax,0x87654321 __asm ret}
static void* next=reinterpret_cast<void*>(&Original);
D2VR_SELECTED_CAMERA_SHIM(Entry,Before,next)
struct Instance {void** table;unsigned canary;};
int main() {
    void* source[83];for(auto& p:source)p=reinterpret_cast<void*>(&Original);
    source[0]=reinterpret_cast<void*>(0x8fffffff);
    void* before[83];std::memcpy(before,source,sizeof(source));
    d2vr_native_image::Guard guard{true,0x400000,0x100000};
    d2vr_native_view::InstanceTable table;
    CHECK(!table.Prepare(source+1,82,82,reinterpret_cast<void*>(&Entry),guard));
    CHECK(!table.Prepare(source+1,82,42,reinterpret_cast<void*>(&Entry),{}));
    CHECK(table.Prepare(source+1,82,42,reinterpret_cast<void*>(&Entry),guard));
    auto instance=static_cast<Instance*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    CHECK(instance);
    using Call=unsigned(__thiscall*)(void*);
    for(unsigned i=0;i<1000;++i) {
        instance->table=source+1;instance->canary=0xabcdef01;
        CHECK(table.Publish(instance,guard));CHECK(table.Owns(instance));
        CHECK(table.Publish(instance,guard));
        CHECK(instance->table[-1]==source[0]);
        for(unsigned j=0;j<82;++j)CHECK(instance->table[j]==(j==42?reinterpret_cast<void*>(&Entry):source[j+1]));
        MEMORY_BASIC_INFORMATION memory{};CHECK(VirtualQuery(instance->table,&memory,sizeof(memory)));
        CHECK(memory.Protect==PAGE_READONLY);
        CHECK(reinterpret_cast<Call>(instance->table[42])(instance)==0x87654321);
        CHECK(observed==instance && calls==i+1 && instance->canary==0xabcdef01);
        // Native destructor/reconstruction republishes its original table.
        instance->table=source+1;CHECK(!table.Owns(instance));
    }
    instance->table=nullptr;CHECK(!table.Publish(instance,guard));
    CHECK(!table.Publish(reinterpret_cast<void*>(0x400000),guard));
    CHECK(std::memcmp(before,source,sizeof(source))==0);
    VirtualFree(instance,0,MEM_RELEASE);
    CHECK(!table.Owns(instance)); // guarded stale observation, never restore it
    std::puts("PASS x86 instance table: 1000 publications/reconstructions, exact ECX/EAX, 82 slots+header, immutable clone, original bytes, guard and foreign-vptr rejection");
}
