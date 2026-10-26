#include <windows.h>
#include <cassert>
#include <string>
#include <mutex>
#include <thread>
#include <vector>
#include "../../src/persistent_log.h"

int main() {
    char directory[MAX_PATH], path[MAX_PATH];
    assert(GetTempPathA(MAX_PATH, directory));
    assert(GetTempFileNameA(directory, "dvl", 0, path));
    d2vr_log::File log;
    assert(!log.Write("not open"));
    assert(log.Open(path));
    assert(!log.Open(path)); // cannot accidentally truncate an active session
    assert(log.Write("exception code=%08X", 0xc0000005u));
    // A separate reader sees the flushed crash record before Close.
    FILE* reader=nullptr;
    reader=_fsopen(path,"r",_SH_DENYNO); assert(reader);
    char line[128]; assert(fgets(line,sizeof(line),reader));
    assert(std::string(line)=="exception code=C0000005\n");
    fclose(reader);
    // Probe for the persistent write handle without touching the file contents.
    HANDLE denyReopen=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,
                                 OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    // Windows sharing checks are symmetric: the existing write handle means
    // this gate must fail, proving we still own the original writable stream.
    assert(denyReopen==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION);
    std::mutex mutex;
    std::vector<std::thread> workers;
    for(int t=0;t<4;++t) workers.emplace_back([&,t]{
        for(int i=0;i<2500;++i) {
            std::lock_guard<std::mutex> guard(mutex);
            assert(log.Write("thread=%d line=%d",t,i));
        }
    });
    for(auto& t:workers)t.join();
    log.Close();log.Close();assert(!log.Write("closed"));
    assert(fopen_s(&reader,path,"r")==0 && reader);
    assert(fgets(line,sizeof(line),reader));
    int expected[4]={},count=0,t=0,i=0;
    while(fgets(line,sizeof(line),reader)) {
        assert(sscanf_s(line,"thread=%d line=%d",&t,&i)==2);
        assert(t>=0 && t<4 && i==expected[t]++);++count;
    }
    assert(count==10000);fclose(reader);
    assert(log.Open(path));assert(log.Write("new session"));log.Close();
    assert(fopen_s(&reader,path,"r")==0 && reader);
    assert(fgets(line,sizeof(line),reader) && std::string(line)=="new session\n");
    assert(!fgets(line,sizeof(line),reader));fclose(reader);
    assert(DeleteFileA(path));
    puts("PASS: persistent handle, immediate crash-line visibility, 10000 serialized records, close/reopen lifecycle");
}
