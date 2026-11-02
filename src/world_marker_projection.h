#pragma once
#include <cmath>
namespace d2vr_marker {
inline bool Project(const float matrix[16],const float world[3],float ndc[2]) {
    float clip[4]{};
    for(int c=0;c<4;++c)clip[c]=world[0]*matrix[c]+world[1]*matrix[4+c]+world[2]*matrix[8+c]+matrix[12+c];
    for(float value:clip)if(!std::isfinite(value))return false;
    if(clip[3]<=1e-4f)return false;
    ndc[0]=clip[0]/clip[3];ndc[1]=clip[1]/clip[3];
    return std::isfinite(ndc[0]) && std::isfinite(ndc[1]);
}
// Native9F5390..53C9 converts NDC using viewport dimensions and movie scale.
// Preserve its origin, original Z and the script's arrow/text/edge-clamp logic.
inline bool Correct(const float native[16],const float rendered[16],const float world[3],
                    float uiX,float uiY,float width,float height,float scaleX,float scaleY,float output[3]) {
    const float inputs[]={uiX,uiY,width,height,scaleX,scaleY,output[0],output[1],output[2]};
    for(float value:inputs)if(!std::isfinite(value))return false;
    if(uiX<=0 || uiY<=0 || width<=0 || height<=0 || scaleX<=0 || scaleY<=0)return false;
    float before[2],after[2];
    if(!Project(native,world,before) || !Project(rendered,world,after))return false;
    const float x=output[0]+(after[0]/uiX-before[0])*.5f*width*scaleX;
    const float y=output[1]-(after[1]/uiY-before[1])*.5f*height*scaleY;
    if(!std::isfinite(x) || !std::isfinite(y))return false;
    output[0]=x;output[1]=y;return true;
}
}
