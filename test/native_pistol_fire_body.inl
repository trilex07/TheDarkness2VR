// Execute SHA-pinned original CFBBA0 bytes supplied privately from TEMP.
// All changed code/data and callees belong to this fixture, never the game.
static bool nativeCameraEnabled=true;
static DWORD nativeAvatarReference[3]{};
static BYTE nativeCameraObject[16]{};
static void* nativeCameraTable[28]{};
static bool __fastcall FixtureGunType(void*,void*,DWORD type){CHECK(type==0x1119658);return true;}
static void* __fastcall FixtureCamera(void*){return nativeCameraObject;}
static bool __fastcall FixtureCameraEnabled(void*){return nativeCameraEnabled;}
static void __fastcall FixtureVector(void*,void*,float* out){out[0]=0;out[1]=0;out[2]=1;out[3]=0;}
static void* __fastcall FixtureAvatar(void* gun,void*,DWORD* out){
    CHECK(gun==guns[0]||gun==guns[1]);++nativeAvatarReference[1];*out=Ptr(nativeAvatarReference);return out;
}
static void* __fastcall FixtureRay(void* out,void*,DWORD zero,const void*,const void*,const void*){
    CHECK(zero==0);auto m=static_cast<float*>(out);Matrix(m,71,72,73);
    m[5]=m[10]=0;m[6]=1;m[9]=-1;return out;
}
static void __cdecl FixtureUnexpectedFree(void*){CHECK(false);}
static __declspec(naked) void FixtureCookie(){__asm ret}
static void NativePistolBody(const char* path,d2vr_weapon_bridge::Fire original,
                            const void*(__cdecl* result)(unsigned)) {
    void* gunTable[78]{};void* avatarTable[152]{};void** shotAvatar=avatarTable;
    gunTable[0x134/4]=reinterpret_cast<void*>(&FixtureAvatar);
    avatarTable[0x25C/4]=reinterpret_cast<void*>(&FixtureVector);
    DWORD descriptor[4]={0,0,0,0x1119658},cookie=0x12345678,emptyRef[3]{};
    alignas(16) float identity[16];Matrix(identity);
    *reinterpret_cast<void***>(nativeCameraObject)=nativeCameraTable;
    nativeCameraTable[0x60/4]=reinterpret_cast<void*>(&FixtureCameraEnabled);
    nativeCameraTable[0x6C/4]=reinterpret_cast<void*>(&FixtureVector);
    BYTE bytes[0x227];DWORD read=0;
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);CHECK(file!=INVALID_HANDLE_VALUE);
    CHECK(GetFileSize(file,nullptr)==sizeof(bytes));CHECK(ReadFile(file,bytes,sizeof(bytes),&read,nullptr)&&read==sizeof(bytes));CHECK(CloseHandle(file));
    CHECK(d2vr_native_clone::MatchesDigest(bytes,sizeof(bytes),d2vr_native_clone::weaponFireBlock.sha256));
    void* bodies[2]{};
    for(unsigned fixed=0;fixed<2;++fixed){
        auto body=static_cast<BYTE*>(VirtualAlloc(nullptr,sizeof(bytes),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));CHECK(body);memcpy(body,bytes,sizeof(bytes));
        // Relocate only the three independently decoded absolute fixture data
        // operands. This avoids reserving addresses already owned by the CRT.
        const DWORD offsets[]={0xD,0x55,0x61},oldAddresses[]={0x10D87F8,0x10A1D78,0xFBB320};
        const DWORD newAddresses[]={Ptr(&cookie),Ptr(emptyRef),Ptr(identity)};
        for(unsigned i=0;i<3;++i){DWORD observed=0;memcpy(&observed,bytes+offsets[i],4);CHECK(observed==oldAddresses[i]);memcpy(body+offsets[i],&newAddresses[i],4);}
        for(const auto& call:d2vr_native_clone::weaponFireCalls){
            CHECK(bytes[call.offset]==0xE8);uint32_t displacement=0;memcpy(&displacement,bytes+call.offset+1,4);
            CHECK(uint32_t(0xCFBBA0+call.offset+5+displacement)==call.originalTarget);
            void* target=nullptr;
            switch(call.originalTarget){
                case 0x4BF720:target=reinterpret_cast<void*>(&FixtureGunType);break;
                case 0xD30BF0:target=reinterpret_cast<void*>(&FixtureCamera);break;
                case 0x402550:target=reinterpret_cast<void*>(&FixtureUnexpectedFree);break;
                case 0x75C320:target=reinterpret_cast<void*>(&FixtureRay);break;
                case 0xC47570:target=fixed?d2vr_weapon_bridge::Bridge::FireEntry():reinterpret_cast<void*>(original);break;
                case 0xE12979:target=reinterpret_cast<void*>(&FixtureCookie);break;
                default:CHECK(false);
            }
            displacement=Ptr(target)-Ptr(body+call.offset+5);memcpy(body+call.offset+1,&displacement,4);
        }
        DWORD old=0;CHECK(VirtualProtect(body,sizeof(bytes),PAGE_EXECUTE_READ,&old));CHECK(FlushInstructionCache(GetCurrentProcess(),body,sizeof(bytes)));bodies[fixed]=body;
    }
    for(unsigned slot=0;slot<2;++slot)for(unsigned cameraPath=0;cameraPath<2;++cameraPath)
    for(unsigned tracked=0;tracked<2;++tracked)for(unsigned fireEnabled=0;fireEnabled<2;++fireEnabled)
    for(unsigned n=0;n<100;++n){
        Reset();nativeCameraEnabled=cameraPath!=0;g_protectedTrackedHands=tracked!=0;
        nativeAvatarReference[0]=Ptr(&shotAvatar);nativeAvatarReference[1]=1;nativeAvatarReference[2]=0;
        Word(guns[slot],0,Ptr(gunTable));Word(guns[slot],4,Ptr(descriptor));
        DWORD gunRef[3]={Ptr(guns[slot]),1,0};Word(inventory,0xB0+12*slot,Ptr(gunRef));
        fires[slot][0x480]=BYTE(fireEnabled);
        const bool emits=cameraPath||fireEnabled;
        alignas(16) float stock[16],expected[16];Matrix(stock,7,8,9);
        if(tracked&&emits)CHECK(ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,expected));
        uintptr_t returns[2]{};
        for(unsigned fixed=0;fixed<2;++fixed){
            const auto before=*static_cast<const unsigned*>(result(3));State state{};
            // Reproduce the old entry adapter too: even its correct incoming
            // controller matrix is overwritten by CFBBA0's camera branch.
            const float* incoming=(!fixed&&tracked&&emits)?expected:stock;
            returns[fixed]=invoke(bodies[fixed],fires[slot],inventory+0xB0+12*slot,incoming,&state);
            CHECK(*static_cast<const unsigned*>(result(3))==before+(emits?1:0));
            CHECK(gunRef[1]==1&&nativeAvatarReference[1]==1);
            if(emits){
                const auto observed=static_cast<const float*>(result(1));
                if(tracked&&(fixed||!cameraPath)){for(unsigned c=0;c<16;++c)CHECK(Near(observed[c],expected[c]));}
                else {CHECK(Near(observed[12],cameraPath?71.f:7.f));CHECK(Near(observed[9],cameraPath?-1.f:0.f));}
                const auto args=static_cast<const uintptr_t*>(result(2));
                CHECK(args[0]==Ptr(fires[slot])&&args[1]==Ptr(inventory+0xB0+12*slot));
                CHECK(args[2]==0x11111111&&args[3]==0x22222222&&args[4]==0x33333333);
            }
        }
        CHECK((returns[0]&255)==(returns[1]&255));
    }
    for(auto body:bodies)CHECK(VirtualFree(body,0,MEM_RELEASE));
    puts("PASS exact native CFBBA0 body:3200 invocations, camera replacement reproduced, late controller matrix consumed by both hands; native query/ref/no-shot paths retained; fixture callees only, not HMD proof");
}
