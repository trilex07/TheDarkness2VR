// Dual physical weapon wheels (hold R3) + right-stick-down crouch toggle.
// Included by d3d9.cpp after native_aim.inl, before SyncOpenXRGamepad.
// Input thread: SyncOpenXRGamepad -> WeaponWheelR3 / ApplyWeaponWheel.
// Render thread: weapon_wheel_renderer.inl reads g_wheelDisplay under its lock.
// Equip requests are STOCK pad pulses (DPAD_LEFT / DPAD_RIGHT); the game keeps
// every equip rule. Logic lives in dual_weapon_wheel.h (host-tested).


// --------------------------------------------------------- native loadout
// Byte-verified once: every offset used by CaptureWeaponLoadout comes from
// these functions of the pinned EXE. Mismatch -> loadout stays invalid and the
// wheels never open (R3 passes through unchanged).
static int g_weaponLoadoutVerified=-1; // -1 unknown, 0 refused, 1 verified
static bool VerifyWeaponLoadoutLayout() {
    if(g_weaponLoadoutVerified>=0) return g_weaponLoadoutVerified==1;
    bool ok=false;
    __try {
        static const BYTE savedRef[]={0x8D,0x14,0xC5,0,0,0,0,0x2B,0xD0,0x8B,0x8C,0x91,0x2C,0x01,0,0}; // 0x7185DC: [inv+0x12C+28*slot]
        static const BYTE handSlot[]={0x8B,0x44,0x24,0x04,0x8D,0x04,0x40,0x8B,0x84,0x81,0xB0,0,0,0};    // 0x8C62E0: [inv+0xB0+12*hand]
        static const BYTE gunSlot[]={0x8B,0x89,0xD4,0,0,0,0x8B,0x44,0x24,0x04,0x89,0x08,0xC2,0x04,0};    // 0x72BC70: gun+0xD4
        static const BYTE swapFlag[]={0x80,0xBE,0xB9,0x07,0,0,0};                                        // 0x8BE7FB: inv+0x7B9
        ok=(uintptr_t)GetModuleHandleW(nullptr)==0x400000 &&
           !memcmp((void*)0x7185DC,savedRef,sizeof(savedRef)) && !memcmp((void*)0x8C62E0,handSlot,sizeof(handSlot)) &&
           !memcmp((void*)0x72BC70,gunSlot,sizeof(gunSlot)) && !memcmp((void*)0x8BE7FB,swapFlag,sizeof(swapFlag)) &&
           *(DWORD*)(0xFFCA10+0x14C)==0x7185C0 && *(DWORD*)(0xFFCA10+0x178)==0x8C62E0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { ok=false; }
    g_weaponLoadoutVerified=ok ? 1:0;
    Log("[WeaponWheel] native loadout layout %s (slot ids from globals 1110258 two-handed / 10CC248, 10CC24C one-handed; saved refs inv+0x12C+28*slot; hand guns inv+0xB0+12*hand, gun+0xD4 slot; swap inv+0x7B9)",
        ok ? "verified" : "REFUSED: unexpected EXE bytes; wheels disabled");
    return ok;
}

// Model identity probe: the gun object's own strings (resource/type names).
// Read-only, SEH-guarded, runs once per new weapon object. Matching keywords
// pick a model icon; anything unmatched keeps the slot's family icon and the
// logged strings are the evidence for the next mapping.
static bool PrintableString(const char* p,char* out,size_t cap) {
    size_t n=0;
    for(;n+1<cap;++n) {
        const unsigned char c=(unsigned char)p[n];
        if(!c) break;
        if(c<0x20 || c>0x7E) return false;
        out[n]=(char)c;
    }
    out[n]=0;
    return n>=4 && n+1<cap && !p[n];
}
static bool ContainsNoCase(const char* s,const char* key) {
    for(;*s;++s) {
        const char* a=s; const char* b=key;
        while(*a && *b && toupper((unsigned char)*a)==toupper((unsigned char)*b)) { ++a; ++b; }
        if(!*b) return true;
    }
    return false;
}
static int IconForName(const char* s) {
    using I=d2vr_weapon_icons::Icon;
    static const struct { const char* key; I icon; } table[]={
        {"1911",I::M1911},{"COBRA",I::Cobra},{"DEAGLE",I::DesertEagle},{"EAGLE",I::DesertEagle},
        {"UZI",I::MicroUzi},{"UMP",I::Ump45},{"MP7",I::Mp7},{"DEFENDER",I::Defender},
        {"SAWN",I::SawnOff},{"SAWED",I::SawnOff},{"1887",I::M1887},{"KALASH",I::Kalashnikov},
        {"AKS",I::Kalashnikov},{"AK47",I::Kalashnikov},{"AK74",I::Kalashnikov},{"CARBINE",I::Carbine},
        {"STRIKER",I::Striker},{"RFB",I::Striker}};
    for(const auto& e:table) if(ContainsNoCase(s,e.key)) return int(e.icon);
    return -1;
}
static bool WideString(const wchar_t* p,char* out,size_t cap) {
    size_t n=0;
    for(;n+1<cap;++n) { const wchar_t c=p[n]; if(!c) break; if(c<0x20 || c>0x7E) return false; out[n]=(char)c; }
    out[n]=0;
    return n>=4 && n+1<cap && !p[n];
}
// Bounded walk (depth 3, first 0x40 bytes per object, <=96 objects) logging
// every ASCII/UTF-16 string that looks like a resource name or matches an icon.
static int ScanResourceNames(int slot,const char* root,DWORD start,int& logged) {
    DWORD queue[96]; int depth[96], head=0, tail=0, icon=-1;
    queue[tail]=start; depth[tail++]=0;
    char text[96];
    while(head<tail && logged<40) {
        const DWORD obj=queue[head]; const int dep=depth[head++];
        for(int off=0;off<0x40 && logged<40;off+=4) {
            DWORD p=0;
            __try { p=*(const DWORD*)(obj+off); } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
            if(p<0x10000 || p>=0x7FFF0000) continue;
            bool str=false, wide=false;
            __try { str=PrintableString((const char*)p,text,sizeof(text)); if(!str) wide=WideString((const wchar_t*)p,text,sizeof(text)); }
            __except(EXCEPTION_EXECUTE_HANDLER) { str=wide=false; }
            if(str || wide) {
                const int match=IconForName(text);
                if(icon<0 && match>=0) icon=match;
                if(match>=0 || strchr(text,'/') || strchr(text,'.') || strchr(text,'\\')) {
                    Log("[WeaponWheelProbe] slot=%d %s depth=%d obj=%08X+%02X %s\"%s\"%s",slot,root,dep,obj,off,wide ? "L":"",text,
                        match>=0 ? " (icon match)":"");
                    ++logged;
                }
            } else if(dep<3 && tail<96) {
                bool seen=false; for(int i=0;i<tail;++i) if(queue[i]==p) seen=true;
                if(!seen) { queue[tail]=p; depth[tail++]=dep+1; }
            }
        }
    }
    return icon;
}
static int ProbeWeaponIdentity(int slot,const BYTE* gun) {
    int icon=-1, logged=0;
    char text[96];
    Log("[WeaponWheelProbe] slot=%d gun=%p vtable=%08X type=%08X; scanning own strings",slot,gun,
        *(const DWORD*)gun,*(const DWORD*)(gun+4));
    for(int off=8;off<0x400 && logged<24;off+=4) {
        DWORD p=0;
        __try { p=*(const DWORD*)(gun+off); } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
        if(p<0x10000 || p>=0x7FFF0000) continue;
        for(int depth=0;depth<2 && logged<24;++depth) {
            // depth 0: the field points at a string; depth 1: at an object
            // whose first 0x40 bytes point at strings (type/resource name).
            for(int inner=0;inner<(depth ? 0x40:4);inner+=4) {
                DWORD q=p;
                __try {
                    if(depth) q=*(const DWORD*)(p+inner);
                    if(q<0x10000 || q>=0x7FFF0000) continue;
                    if(!PrintableString((const char*)q,text,sizeof(text))) continue;
                } __except(EXCEPTION_EXECUTE_HANDLER) { continue; }
                const int match=IconForName(text);
                if(icon<0 && match>=0) icon=match;
                if(depth) Log("[WeaponWheelProbe] slot=%d +%03X/+%02X -> \"%s\"%s",slot,off,inner,text,match>=0 ? " (icon match)":"");
                else Log("[WeaponWheelProbe] slot=%d +%03X -> \"%s\"%s",slot,off,text,match>=0 ? " (icon match)":"");
                ++logged;
            }
        }
    }
    // The model lives in the type object (+4); both identical pistols shared it.
    const BYTE* type=nullptr;
    __try { type=*(const BYTE* const*)(gun+4); } __except(EXCEPTION_EXECUTE_HANDLER) { type=nullptr; }
    for(int off=0;type && off<0x100 && logged<48;off+=4) {
        DWORD p=0;
        __try { p=*(const DWORD*)(type+off); } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
        if(p<0x10000 || p>=0x7FFF0000) continue;
        for(int inner=-4;inner<0x60 && logged<48;inner+=4) {
            DWORD q=p;
            __try {
                if(inner>=0) q=*(const DWORD*)(p+inner);
                if(q<0x10000 || q>=0x7FFF0000) continue;
                if(!PrintableString((const char*)q,text,sizeof(text))) continue;
            } __except(EXCEPTION_EXECUTE_HANDLER) { continue; }
            const int match=IconForName(text);
            if(icon<0 && match>=0) icon=match;
            if(inner<0) Log("[WeaponWheelProbe] slot=%d type+%02X -> \"%s\"%s",slot,off,text,match>=0 ? " (icon match)":"");
            else Log("[WeaponWheelProbe] slot=%d type+%02X/+%02X -> \"%s\"%s",slot,off,inner,text,match>=0 ? " (icon match)":"");
            ++logged;
        }
    }
    if(icon<0) {
        int deep=0; DWORD mesh=0, typeRoot=0;
        __try { mesh=*(const DWORD*)(gun+0x44); typeRoot=*(const DWORD*)(gun+4); } __except(EXCEPTION_EXECUTE_HANDLER) {}
        if(mesh) icon=ScanResourceNames(slot,"mesh",mesh,deep);
        if(icon<0 && typeRoot) icon=ScanResourceNames(slot,"type",typeRoot,deep);
        logged+=deep;
    }
    Log("[WeaponWheelProbe] slot=%d strings=%d icon=%s",slot,logged,
        icon>=0 ? d2vr_weapon_icons::Name(d2vr_weapon_icons::Icon(icon)) : "family fallback");
    return icon;
}

struct WeaponLoadoutSample {
    d2vr_wheel::Loadout loadout;
    DWORD object[3]{};   // saved weapon objects (identity changes -> re-probe)
    DWORD type[3]{};     // object+4: same value = same weapon model (headset 23:20)
    int slotAmmo[3]{-1,-1,-1}; // rounds in the magazine (gun+0xA48, native getter 0x6E8020)
    int handAmmo[2]{-1,-1};    // same for the gun in native hand 0 (right) / 1 (left)
    int maxAmmo[3]{};          // largest magazine count seen for that weapon type this run
    int icon[3]{-1,-1,-1};
};
static SRWLOCK g_weaponLoadoutLock=SRWLOCK_INIT;
static WeaponLoadoutSample g_weaponLoadout;
static ULONGLONG g_weaponLoadoutTime=0;

// Called from CaptureNativeGameplay with the already ownership-checked inventory.
static void CaptureWeaponLoadout(BYTE* inventory) {
    if(!g_weaponWheelEnabled || !VerifyWeaponLoadoutLayout()) return;
    WeaponLoadoutSample s;
    static int lastIds=-1;
    int ids[3]{-1,-1,-1}; // native slot id of canonical 0 two-handed, 1 and 2 one-handed
    __try {
        ids[0]=*(int*)0x1110258; ids[1]=*(int*)0x10CC248; ids[2]=*(int*)0x10CC24C; // runtime enum values
        const bool idsOk=ids[0]>=0 && ids[0]<8 && ids[1]>=0 && ids[1]<8 && ids[2]>=0 && ids[2]<8 &&
            ids[0]!=ids[1] && ids[0]!=ids[2] && ids[1]!=ids[2];
        if(!idsOk) __leave;
        for(int slot=0;slot<3;++slot) {
            auto ref=*(DWORD**)(inventory+0x12C+28*ids[slot]);
            s.object[slot]=ref ? *ref : 0;
            s.type[slot]=s.object[slot] ? *(DWORD*)(uintptr_t)(s.object[slot]+4) : 0;
            if(s.object[slot]) {
                const int ammo=*(int*)(uintptr_t)(s.object[slot]+0xA48);
                s.slotAmmo[slot]=ammo>=0 && ammo<1000 ? ammo : -1;
            }
            s.loadout.owned[slot]=s.object[slot]!=0;
        }
        for(int hand=0;hand<2;++hand) {
            auto gun=**(BYTE***)(inventory+0xB0+12*hand);
            const int native=gun ? *(int*)(gun+0xD4) : 8;
            if(gun && *(DWORD*)gun==0x1010E30) {
                const int ammo=*(int*)(gun+0xA48);
                s.handAmmo[hand]=ammo>=0 && ammo<1000 ? ammo : -1;
            }
            int canonical=native==8 ? d2vr_wheel::kSlotEmpty : d2vr_wheel::kSlotOther;
            for(int slot=0;slot<3;++slot) if(native==ids[slot]) canonical=slot;
            s.loadout.hand[hand]=canonical;
        }
        s.loadout.swapped=*(BYTE*)(inventory+0x7B9)!=0;
        s.loadout.valid=true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { s=WeaponLoadoutSample{}; }
    const int idKey=(ids[0]&15)|(ids[1]&15)<<4|(ids[2]&15)<<8;
    if(idKey!=lastIds) {
        lastIds=idKey;
        Log("[WeaponWheel] native slot ids two-handed=%d one-handed=%d,%d%s",ids[0],ids[1],ids[2],
            s.loadout.valid ? "" : " (invalid: loadout disabled)");
    }
    static DWORD probed[3]{};
    static int probedIcon[3]{-1,-1,-1};
    for(int slot=0;slot<3 && s.loadout.valid;++slot) {
        if(s.object[slot] && s.object[slot]!=probed[slot]) {
            probed[slot]=s.object[slot];
            probedIcon[slot]=ProbeWeaponIdentity(slot,(const BYTE*)(uintptr_t)s.object[slot]);
        }
        s.icon[slot]=s.object[slot] ? probedIcon[slot] : -1;
    }
    // Largest magazine count per weapon type: an SMG holds far more rounds than
    // a pistol, so a one-handed type that ever exceeded 20 is shown as an SMG
    // when its name is not found (headset 23:42: uzi drawn as a pistol).
    static DWORD maxType[8]{}; static int maxValue[8]{};
    for(int slot=0;slot<3 && s.loadout.valid;++slot) {
        if(!s.type[slot]) continue;
        int i=0; while(i<8 && maxType[i] && maxType[i]!=s.type[slot]) ++i;
        if(i==8) i=7;
        if(maxType[i]!=s.type[slot]) { maxType[i]=s.type[slot]; maxValue[i]=0; }
        if(s.slotAmmo[slot]>maxValue[i]) maxValue[i]=s.slotAmmo[slot];
        s.maxAmmo[slot]=maxValue[i];
    }
    static int lastAmmo[2]{-2,-2};
    if(s.loadout.valid && (s.handAmmo[0]!=lastAmmo[0] || s.handAmmo[1]!=lastAmmo[1])) {
        static ULONGLONG lastLog=0; const ULONGLONG t=GetTickCount64();
        if(t-lastLog>500 || (lastAmmo[0]>=0 && s.handAmmo[0]>lastAmmo[0])) { // reloads always logged
            lastLog=t;
            Log("[AmmoHud] magazine right=%d left=%d (max seen slot0/1/2=%d/%d/%d)",s.handAmmo[0],s.handAmmo[1],
                s.maxAmmo[0],s.maxAmmo[1],s.maxAmmo[2]);
        }
        lastAmmo[0]=s.handAmmo[0]; lastAmmo[1]=s.handAmmo[1];
    }
    static int lastKey=-1;
    const int key=s.loadout.valid ? (int(s.loadout.owned[0])|int(s.loadout.owned[1])<<1|int(s.loadout.owned[2])<<2|
        s.loadout.hand[0]<<3|s.loadout.hand[1]<<7|int(s.loadout.swapped)<<11) : -2;
    if(key!=lastKey) {
        lastKey=key;
        Log("[WeaponWheel] native loadout valid=%d owned two-handed=%d one-handed1=%d one-handed2=%d hands right=%d left=%d swapped=%d",
            (int)s.loadout.valid,(int)s.loadout.owned[0],(int)s.loadout.owned[1],(int)s.loadout.owned[2],
            s.loadout.hand[0],s.loadout.hand[1],(int)s.loadout.swapped);
    }
    if(TryAcquireSRWLockExclusive(&g_weaponLoadoutLock)) {
        g_weaponLoadout=s; g_weaponLoadoutTime=GetTickCount64();
        ReleaseSRWLockExclusive(&g_weaponLoadoutLock);
    }
}
// The sample is refreshed by the renderer and briefly goes stale; accept up to 1 s.
static bool CopyWeaponLoadout(WeaponLoadoutSample& out) {
    AcquireSRWLockShared(&g_weaponLoadoutLock);
    out=g_weaponLoadout; const ULONGLONG time=g_weaponLoadoutTime;
    ReleaseSRWLockShared(&g_weaponLoadoutLock);
    return out.loadout.valid && time && GetTickCount64()-time<1000;
}

// ------------------------------------------------------------- display
struct WheelDisplay {
    bool visible[2]{};          // XR hand order: 0 left, 1 right
    XrPosef pose[2]{};
    XrSpace space=XR_NULL_HANDLE;
    d2vr_weapon_wheel_graphics::Wheel wheel[2];
    uint64_t revision=0;
};
static SRWLOCK g_wheelDisplayLock=SRWLOCK_INIT;
static WheelDisplay g_wheelDisplay;
constexpr float kWheelSizeM=.26f;   // quad edge; wheel radius .9 of half -> ~11.7 cm
constexpr float kWheelDeadM=.045f;  // hand travel before a sector highlights

static d2vr_weapon_icons::Icon IconForSlot(const WeaponLoadoutSample& s,int slot) {
    using I=d2vr_weapon_icons::Icon;
    if(slot>=0 && slot<3 && s.icon[slot]>=0 && s.icon[slot]<int(I::Count)) return I(s.icon[slot]);
    if(slot==d2vr_wheel::kSlotTwoHanded) return I::AssaultRifle;
    if(slot==d2vr_wheel::kSlotEmpty) return I::EmptyHand;
    return slot>=0 && slot<3 && s.maxAmmo[slot]>20 ? I::Smg : I::Pistol;
}

static d2vr_weapon_wheel_graphics::Slot SlotForItem(const WeaponLoadoutSample& s,const d2vr_wheel::Item& it) {
    const auto& l=s.loadout;
    if(it.kind==d2vr_wheel::Kind::TwoHanded) return {IconForSlot(s,d2vr_wheel::kSlotTwoHanded),true,false};
    const int slot=it.slot>=0 ? it.slot : l.RightOneHanded();
    return {IconForSlot(s,slot),true,it.kind==d2vr_wheel::Kind::Dual};
}
static bool SameOneHandedModel(const WeaponLoadoutSample& s) {
    return s.type[1] && s.type[1]==s.type[2];
}
static d2vr_wheel::EquipMemory g_equipMemory;

static XrQuaternionf QuatFromBasis(const XrVector3f& x,const XrVector3f& y,const XrVector3f& z) {
    const float tr=x.x+y.y+z.z; XrQuaternionf q{};
    if(tr>0) { const float s=std::sqrt(tr+1)*2; q.w=.25f*s; q.x=(y.z-z.y)/s; q.y=(z.x-x.z)/s; q.z=(x.y-y.x)/s; }
    else if(x.x>y.y && x.x>z.z) { const float s=std::sqrt(1+x.x-y.y-z.z)*2; q.w=(y.z-z.y)/s; q.x=.25f*s; q.y=(y.x+x.y)/s; q.z=(z.x+x.z)/s; }
    else if(y.y>z.z) { const float s=std::sqrt(1+y.y-x.x-z.z)*2; q.w=(z.x-x.z)/s; q.x=(y.x+x.y)/s; q.y=.25f*s; q.z=(z.y+y.z)/s; }
    else { const float s=std::sqrt(1+z.z-x.x-y.y)*2; q.w=(x.y-y.x)/s; q.x=(z.x+x.z)/s; q.y=(z.y+y.z)/s; q.z=.25f*s; }
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(n>1e-6f) { q.x/=n; q.y/=n; q.z/=n; q.w/=n; } else q={0,0,0,1};
    return q;
}

// ---------------------------------------------------------- input state
struct WheelHand {
    bool shown=false;
    d2vr_wheel::Menu menu;
    d2vr_wheel::HandPick pick;
    XrVector3f anchor{}, axisX{}, axisY{};
    XrPosef pose{};
};
static d2vr_wheel::HoldGate g_wheelGate;
static d2vr_wheel::StickFlick g_stickFlick;
static d2vr_wheel::Equip g_wheelEquip;
static WheelHand g_wheelHand[2]; // XR order: 0 left, 1 right; only one is shown
static WeaponLoadoutSample g_wheelOpenLoadout;
static bool g_wheelAllowed=false;   // wheels may open / stay open
static bool g_inputContext=false;   // gameplay context for the crouch flick
static XrSpace g_wheelSpace=XR_NULL_HANDLE;

static void PublishWheelDisplay(bool open) {
    WheelDisplay d;
    if(open) for(int h=0;h<2;++h) {
        const auto& w=g_wheelHand[h];
        d.visible[h]=w.shown && w.menu.count>0;
        d.pose[h]=w.pose;
        auto& g=d.wheel[h];
        g.count=w.menu.count; g.selected=w.pick.hover; g.equipped=w.menu.equipped;
        for(int i=0;i<w.menu.count;++i) g.slots[i]=SlotForItem(g_wheelOpenLoadout,w.menu.item[i]);
    }
    d.space=g_wheelSpace;
    AcquireSRWLockExclusive(&g_wheelDisplayLock);
    const auto& old=g_wheelDisplay;
    bool same=old.visible[0]==d.visible[0] && old.visible[1]==d.visible[1] && old.space==d.space;
    for(int h=0;h<2 && same;++h) if(d.visible[h]) {
        const auto& a=old.wheel[h]; const auto& b=d.wheel[h];
        same=a.count==b.count && a.selected==b.selected && a.equipped==b.equipped &&
             !memcmp(&old.pose[h],&d.pose[h],sizeof(XrPosef));
        for(int i=0;i<b.count && same;++i) same=a.slots[i].icon==b.slots[i].icon && a.slots[i].dual==b.slots[i].dual;
    }
    d.revision=same ? old.revision : old.revision+1;
    g_wheelDisplay=d;
    ReleaseSRWLockExclusive(&g_wheelDisplayLock);
}

// One wheel at the right hand (left hand if the right is not tracked) with
// every owned weapon (user request 2026-10-31 23:11).
static void OpenWheels() {
    g_wheelSpace=g_xrSpace;
    const auto head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    const auto& l=g_wheelOpenLoadout.loadout;
    for(auto& w:g_wheelHand) w=WheelHand{};
    const int h=g_hands[1].gripValid ? 1 : 0;
    auto& w=g_wheelHand[h];
    w.menu=d2vr_wheel::WeaponMenu(l,SameOneHandedModel(g_wheelOpenLoadout));
    if(g_hands[h].gripValid && w.menu.count) {
        w.anchor=g_hands[h].grip.position;
        // Plane faces the head, upright in the tracking space.
        XrVector3f z{head.position.x-w.anchor.x,0,head.position.z-w.anchor.z};
        float n=std::sqrt(z.x*z.x+z.z*z.z);
        if(!(n>1e-3f)) { z={0,0,1}; n=1; }
        z.x/=n; z.z/=n;
        const XrVector3f y{0,1,0}, x{y.y*z.z-y.z*z.y, y.z*z.x-y.x*z.z, y.x*z.y-y.y*z.x};
        w.axisX=x; w.axisY=y;
        w.pose.position=w.anchor;
        w.pose.orientation=QuatFromBasis(x,y,z);
        w.shown=true;
    }
    static const char* const kKind[]={"single","dual","two-handed"};
    char items[96]=""; size_t used=0;
    for(int i=0;i<w.menu.count && used<sizeof(items);++i)
        used+=size_t(_snprintf_s(items+used,sizeof(items)-used,_TRUNCATE,"%s%s:%d",i?" ":"",kKind[int(w.menu.item[i].kind)],w.menu.item[i].slot));
    Log("[WeaponWheel] open at %s hand: [%s] equipped=%d%s; hands right=%d left=%d sameModel=%d",h ? "right":"left",
        items,w.menu.equipped,w.shown ? "" : " (hidden)",l.hand[0],l.hand[1],(int)SameOneHandedModel(g_wheelOpenLoadout));
}

static void CloseWheels(bool commit) {
    int pick=-1; d2vr_wheel::Item item{};
    for(const auto& w:g_wheelHand)
        if(commit && w.shown && w.pick.touched && w.pick.hover>=0 && w.pick.hover<w.menu.count) {
            pick=w.pick.hover; item=w.menu.item[pick];
        }
    if(commit) {
        WeaponLoadoutSample now;
        const bool fresh=CopyWeaponLoadout(now);
        if(pick>=0 && fresh && !d2vr_wheel::Matches(item,now.loadout)) g_wheelEquip.Start(item);
        Log("[WeaponWheel] release: pick=%d kind=%d slot=%d fresh=%d hands right=%d left=%d -> %s",pick,int(item.kind),item.slot,(int)fresh,
            fresh ? now.loadout.hand[0] : -1,fresh ? now.loadout.hand[1] : -1,g_wheelEquip.Active() ? "equip (closed-loop D-pad)" : "no change");
    } else Log("[WeaponWheel] cancelled");
    for(auto& w:g_wheelHand) w=WheelHand{};
    PublishWheelDisplay(false);
}

// Context in which the wheels may open and the crouch flick acts.
static bool WeaponWheelContext(const XINPUT_GAMEPAD& gp,bool focused) {
    if(!focused || !g_in3DGameplayMode || (gp.wButtons & XINPUT_GAMEPAD_START) || g_nativeCinematicPlaying)
        return false;
    NativeGameplaySample sample;
    // Holding an object with a tentacle keeps the wheel/flicks (user 23:26);
    // only a character held for an execution blocks them.
    if(CopyNativeGameplay(sample) && sample.demonHold==2) return false;
    return true;
}

// Before the recenter chord: decides whether the game sees R3 this poll.
static bool WeaponWheelR3(const XINPUT_GAMEPAD& gp,bool r3,bool l3,bool focused) {
    g_inputContext=WeaponWheelContext(gp,focused);
    if(!g_weaponWheelEnabled) { g_wheelAllowed=false; return r3; }
    WeaponLoadoutSample loadout;
    const bool haveLoadout=CopyWeaponLoadout(loadout);
    const bool wasOpen=g_wheelGate.open;
    g_wheelAllowed=g_inputContext && (wasOpen || haveLoadout) &&
        (g_wheelSpace==g_xrSpace || !wasOpen) && (g_hands[0].gripValid || g_hands[1].gripValid);
    bool forward=false;
    const auto event=g_wheelGate.Update(r3,l3,g_wheelAllowed,GetTickCount64(),forward);
    if(event==d2vr_wheel::HoldGate::Event::Open) {
        g_wheelOpenLoadout=loadout;
        OpenWheels();
    } else if(event==d2vr_wheel::HoldGate::Event::Commit) CloseWheels(true);
    else if(event==d2vr_wheel::HoldGate::Event::Cancel) CloseWheels(false);
    return forward;
}

// [Input] CrouchButton / JumpButton / AButton, re-read live when used.
static uint16_t ReadPadSetting(const char* key,const char* fallback,uint16_t fallbackBits) {
    struct Cache { const char* key; char last[32]; uint16_t bits; };
    static Cache cache[3]={{"CrouchButton","",0},{"JumpButton","",0},{"AButton","",0}};
    Cache* c=nullptr;
    for(auto& e:cache) if(!strcmp(e.key,key)) c=&e;
    if(!c) return fallbackBits;
    char value[32]{};
    GetPrivateProfileStringA("Input",key,fallback,value,sizeof(value),g_activeIni[0] ? g_activeIni:".\\Darkness2VR.ini");
    if(strcmp(c->last,value) || (!c->bits && strcmp(value,"OFF") && strcmp(value,"off"))) {
        strcpy_s(c->last,value);
        uint16_t parsed=0;
        if(d2vr_wheel::ParseCrouchButton(value,parsed)) c->bits=parsed;
        else { c->bits=fallbackBits; Log("[PadMap] unknown %s=%s; using %s",key,value,fallback); }
        Log("[PadMap] %s=%s (buttons=0x%04X)",key,value,c->bits);
    }
    return c->bits;
}
// Physical A (PSVR2 Cross): reload by default (stock X, user request 23:11).
// While an enemy is held for an execution the stock A choice passes through.
static bool MenuConfirmActive(); // eye_gaze.inl: pause or menu without world
static uint16_t PhysicalAButton() {
    if(!g_inputContext || MenuConfirmActive()) return XINPUT_GAMEPAD_A; // menus incl. pause: A confirms
    NativeGameplaySample sample;
    if(CopyNativeGameplay(sample) && sample.demonHold==2) return XINPUT_GAMEPAD_A;
    return ReadPadSetting("AButton","X",XINPUT_GAMEPAD_X);
}

static void ApplyMenuTriggerConfirm(XINPUT_GAMEPAD& gp);
// After the recenter chord, before trigger/gesture rewriting.
static void ApplyWeaponWheel(XINPUT_GAMEPAD& gp) {
    const ULONGLONG now=GetTickCount64();
    if(g_weaponWheelEnabled) {
        if(g_wheelGate.open) {
            // Wheel owns the right stick and both triggers while it is shown.
            gp.sThumbRX=gp.sThumbRY=0; gp.bLeftTrigger=gp.bRightTrigger=0;
            for(int h=0;h<2;++h) {
                auto& w=g_wheelHand[h];
                if(!w.shown) continue;
                if(!g_hands[h].gripValid) { w.pick.hover=-1; continue; } // tracking loss: no guess
                const auto& p=g_hands[h].grip.position;
                const XrVector3f d{p.x-w.anchor.x,p.y-w.anchor.y,p.z-w.anchor.z};
                const float u=d.x*w.axisX.x+d.y*w.axisX.y+d.z*w.axisX.z;
                const float v=-(d.x*w.axisY.x+d.y*w.axisY.y+d.z*w.axisY.z);
                const int before=w.pick.hover;
                w.pick.Update(u,v,w.menu.count,kWheelDeadM);
                // [EyeTracking] GazeWheel: looking at an item selects it (wins over
                // the hand while the gaze is on the wheel; user 01:01).
                XrVector3f go{},gd{}; float gu=0,gv=0;
                if(g_gazeWheel && GazeWorldRay(go,gd) &&
                   d2vr_gaze_menu::HitQuad({go.x,go.y,go.z},{gd.x,gd.y,gd.z},
                       {w.pose.position.x,w.pose.position.y,w.pose.position.z},
                       {w.pose.orientation.x,w.pose.orientation.y,w.pose.orientation.z,w.pose.orientation.w},
                       kWheelSizeM,kWheelSizeM,gu,gv)) {
                    // Plane units: quad spans [-1,1]; wheel radius .9, centre disc .23.
                    const int sector=d2vr_wheel::SectorFromOffset((gu-.5f)*2,(gv-.5f)*2,w.menu.count,.23f);
                    if(sector>=0) { w.pick.hover=sector; w.pick.touched=true; }
                }
                if(w.pick.hover!=before && w.pick.hover>=0)
                    Log("[WeaponWheel] %s hand hovers item %d (kind=%d slot=%d)",h ? "right":"left",w.pick.hover,
                        int(w.menu.item[w.pick.hover].kind),w.menu.item[w.pick.hover].slot);
            }
            PublishWheelDisplay(true);
        }
        WeaponLoadoutSample loadout;
        const bool fresh=CopyWeaponLoadout(loadout);
        if(!fresh) loadout.loadout.valid=false;
        const int target=int(g_wheelEquip.target.kind)*10+g_wheelEquip.target.slot, presses=g_wheelEquip.presses;
        const auto result=g_wheelEquip.Step(loadout.loadout,g_equipMemory,now);
        if(g_wheelEquip.presses!=presses)
            Log("[WeaponWheel] equip target=%d (kind*10+slot): press %d buttons=0x%04X (hands right=%d left=%d; learned R/L/D/2H=%04X/%04X/%04X/%04X)",
                target,g_wheelEquip.presses,g_wheelEquip.bits,loadout.loadout.hand[0],loadout.loadout.hand[1],
                g_equipMemory.button[1],g_equipMemory.button[2],g_equipMemory.button[3],g_equipMemory.button[0]);
        if(result==d2vr_wheel::Equip::Result::Done)
            Log("[WeaponWheel] equip target=%d confirmed (hands right=%d left=%d)",target,loadout.loadout.hand[0],loadout.loadout.hand[1]);
        else if(result==d2vr_wheel::Equip::Result::Failed)
            Log("[WeaponWheel] equip target=%d FAILED after %d press(es) (hands right=%d left=%d fresh=%d)",target,presses,
                loadout.loadout.hand[0],loadout.loadout.hand[1],(int)fresh);
        gp.wButtons|=g_wheelEquip.Bits(now);
    }
    ApplyMenuTriggerConfirm(gp);
    if(g_crouchStickDown) {
        // Head owns pitch: right stick Y never reaches the game in gameplay.
        const float x=gp.sThumbRX/32767.f, y=gp.sThumbRY/32767.f;
        const bool allowed=!g_wheelGate.open && g_inputContext;
        d2vr_wheel::StickFlick::Dir active=d2vr_wheel::StickFlick::Dir::None;
        const auto fired=g_stickFlick.Update(x,y,allowed,now,active);
        if(fired==d2vr_wheel::StickFlick::Dir::Up)
            Log("[StickFlick] right stick up -> jump (buttons=0x%04X)",ReadPadSetting("JumpButton","A",XINPUT_GAMEPAD_A));
        else if(fired==d2vr_wheel::StickFlick::Dir::Down)
            Log("[StickFlick] right stick down -> crouch toggle (buttons=0x%04X)",ReadPadSetting("CrouchButton","R3",XINPUT_GAMEPAD_RIGHT_THUMB));
        if(g_inputContext) gp.sThumbRY=0;
        if(active==d2vr_wheel::StickFlick::Dir::Up) gp.wButtons|=ReadPadSetting("JumpButton","A",XINPUT_GAMEPAD_A);
        else if(active==d2vr_wheel::StickFlick::Dir::Down) gp.wButtons|=ReadPadSetting("CrouchButton","R3",XINPUT_GAMEPAD_RIGHT_THUMB);
    }
}

// Menus with gaze: the right trigger confirms like A (look + pull).
static void ApplyMenuTriggerConfirm(XINPUT_GAMEPAD& gp) {
    const bool menu=MenuConfirmActive();
    const bool down=gp.bRightTrigger>150;
    static bool wasDown=false;
    if(down && !wasDown) Log("[R2Route] menu=%d world=%d inputContext=%d stickY=%d buttonsBefore=0x%04X",
        int(menu),int(g_in3DGameplayMode),int(g_inputContext),int(gp.sThumbRY),unsigned(gp.wButtons));
    wasDown=down;
    if(menu && down) { gp.wButtons|=XINPUT_GAMEPAD_A; gp.bRightTrigger=0; }
}

static void ResetWeaponWheelInput() {
    if(g_wheelGate.open) CloseWheels(false);
    g_wheelGate.Reset(); g_stickFlick.Reset(); g_wheelAllowed=g_inputContext=false;
}
