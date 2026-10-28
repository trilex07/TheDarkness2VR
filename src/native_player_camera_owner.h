#pragma once
#include <cstdint>

namespace d2vr_player_camera {
// Read only at the owned 985D80 renderer callback. That native method holds
// renderer+460 as an optional FOV override and uses its +2A0 virtual getter at
// 985FCB. This edge is absent in observed gameplay; it is not a general player
// source. Keep admission strict until a current-view owner route is proven.
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
                   CameraOwner,InventoryReference,InventoryType,InventoryOwner,Matched};
template<class Read> Status Inspect(uint32_t renderer,Read read,Owner& result,uint32_t ownedCameraTable=0) {
    result={};Owner found;
    const auto word=[&](uint32_t object,uint32_t offset,uint32_t& value) {
        value=0;
        return object && !(object&3) && object<=UINT32_MAX-offset && read(object+offset,value);
    };
    const auto ref=[&](uint32_t object,uint32_t offset,uint32_t& value) {
        uint32_t reference=0;return word(object,offset,reference) && word(reference,0,value) && value;
    };
    uint32_t table=0,back=0;
    if(!ref(renderer,0x460,found.avatar))return Status::RendererReference;
    if(!word(found.avatar,0,table) || table!=0x1001630)return Status::AvatarType;
    if(!ref(found.avatar,0x62c,found.camera))return Status::CameraReference;
    if(!word(found.camera,0,table) || (table!=0xffc7c0 && (!ownedCameraTable || table!=ownedCameraTable)))return Status::CameraType;
    if(!ref(found.camera,0x30,back) || back!=found.avatar)return Status::CameraOwner;
    if(!ref(found.avatar,0x630,found.inventory))return Status::InventoryReference;
    if(!word(found.inventory,0,table) || table!=0xffca10)return Status::InventoryType;
    if(!ref(found.inventory,0x2a8,back) || back!=found.avatar)return Status::InventoryOwner;
    result=found;return Status::Matched;
}
}
