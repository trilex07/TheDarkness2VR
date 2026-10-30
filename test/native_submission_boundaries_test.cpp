#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "../src/recorder_sync.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using SubmissionQueryInterface=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,REFIID,void**);
using SubmissionSoftwareProcessing=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,BOOL);
static IDirect3DDevice9* device=reinterpret_cast<IDirect3DDevice9*>(0x1234);
static void* returned=device;
static HRESULT status=S_OK;
static unsigned queries=0,software=0;
static HRESULT STDMETHODCALLTYPE Query(IDirect3DDevice9* d,REFIID,void** out){CHECK(d==device);++queries;if(out)*out=returned;return status;}
static HRESULT STDMETHODCALLTYPE Software(IDirect3DDevice9* d,BOOL value){CHECK(d==device && value==TRUE);++software;return status;}
static SubmissionQueryInterface g_submissionQueryInterface=&Query;
static SubmissionSoftwareProcessing g_submissionSoftwareProcessing=&Software;
#include "actual_native_boundaries.inl"
int main(int argc,char** argv){
 CHECK(argc==2);d2vr_submission::requested=true;d2vr_submission::gate.Configure(true);
 void* out=nullptr;
 for(auto iid:{__uuidof(IUnknown),__uuidof(IDirect3DDevice9),__uuidof(IDirect3DDevice9Ex)}){
  status=S_FALSE;CHECK(SubmissionDeviceQueryInterface(device,iid,&out)==status && out==device);
  CHECK(d2vr_submission::gate.Enabled());
 }
 const GUID unknown={0x12345678,0xabcd,0xef01,{1,2,3,4,5,6,7,8}};
 status=E_NOINTERFACE;returned=reinterpret_cast<void*>(0x5678);
 CHECK(SubmissionDeviceQueryInterface(device,unknown,&out)==status && out==returned);
 CHECK(d2vr_submission::gate.Enabled() && queries==4);
 status=S_OK;
 if(!strcmp(argv[1],"unknown")){
  // Callback/raw scope is not permission to expose an untracked device alias.
  d2vr_submission::Epoch::Bypass raw(d2vr_submission::gate);
  d2vr_record_sync::DriverScope callback;
  CHECK(SubmissionDeviceQueryInterface(device,unknown,&out)==status && out==returned);
  CHECK(d2vr_submission::gate.Reasons()&d2vr_submission::Epoch::InterfaceEscape);
 }else if(!strcmp(argv[1],"alias")){
  CHECK(SubmissionDeviceQueryInterface(device,__uuidof(IDirect3DDevice9),&out)==status && out==returned);
  CHECK(d2vr_submission::gate.Reasons()&d2vr_submission::Epoch::InterfaceEscape);
 }else{
  CHECK(!strcmp(argv[1],"software"));status=S_FALSE;
  CHECK(SubmissionSetSoftwareProcessing(device,TRUE)==status && software==1);
  CHECK(d2vr_submission::gate.Reasons()&d2vr_submission::Epoch::UncontrolledMutation);
 }
 d2vr_submission::gate.Invalidate();CHECK(!d2vr_submission::gate.Enabled());
 printf("PASS actual native submission boundary: %s; exact HRESULT/output/call count and persistent denial\n",argv[1]);
}
