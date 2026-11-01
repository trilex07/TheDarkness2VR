// Called inside the owned, selected 985D80 view callback. Read only.
// Stock EXE: 56CD8B stores vtable FF8818; FF88AC is 9922F0;
// 9922F8 loads owner+274 and 99230C calls 776DC0 on that renderer.
static void ObserveGazeLodOwner(void* viewRenderer) {
    static ULONGLONG next=0;
    const auto now=GetTickCount64();
    if(now<next)return;
    next=now+5000;
    if(!GetPrivateProfileIntA("Render","GazeLodDiagnostic",0,
        g_activeIni[0]?g_activeIni:".\\Darkness2VR.ini"))return;
    unsigned owner=0,table=0,sceneRenderer=0,sceneTable=0;
    bool admitted=false;
    __try {
        const auto base=reinterpret_cast<const unsigned char*>(viewRenderer);
        const auto reference=*reinterpret_cast<const unsigned*>(base+0x2a4);
        if(reference)owner=*reinterpret_cast<const unsigned*>(reference);
        if(owner) {
            table=*reinterpret_cast<const unsigned*>(owner);
            if(table==0xff8818 &&
               *reinterpret_cast<const unsigned*>(0xff88ac)==0x9922f0) {
                sceneRenderer=*reinterpret_cast<const unsigned*>(owner+0x274);
                if(sceneRenderer)sceneTable=*reinterpret_cast<const unsigned*>(sceneRenderer);
                admitted=sceneRenderer!=0;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {admitted=false;}
    Log("[GazeLOD] selected-view owner=%08X table=%08X sceneRenderer=%08X sceneTable=%08X linked=%d; read-only",
        owner,table,sceneRenderer,sceneTable,int(admitted));
}
