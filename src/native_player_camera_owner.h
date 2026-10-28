#pragma once
#include <cstdint>

namespace d2vr_player_camera {
// Read only at the owned 985D80 renderer callback. That native method holds
// renderer+460 as a smart reference and uses its +2A0 virtual FOV getter at
// 985FCB. This is not an object scanner or a globally retained native pointer.
struct Owner {uint32_t avatar=0,camera=0,inventory=0;};
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
