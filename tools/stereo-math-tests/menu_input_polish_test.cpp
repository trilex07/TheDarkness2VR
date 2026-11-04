#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <openxr/openxr.h>
#include "../../src/native_action_movies.h"
#include "../../src/unarmed_trigger_dpad.h"
using UINT=unsigned;using DWORD=unsigned;using HRESULT=long;
#define FAILED(x) ((x)<0)
struct D3DVIEWPORT9{float MinZ=0,MaxZ=.05f;};
struct IDirect3DDevice9{
 D3DVIEWPORT9 vp;bool fail=false;float m[16]={};int base=-1;
 HRESULT GetViewport(D3DVIEWPORT9* p){*p=vp;return fail?-1:0;}
 HRESULT GetVertexShaderConstantF(int b,float* p,UINT){base=b;std::memcpy(p,m,64);return fail?-1:0;}
};
static bool g_headGeometry=true,g_in3DGameplayMode=true,menu=true;
static int g_replayEye=-1;
static struct VS{int wvpBase=8;unsigned hash=123;}vs;
static VS* g_activeVs=&vs;
static bool NativeMenuPresentationActive(){return menu;}
static unsigned menuFlags=0;
static unsigned NativeVrMenuFlags(){return menuFlags;}
static bool g_nativeCameraCoverInstalled=false,g_nativeCameraCoverConfirmed=false;
static float g_nativeUiReferencePx=1,g_nativeUiReferencePy=1,g_nativeCoveredPx=1,g_nativeCoveredPy=1;
static void Log(const char*,...){}
#include "d2vr-menu-under-test.inl"
static unsigned checks=0;
static void Check(bool ok){++checks;if(!ok){std::printf("FAIL menu/input check %u\n",checks);std::exit(1);}}
int main(){
 using namespace d2vr_action_movies;
 Check(ClassifyMenu("PressStart.swf")==Title && ClassifyMenu("MainMenu.swf")==Main);
 Check(ClassifyMenu("Pause.swf")==Pause && ClassifyMenu("NewTalents.swf")==Talents);
 Check(ClassifyMenu("OptionsDisplay.swf")==Options);
 Check(!ClassifyMenu("QuickTimePopup.swf") && !ClassifyMenu("ConversationPopUp.swf"));
 Check(Classify("QuickTimePopup.swf")==Qte && ProtectRapidChoices(Story));
 float ux=.2f,uy=.3f;g_in3DGameplayMode=false;NativeCameraUiScale(ux,uy);Check(ux==1 && uy==1);
 g_in3DGameplayMode=true;menuFlags=Title;ux=.2f;uy=.3f;NativeCameraUiScale(ux,uy);Check(ux==1 && uy==1);
 menuFlags=Pause;ux=.2f;uy=.3f;NativeCameraUiScale(ux,uy);Check(std::fabs(ux-.31f)<.0001f && std::fabs(uy-.465f)<.0001f);
 menuFlags=0;menu=false;ux=.2f;uy=.3f;NativeCameraUiScale(ux,uy);Check(ux==.2f && uy==.3f);menu=true;
 for(int eye:{-1,1}){
  g_replayEye=eye;IDirect3DDevice9 d;d.m[11]=1;
  Check(HideMenuFirstPerson(&d) && d.base==8);
  d.vp.MaxZ=1;Check(!HideMenuFirstPerson(&d));d.vp.MaxZ=.05f;
  d.m[11]=0;Check(!HideMenuFirstPerson(&d)); // orthographic Flash must survive
  d.m[11]=std::numeric_limits<float>::quiet_NaN();Check(!HideMenuFirstPerson(&d));
  d.m[11]=1;menu=false;Check(!HideMenuFirstPerson(&d));menu=true;
  d.fail=true;Check(!HideMenuFirstPerson(&d));
 }
 // The wheel follows both translation and rotation, independently of the hand.
 for(float yaw:{0.f,.8f,-1.4f}){
  XrPosef h{{0,std::sin(yaw/2),0,std::cos(yaw/2)},{2,1.8f,-3}};
  const auto w=WeaponWheelPose(h);
  const auto inv=XrQuaternionf{-h.orientation.x,-h.orientation.y,-h.orientation.z,h.orientation.w};
  const auto local=RotVec(inv,w.position.x-h.position.x,w.position.y-h.position.y,w.position.z-h.position.z);
  Check(std::fabs(local.x-.10f)<.0001f && std::fabs(local.y+.04f)<.0001f && std::fabs(local.z+.55f)<.0001f);
  Check(std::memcmp(&w.orientation,&h.orientation,sizeof(h.orientation))==0);
 }
 using namespace d2vr_input;
 TentacleSwitchGuard guard;DoubleTapDpad tap;
 auto press=[&](uint64_t t,bool grip){uint16_t b=DoubleTapDpad::kX;tap.Update(b,8,!guard.Update(true,grip?1.f:0.f,false,t),t);return b;};
 Check(press(100,false)==DoubleTapDpad::kX);uint16_t b=0;tap.Update(b,8,true,120);
 Check(press(200,true)==DoubleTapDpad::kX); // never swallow a heart/choice press
 Check(guard.Update(true,0,false,300));Check(!guard.Update(true,0,false,451));
 b=0;tap.Update(b,8,true,460);Check(press(470,false)==DoubleTapDpad::kX); // blocked interval clears first tap
 Check(guard.Update(false,0,true,500));Check(guard.Update(false,0,false,749));Check(!guard.Update(false,0,false,750));
 std::printf("PASS %u actual-source menu, FP visibility, wheel pose and tentacle-switch checks\n",checks);
}
