"""Source contract for the outdoor live-phase CPU change (side/outdoor-recorder-lock)."""
import pathlib, re, sys
root = pathlib.Path(__file__).resolve().parents[1]
sync = (root / "src/recorder_sync.h").read_text(encoding="utf-8")
lock = (root / "src/recorder_lock.h").read_text(encoding="utf-8")
d3d9 = (root / "src/d3d9.cpp").read_text(encoding="utf-8")
replay = (root / "src/stereo_replay.inl").read_text(encoding="utf-8")
fails = []
def need(cond, msg):
    if not cond: fails.append(msg)
need('#include "recorder_lock.h"' in sync, "recorder_sync.h must include recorder_lock.h")
need("inline Lock mutex;" in sync, "recorder mutex must be d2vr_record_sync::Lock")
need("SRWLock" not in sync and "SRWLOCK" not in sync.split("//")[0], "no SRW calls left on the recorder mutex")
need(sync.count("mutex.lock()") == 2 and sync.count("mutex.unlock()") == 3,
     "Acquire/Resume lock; Release/RecoverReplayException/DriverScope unlock")
need("WaitForSingleObject(h,INFINITE)" in lock and "CreateSemaphoreW" in lock,
     "contended waiters must block in the kernel, not spin unbounded")
need(re.search(r"for\(unsigned spin=0;spin<\d+;", lock) is not None, "spin phase must be bounded")
need("exchange(0,std::memory_order_release)==2" in lock, "unlock must wake when waiters are announced")
need("static const d2vr_replay::StereoParams& EyeParamsRef(int eye)" in d3d9, "EyeParamsRef accessor")
vs = d3d9[d3d9.index("static thread_local float s_tmpConst[d2vr_replay::kVertexStereoUploadRows * 4];"):][:1200]
need("EyeParamsRef(0)" in vs and "MakeEyeParams(0)" not in vs, "VS upload hook must not copy StereoParams")
need("monoParams.separation = 0.0f" in vs, "mode-2 mono override preserved")
need("EyeParamsRef(0))) {" in replay, "PS constant hook must not copy StereoParams")
build = d3d9[d3d9.index("static d2vr_replay::StereoParams MakeEyeParams(int eye) {"):][:900]
need("g_frameEyeParamsValid ? g_frameEyeParams[eye ? 1:0] : BuildEyeParams(eye)" in build, "MakeEyeParams unchanged")
need("built[e]=BuildEyeParams(e);" in build, "EyeParamsRef must rebuild identically when no frame cache")
if fails:
    print("\n".join("FAIL: " + f for f in fails)); sys.exit(1)
print("recorder lock / eye params contract: ok")
