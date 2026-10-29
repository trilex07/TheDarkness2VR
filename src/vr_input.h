#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace d2vr_input {
// Automatic support is a named action, independent of grip/tentacle actions.
// Distance uses controller grip centres; hysteresis prevents contact chatter.
struct HandSupport {
    bool active=false;
    double entered=-1;
    void Reset() { active=false; entered=-1; }
    bool Update(double time,bool valid,bool offHandEquipped,float distance) {
        if(!valid || offHandEquipped || !std::isfinite(distance) || distance>.26f) Reset();
        else if(!active) {
            if(distance>.18f) entered=-1;
            else if(entered<0 || time<entered) entered=time;
            else if(time-entered>=.12) active=true;
        }
        return active;
    }
};
struct Vec3 { float x=0, y=0, z=0; };
inline float Length(Vec3 v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
inline Vec3 Difference(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }

// HUD-only render segments interleave with world segments. Require a sustained
// sequence of recent world observations, not consecutive EndScene calls.
// This is render activity, NOT proof of native gameplay/pause/cutscene state.
struct WorldActivity {
    bool active=false, observing=false;
    double first=0,last=0;
    bool Update(bool world, double time) {
        if(observing && (time<last || time-last>.5)) { observing=false; active=false; }
        if(world) {
            if(!observing) { first=time; observing=true; }
            last=time;
            if(time-first>=.15) active=true;
        } else if(active && (!observing || time-last>.5)) active=false;
        return active;
    }
};

// A zone must be left after startup/tracking loss before it can fire. Dwell,
// hysteresis and sample continuity prevent repeated actions or recovery spikes.
struct DwellZone {
    bool armed=false, inside=false;
    double entered=0;
    bool Update(bool valid, float distance, double time, float enter, float exit, double dwell) {
        if (!valid || !std::isfinite(distance)) { *this={}; return false; }
        if (distance > exit) { armed=true; inside=false; return false; }
        if (!armed) return false;
        if (distance > enter) { inside=false; return false; }
        if (!inside) { entered=time; inside=true; }
        if (time-entered < dwell) return false;
        armed=false; inside=false; return true;
    }
};
// Defer a short menu tap until release; a continuous one-second hold recenters
// once and consumes the menu tap. Reacquisition requires a fresh release.
struct RecenterHoldTap {
    struct Result {bool tap=false,recenter=false;};
    bool armed=false,holding=false,fired=false;
    double began=0,last=-1;
    Result Update(bool active,bool down,double time) {
        if(!active || !std::isfinite(time) || (last>=0 && (time<last || time-last>.25))) {
            *this={};return {};
        }
        last=time;
        if(!down) {
            const bool tap=holding && !fired;
            holding=fired=false;armed=true;
            return {tap,false};
        }
        if(!armed) return {};
        if(!holding) {holding=true;began=time;}
        if(!fired && time-began>=1.0) {fired=true;return {false,true};}
        return {};
    }
};
// Left grip is an explicit modifier for the missing stock Y action. Choose
// action/menu ownership once on the triangle press, so releasing grip cannot
// turn an execution-button hold into pause/recenter. Tracking loss requires
// a fresh triangle release; the native game selects the actual Y action.
struct ContextTriangle {
    struct Result {bool action=false,tap=false,recenter=false;};
    RecenterHoldTap menu;
    bool armed=false,holding=false,actionMode=false;
    double last=-1;
    Result Update(bool active,bool down,bool modifier,double time,bool holdRecenter=true) {
        if(!active || !std::isfinite(time) || (last>=0 && (time<last || time-last>.25))) {
            *this={};return {};
        }
        last=time;
        if(!down) {
            const auto result=menu.Update(true,false,time);
            holding=actionMode=false;armed=true;
            return {false,result.tap,result.recenter};
        }
        if(!armed)return {};
        if(!holding){holding=true;actionMode=modifier;}
        if(actionMode){menu={};return {true,false,false};}
        if(!holdRecenter)return {false,true,false};
        const auto result=menu.Update(true,true,time);
        return {false,result.tap,result.recenter};
    }
};
struct RecenterChord {
    double began=0;
    bool holding=false, fired=false;
    bool suppress=false;
    bool Update(bool left, bool right, double time) {
        if(!left && !right) { suppress=holding=fired=false; return false; }
        // A chord is a click, not a dwell gesture. Latch until BOTH buttons are
        // released so uneven release/re-press cannot recenter twice.
        if (!left || !right) return false;
        suppress=true;
        if (!holding) { holding=true; began=time; }
        if (!fired) { fired=true; return true; }
        return false;
    }
};
struct Motion {
    Vec3 last{}, velocity{};
    double lastTime=0;
    bool valid=false;
    void Update(bool tracked, Vec3 position, double time,const Vec3* locatedVelocity=nullptr) {
        tracked=tracked && std::isfinite(time) && std::isfinite(position.x) &&
            std::isfinite(position.y) && std::isfinite(position.z);
        const double dt=time-lastTime;
        velocity={};
        if (tracked && valid && dt>=.002 && dt<=.1) {
            Vec3 d=Difference(position,last);
            // Reject reacquisition jumps, never turn them into a throw/slash.
            if (Length(d)<.5f) {
                velocity={float(d.x/dt),float(d.y/dt),float(d.z/dt)};
                // Runtime velocity belongs to this located grip, base space
                // and predicted time. Avoid differentiating predicted poses
                // across uneven game frames when the runtime supplies it.
                // Recovery/gap/jump gates apply to either velocity source.
                if(locatedVelocity && std::isfinite(locatedVelocity->x) &&
                   std::isfinite(locatedVelocity->y) && std::isfinite(locatedVelocity->z) &&
                   std::isfinite(Length(*locatedVelocity)))velocity=*locatedVelocity;
            }
        }
        valid=tracked; last=position; lastTime=time;
    }
};
struct Slash {
    bool armed=false;
    double lastFire=-10;
    bool Update(bool tracked, bool grip, float speed, double time) {
        if (!tracked || !grip) { armed=false; return false; }
        if (speed<.35f) armed=true;
        if (armed && speed>1.2f && time-lastFire>.45) {
            armed=false; lastFire=time; return true;
        }
        return false;
    }
};

enum class GrabFeedback { Unknown, Empty, Busy };
// Keep the powered part of a stroke, rather than the braking/rebound sample
// present when squeeze finally crosses its release threshold. Fixed storage;
// time weighting makes selection independent of XR refresh rate.
struct ThrowStroke {
    struct Sample { Vec3 velocity{},aim{}; double time=0,dt=0; };
    Sample samples[32]{};
    int count=0;
    void Reset() { count=0; }
    void Add(Vec3 velocity,Vec3 aim,double time,double dt) {
        const float speed=Length(velocity);
        if(!std::isfinite(speed) || speed<.8f || speed>15 || dt<=0 || dt>.1) return;
        if(count==32) { for(int i=1;i<32;++i) samples[i-1]=samples[i]; --count; }
        samples[count++]={velocity,aim,time,dt};
    }
    bool Select(double time,Vec3& velocity,Vec3& aim) const {
        float best=0;
        for(int i=0;i<count;++i) {
            const auto& peak=samples[i]; const double age=time-peak.time;
            if(age<0 || age>.18) continue;
            const float speed=Length(peak.velocity);
            Vec3 sum{}; double weight=0;
            for(int j=0;j<count;++j) {
                const auto& s=samples[j];
                if(time-s.time<0 || time-s.time>.18 || std::fabs(s.time-peak.time)>.035) continue;
                const float n=Length(s.velocity);
                const float dot=(s.velocity.x*peak.velocity.x+s.velocity.y*peak.velocity.y+s.velocity.z*peak.velocity.z)/(n*speed);
                if(dot<.65f) continue;
                sum.x+=s.velocity.x*float(s.dt); sum.y+=s.velocity.y*float(s.dt); sum.z+=s.velocity.z*float(s.dt);
                weight+=s.dt;
            }
            if(weight<.018) continue; // reject one-frame high-frequency spikes
            const Vec3 mean={sum.x/float(weight),sum.y/float(weight),sum.z/float(weight)};
            const float score=Length(mean)*float(1-.3*age/.18);
            if(score>best) { best=score; velocity=mean; aim=peak.aim; }
        }
        return best>=.6f;
    }
};
// Assisted throws follow the controller at the powered stroke. A deliberately
// lower assist retains physical velocity aiming. Neither input uses head gaze.
inline bool ThrowDirection(Vec3 velocity,Vec3 aim,float assist,Vec3& out) {
    const float speed=Length(velocity),a=Length(aim);
    if(!std::isfinite(speed) || speed<.8f || !std::isfinite(assist)) return false;
    assist=std::max(0.f,std::min(1.f,assist));
    if(!std::isfinite(a) || a<.5f || a>1.5f) assist=0;
    out={velocity.x/speed*(1-assist),velocity.y/speed*(1-assist),velocity.z/speed*(1-assist)};
    if(assist>0) { out.x+=aim.x/a*assist; out.y+=aim.y/a*assist; out.z+=aim.z/a*assist; }
    const float length=Length(out);
    if(!std::isfinite(length) || length<.01f) return false;
    out={out.x/length,out.y/length,out.z/length}; return true;
}
// Busy includes native acquisition/attachment, not just a fully settled hold.
struct TentacleOutput {
    bool grab=false, throwObject=false, slash=false;
    Vec3 throwVelocity{},throwAim{}; // powered stroke and controller aim, frozen at release
    bool leftButton=false, rightButton=false, ownsRightStick=false;
    float slashX=0,slashY=0;
};
struct Tentacles {
    bool leftArmed=false,leftDown=false,holdingIntent=false,rightArmed=false,rightDown=false;
    bool pauseDown=false,paused=false,rightReady=false;
    bool awaitingThrowAck=false;
    double lastTime=-1,leftUntil=0,throwAt=0,recentMotion=-10;
    double slashStart=-10,lastSlash=-10;
    double grabAt=-10,ignoreFeedbackUntil=0;
    float strokeX=0,strokeY=0;
    Vec3 slashTravel{};
    Vec3 grabPosition{};
    Vec3 recentVelocity{},pendingThrowVelocity{};
    Vec3 pendingThrowAim{};
    ThrowStroke throwStroke;
    TentacleOutput Update(bool context,bool pause,bool leftValid,bool rightValid,
        float leftGrip,float rightGrip,Vec3 leftPosition,Vec3 leftVelocity,
        Vec3 rightVelocity,double time,GrabFeedback feedback=GrabFeedback::Unknown,Vec3 leftAim={}) {
        TentacleOutput out;
        if(pause && !pauseDown) paused=!paused;
        pauseDown=pause;
        const double dt=lastTime<0 ? 0:time-lastTime;
        const bool clockOK=std::isfinite(time) && (lastTime<0 || (dt>=0 && dt<=.15));
        lastTime=time;
        if(!context || pause || paused || !clockOK) {
            leftArmed=leftDown=holdingIntent=rightArmed=rightDown=rightReady=false;
            leftUntil=throwAt=0; recentMotion=slashStart=lastSlash=-10;
            slashTravel={};
            grabAt=-10; ignoreFeedbackUntil=0;
            awaitingThrowAck=false;
            recentVelocity=pendingThrowVelocity={};
            pendingThrowAim={}; throwStroke.Reset();
            return out;
        }
        if(!leftValid) {
            leftArmed=leftDown=holdingIntent=false; leftUntil=throwAt=0; recentMotion=-10;
            grabAt=-10; ignoreFeedbackUntil=0;
            awaitingThrowAck=false;
            recentVelocity=pendingThrowVelocity={};
            pendingThrowAim={}; throwStroke.Reset();
        } else {
            // Give the native animation/query time to acquire. Never retry on a
            // timer: an EMPTY acknowledgement only permits the next grip press.
            // A delayed native BUSY after a throw is still the old object.
            // It cannot silently become a new hold. A deliberate grip press
            // can retry a failed throw; EMPTY acknowledges a completed one.
            if(awaitingThrowAck && feedback==GrabFeedback::Empty && time>=leftUntil) awaitingThrowAck=false;
            if(!awaitingThrowAck && feedback==GrabFeedback::Busy && time>=ignoreFeedbackUntil) holdingIntent=true;
            if(feedback==GrabFeedback::Empty && time-grabAt>=.65 &&
               time>=ignoreFeedbackUntil && time>=leftUntil && !throwAt) holdingIntent=false;
            if(leftGrip<.35f) leftArmed=true;
            const bool down=leftGrip>(leftDown ? .35f:.65f);
            if(leftArmed && down && !leftDown && !throwAt) {
                if(awaitingThrowAck) {
                    holdingIntent=feedback==GrabFeedback::Busy;
                    awaitingThrowAck=false;
                }
                if(!holdingIntent) {
                    out.grab=true; leftUntil=time+.12; holdingIntent=true; grabAt=time;
                }
                grabPosition=leftPosition; recentMotion=-10;
                recentVelocity=pendingThrowVelocity={};
                pendingThrowAim={}; throwStroke.Reset();
            }
            if(leftDown && Length(leftVelocity)>.8f &&
                Length(Difference(leftPosition,grabPosition))>.12f) {
                recentMotion=time; recentVelocity=leftVelocity;
                throwStroke.Add(leftVelocity,leftAim,time,dt);
            }
            Vec3 stroke{},strokeAim{};
            if(leftArmed && leftDown && !down && holdingIntent && throwStroke.Select(time,stroke,strokeAim)) {
                // A second press throws. Always expose a neutral gap to engine polls.
                throwAt=(time>leftUntil+.08 ? time:leftUntil+.08);
                pendingThrowVelocity=stroke; pendingThrowAim=strokeAim;
            }
            leftDown=leftArmed && down;
            if(throwAt && time>=throwAt) {
                out.throwObject=true; leftUntil=time+.12; throwAt=0; holdingIntent=false;
                out.throwVelocity=pendingThrowVelocity;
                out.throwAim=pendingThrowAim;
                recentVelocity=pendingThrowVelocity={};
                pendingThrowAim={}; throwStroke.Reset();
                ignoreFeedbackUntil=time+.45;
                awaitingThrowAck=true;
                recentMotion=-10;
            }
            out.leftButton=time<leftUntil;
        }
        if(!rightValid) {
            rightArmed=rightDown=rightReady=false; slashStart=-10; slashTravel={};
        } else {
            if(rightGrip<.35f) rightArmed=true;
            const bool wasDown=rightDown;
            rightDown=rightArmed && rightGrip>(rightDown ? .35f:.65f);
            // Only planar strokes have an unambiguous stock-game slash direction.
            const float planar=std::sqrt(rightVelocity.x*rightVelocity.x+rightVelocity.y*rightVelocity.y);
            if(!rightDown) { rightReady=false; slashTravel={}; }
            if(rightDown && !wasDown) { rightReady=true; slashTravel={}; }
            // A new grip can start in motion. Rearm at a soft slowdown OR on a
            // deliberate reverse stroke; no mandatory full stop between slashes.
            if(rightDown && time-lastSlash>.28 && (planar<.45f ||
                (planar>.75f && (rightVelocity.x*strokeX+rightVelocity.y*strokeY)/planar<-.25f))) {
                if(!rightReady) slashTravel={};
                rightReady=true;
            }
            if(rightDown && rightReady) {
                if(planar<.45f) slashTravel={};
                else {
                    slashTravel.x+=rightVelocity.x*float(dt);
                    slashTravel.y+=rightVelocity.y*float(dt);
                }
            }
            if(rightDown && rightReady && planar>.75f && Length(slashTravel)>=.04f && time-lastSlash>.28) {
                rightReady=false; lastSlash=slashStart=time; out.slash=true;
                slashTravel={};
                strokeX=rightVelocity.x/planar; strokeY=rightVelocity.y/planar;
            }
            const double age=time-slashStart;
            if(age>=0 && age<.18) {
                // RB precedes direction, remains held through a neutral recovery.
                out.rightButton=out.ownsRightStick=true;
                if(age>=.03 && age<.15) { out.slashX=strokeX; out.slashY=strokeY; }
            }
        }
        return out;
    }
};
}
