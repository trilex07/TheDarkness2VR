// Local simulator-only sample producer. Never opens the physical bridge section.
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "../src/psvr2/GazeShare.h"
int main(int argc,char** argv){
 if(argc<2)return 2;
 const DWORD pid=DWORD(std::strtoul(argv[1],nullptr,10));
 const double seconds=argc>2?std::atof(argv[2]):30;
 if(!pid || seconds<=0 || seconds>600)return 2;
 HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,pid);if(!process)return 3;
 wchar_t name[96];swprintf_s(name,L"Local\\Darkness2VR.SimulatorGaze.%lu",pid);
 HANDLE mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(d2vr::psvr2::GazeShared),name);
 if(!mapping)return 4;
 if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mapping);return 5;}
 auto* shared=static_cast<d2vr::psvr2::GazeShared*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,0));
 if(!shared)return 6;
 shared->magic=d2vr::psvr2::kGazeMagic;shared->version=d2vr::psvr2::kGazeVersion;shared->seq.store(0);
 const auto start=GetTickCount64();unsigned frame=0;
 while(GetTickCount64()-start<uint64_t(seconds*1000) && WaitForSingleObject(process,0)==WAIT_TIMEOUT){
  const auto now=GetTickCount64();const auto phase=(now-start)%6000;
  // Fixation/noise -> 80ms blink -> one 10ms outlier -> true saccade -> long loss.
  float angle=phase>=3000 && phase<4500?.35f:0;
  if(phase<1500)angle=(frame%2?.002f:-.002f);
  if(phase>=2500 && phase<2510)angle=-.6f;
  d2vr::psvr2::GazeSample s{};s.publishMs=now;s.frameCounter=++frame;
  s.flags=(phase>=1500 && phase<1580)||(phase>=4500)?0:d2vr::psvr2::GazeCombinedValid;
  if(phase>=1500 && phase<1580)s.flags|=d2vr::psvr2::GazeBlinkLeft|d2vr::psvr2::GazeBlinkRight;
  // Production default GazeAxes=-x,y,-z. Positive target yaw points right.
  s.dirCombined[0]=-std::sin(angle);s.dirCombined[2]=std::cos(angle);
  d2vr::psvr2::GazeWrite(*shared,s);Sleep(10);
 }
 UnmapViewOfFile(shared);CloseHandle(mapping);CloseHandle(process);
 std::printf("Simulator gaze: %u samples, pid=%lu, isolated section\n",frame,pid);return 0;
}
