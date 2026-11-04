#pragma once
#include <windows.h>
// Optional polling backoff. It never establishes GPU completion: the caller
// must still observe the SAME event query returning S_OK before resource reuse.
// No global timer resolution or scheduler setting is changed.
namespace d2vr_gpu_wait {
struct Timer {
    HANDLE handle=nullptr;
    Timer():handle(CreateWaitableTimerExW(nullptr,nullptr,0x00000002,
        TIMER_MODIFY_STATE|SYNCHRONIZE)) {}
    ~Timer(){if(handle)CloseHandle(handle);}
    Timer(const Timer&)=delete;
    Timer& operator=(const Timer&)=delete;
};
inline void Pause(unsigned microseconds,unsigned& polls) {
    if(!microseconds || ++polls<=4){SwitchToThread();return;}
    const DWORD error=GetLastError();
    static thread_local Timer timer;
    LARGE_INTEGER due{};due.QuadPart=-LONGLONG(microseconds)*10;
    if(!timer.handle || !SetWaitableTimer(timer.handle,&due,0,nullptr,nullptr,FALSE) ||
       WaitForSingleObject(timer.handle,4)!=WAIT_OBJECT_0)SwitchToThread();
    SetLastError(error);
}
}
