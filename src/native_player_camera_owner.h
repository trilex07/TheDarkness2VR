#pragma once
#include <cstdint>

namespace d2vr_player_camera {
// RegionMgr::GetPlayingCinematic, FC7D00+1AC -> 6CF490, returns
// &manager[268]. D9B590 copies this smart reference through 59AB10.
// Inspect only its nullness while the native view callback owns the manager;
// never retain or dereference the cinematic object itself.
template<class Read> bool PlayingCinematic(uint32_t manager,Read read,bool& playing) {
    uint32_t table=0,reference=0,object=0;
    if(!manager || (manager&3) || manager>UINT32_MAX-0x268 ||
       !read(manager,table) || table!=0xfc7d00 ||
       !read(manager+0x268,reference) || !reference || (reference&3) ||
       !read(reference,object))return false;
    playing=object!=0;return true;
}
// The owned 985D80 callback preserves the selected RegionMgr receiver. Its
// separate local-player getter supplies a scoped native avatar reference.
// Renderer+460 remains only an optional FOV override, absent in observed play.
struct Owner {uint32_t avatar=0,camera=0,inventory=0;};
// Bounded rejection evidence for fields consumed by the pinned renderer.
// Never invoke an unknown getter or scan memory for a matching avatar type.
struct RendererTrace {
    uint32_t rendererTable=0,camera=0,contextReference=0,context=0,contextTable=0;
    uint32_t contextGetter=0,overrideReference=0,overrideAvatar=0,sceneReference=0;
};
template<class Read> RendererTrace TraceRenderer(uint32_t renderer,Read read) {
    RendererTrace out;
    const auto word=[&](uint32_t object,uint32_t offset,uint32_t& value) {
        value=0;
        return object && !(object&3) && object<=UINT32_MAX-offset && read(object+offset,value);
    };
    uint32_t reference=0;
    word(renderer,0,out.rendererTable);
    if(word(renderer,0x3f4,reference))word(reference,0,out.camera);
    if(word(renderer,0x3f8,out.contextReference) && word(out.contextReference,0,out.context) &&
       word(out.context,0,out.contextTable) && out.contextTable>=0x400000 && out.contextTable<0x11ec000-0xcc)
        word(out.contextTable,0xcc,out.contextGetter);
    if(word(renderer,0x460,out.overrideReference))word(out.overrideReference,0,out.overrideAvatar);
    word(renderer,0x4a8,out.sceneReference);
    return out;
}
enum class Status {RendererReference,AvatarType,CameraReference,CameraType,
                   CameraOwner,InventoryReference,InventoryType,InventoryOwner,Matched,
                   ViewContext,RegionManager,SceneReference,LocalPlayerReference};
// 985E6D: context+CC -> RegionMgr; 985F75: RegionMgr+174 -> scene.
// B0EF20 returns RegionMgr+1C8. Local player is the distinct +18C/964570
// getter. Never reinterpret the scene's pointer/layout as an avatar.
template<class Read> Status InspectViewContext(uint32_t renderer,uint32_t manager,
    uint32_t sceneReference,Read read) {
    const auto word=[&](uint32_t p,uint32_t offset,uint32_t& value) {
        value=0;return p && !(p&3) && p<=UINT32_MAX-offset && read(p+offset,value);
    };
    uint32_t ref=0,context=0,table=0,selected=0,scene=0;
    if(!word(renderer,0x3f8,ref) || !word(ref,0,context) ||
       !word(context,0,table) || table!=0xfc6d40 ||
       !word(context,0x64,ref) || !word(ref,0,selected) || selected!=manager)
        return Status::ViewContext;
    // Static table slots/code are pinned once at native owner admission.
    if(!word(manager,0,table) || table!=0xfc7d00)return Status::RegionManager;
    if(!word(sceneReference,0,scene) || !word(manager,0x1c8,ref) || scene!=ref)
        return Status::SceneReference;
    return Status::Matched;
}
template<class Read> Status InspectAvatar(uint32_t avatar,Read read,Owner& result,uint32_t ownedCameraTable=0) {
    result={};Owner found;found.avatar=avatar;
    const auto word=[&](uint32_t object,uint32_t offset,uint32_t& value) {
        value=0;
        return object && !(object&3) && object<=UINT32_MAX-offset && read(object+offset,value);
    };
    const auto ref=[&](uint32_t object,uint32_t offset,uint32_t& value) {
        uint32_t reference=0;return word(object,offset,reference) && word(reference,0,value) && value;
    };
    uint32_t table=0,back=0;
    if(!word(found.avatar,0,table) || table!=0x1001630)return Status::AvatarType;
    if(!ref(found.avatar,0x62c,found.camera))return Status::CameraReference;
    if(!word(found.camera,0,table) || (table!=0xffc7c0 && (!ownedCameraTable || table!=ownedCameraTable)))return Status::CameraType;
    if(!ref(found.camera,0x30,back) || back!=found.avatar)return Status::CameraOwner;
    if(!ref(found.avatar,0x630,found.inventory))return Status::InventoryReference;
    if(!word(found.inventory,0,table) || table!=0xffca10)return Status::InventoryType;
    if(!ref(found.inventory,0x2a8,back) || back!=found.avatar)return Status::InventoryOwner;
    result=found;return Status::Matched;
}
template<class Read> Status Inspect(uint32_t renderer,Read read,Owner& result,uint32_t ownedCameraTable=0) {
    result={};uint32_t reference=0,avatar=0;
    if(!renderer || (renderer&3) || renderer>UINT32_MAX-0x460 || !read(renderer+0x460,reference) ||
       !reference || (reference&3) || !read(reference,avatar) || !avatar)return Status::RendererReference;
    return InspectAvatar(avatar,read,result,ownedCameraTable);
}
template<class Read,class Get,class Release,class Consume>
Status WithSelectedLocalPlayer(uint32_t renderer,uint32_t manager,uint32_t sceneReference,
    Read read,Get get,Release release,Consume consume,uint32_t ownedCameraTable=0) {
    const auto context=InspectViewContext(renderer,manager,sceneReference,read);
    if(context!=Status::Matched)return context;
    // Native getter owns one strong reference, including its empty sentinel.
    // Release exactly once on every return and while unwinding consume().
    struct Held {uint32_t value=0;Release& release;~Held(){if(value)release(value);}} held{0,release};
    if(get(manager,&held.value)!=&held.value || !held.value)return Status::LocalPlayerReference;
    uint32_t avatar=0;
    if((held.value&3) || !read(held.value,avatar) || !avatar)return Status::LocalPlayerReference;
    Owner owner;
    const auto status=InspectAvatar(avatar,read,owner,ownedCameraTable);
    if(status==Status::Matched)consume(owner);
    return status;
}
}
