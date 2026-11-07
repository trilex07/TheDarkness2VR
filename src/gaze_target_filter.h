#pragma once
#include <cmath>
#include <cstdint>

namespace d2vr_gaze_target {
struct Direction { float x=0,y=0,z=-1; };
inline float Dot(Direction a,Direction b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool Normalize(Direction& d){
 const float n=std::sqrt(Dot(d,d));
 if(!std::isfinite(n)||n<.5f||n>1.5f)return false;
 d={d.x/n,d.y/n,d.z/n};return true;
}
// Interaction rays only: never delay foveated rendering, menus or gun aim.
// Tracking-space filtering keeps a fixation steady while the head moves.
struct Filter {
 bool active=false,pending=false;
 uint64_t lastNow=0,lastSample=0,lastAccepted=0,pendingSince=0;
 Direction value{},candidate{};
 void Reset(){*this={};}
 bool Update(bool valid,uint64_t sampleMs,uint64_t now,Direction raw,Direction& out){
  if(now<lastNow)Reset();
  lastNow=now;
  valid=valid && sampleMs<=now && now-sampleMs<=100 && Normalize(raw);
  if(!valid)pending=false;
  if(active && (now<lastAccepted || now-lastAccepted>160))Reset();
  lastNow=now;
  if(valid && (!active || sampleMs>lastSample)){
   const uint64_t previous=lastSample;lastSample=sampleMs;
   if(!active){value=raw;active=true;lastAccepted=sampleMs;pending=false;}
   else if(Dot(value,raw)<.98480775f){ // >10 degrees: confirm a saccade, not one bad sample
    if(!pending || Dot(candidate,raw)<.99862953f){candidate=raw;pendingSince=sampleMs;pending=true;}
    else if(sampleMs>=pendingSince && sampleMs-pendingSince>=20){
     value=raw;lastAccepted=sampleMs;pending=false;
    }
   }else{
    pending=false;lastAccepted=sampleMs;
    if(Dot(value,raw)<.99998134f){ // 0.35-degree deadband around a fixation
     const float dt=float(sampleMs-previous);
     const float a=1.f-std::exp(-dt/20.f);
     Direction next{value.x+a*(raw.x-value.x),value.y+a*(raw.y-value.y),value.z+a*(raw.z-value.z)};
     if(Normalize(next))value=next;
    }
   }
  }
  if(!active)return false;
  out=value;return true;
 }
};
}
