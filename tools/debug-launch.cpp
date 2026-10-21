// Narrow x86 launch observer. Reports real exceptions/exit codes and debug text;
// forwards every non-breakpoint exception to the game, never changes game code.
#include <windows.h>
#include <cstdio>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3) { puts("debug-launch.exe <exe> <working-directory>"); return 2; }
    STARTUPINFOA si{sizeof(si)}; PROCESS_INFORMATION pi{};
    std::string command="\""+std::string(argv[1])+"\"";
    if(!CreateProcessA(argv[1],command.data(),nullptr,nullptr,FALSE,DEBUG_ONLY_THIS_PROCESS,nullptr,argv[2],&si,&pi)) {
        printf("CreateProcess error=%lu\n",GetLastError()); return 1;
    }
    DebugSetProcessKillOnExit(FALSE);
    printf("PID=%lu\n",pi.dwProcessId); fflush(stdout);
    auto start=GetTickCount64();
    for(;;) {
        DEBUG_EVENT e{};
        if(!WaitForDebugEvent(&e,1000)) {
            if(GetTickCount64()-start>60000) { DebugActiveProcessStop(pi.dwProcessId); puts("DETACHED at 60s, game remains running"); break; }
            continue;
        }
        DWORD disposition=DBG_CONTINUE;
        if(e.dwDebugEventCode==EXCEPTION_DEBUG_EVENT) {
            auto& x=e.u.Exception;
            printf("exception=%08lX address=%p first=%lu thread=%lu\n",x.ExceptionRecord.ExceptionCode,x.ExceptionRecord.ExceptionAddress,x.dwFirstChance,e.dwThreadId);
            if(x.ExceptionRecord.ExceptionCode!=EXCEPTION_BREAKPOINT) disposition=DBG_EXCEPTION_NOT_HANDLED;
            if(!x.dwFirstChance) {
                HANDLE t=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,e.dwThreadId);
                CONTEXT c{}; c.ContextFlags=CONTEXT_FULL;
                if(t && GetThreadContext(t,&c)) {
                    printf("EIP=%08lX ESP=%08lX EBP=%08lX\n",c.Eip,c.Esp,c.Ebp);
                    DWORD stack[32]{}; SIZE_T got=0;
                    ReadProcessMemory(pi.hProcess,(void*)c.Esp,stack,sizeof(stack),&got);
                    for(unsigned i=0;i<got/4;i++) printf("stack+%02X=%08lX\n",i*4,stack[i]);
                }
                if(t)CloseHandle(t);
            }
        } else if(e.dwDebugEventCode==OUTPUT_DEBUG_STRING_EVENT) {
            char buf[2048]{}; SIZE_T got=0;
            ReadProcessMemory(pi.hProcess,e.u.DebugString.lpDebugStringData,buf,
                min(sizeof(buf)-2,(size_t)e.u.DebugString.nDebugStringLength*(e.u.DebugString.fUnicode?2:1)),&got);
            if(e.u.DebugString.fUnicode) printf("debug=%ls\n",(wchar_t*)buf); else printf("debug=%s\n",buf);
        } else if(e.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT) {
            char name[MAX_PATH]{};
            if(e.u.LoadDll.hFile) { GetFinalPathNameByHandleA(e.u.LoadDll.hFile,name,MAX_PATH,0); CloseHandle(e.u.LoadDll.hFile); }
            printf("module=%p %s\n",e.u.LoadDll.lpBaseOfDll,name);
        } else if(e.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT) {
            if(e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
        } else if(e.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT) {
            printf("EXIT code=%08lX elapsedMs=%llu\n",e.u.ExitProcess.dwExitCode,GetTickCount64()-start);
            ContinueDebugEvent(e.dwProcessId,e.dwThreadId,DBG_CONTINUE); break;
        }
        fflush(stdout); ContinueDebugEvent(e.dwProcessId,e.dwThreadId,disposition);
        if(GetTickCount64()-start>60000) { DebugActiveProcessStop(pi.dwProcessId); puts("DETACHED at 60s, game remains running"); break; }
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}
