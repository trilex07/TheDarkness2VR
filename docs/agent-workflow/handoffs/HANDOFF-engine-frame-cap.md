# Engine frame cap continuation

- Task: engine-frame-cap; role MAIN, offline candidate only.
- Branch: codex/engine-frame-cap; base f96659ee.
- Checkout: C:/Users/Trilex/vr-mods/Darkness2VR-engine-frame-cap.
- Original unfinished source remains intact in hud-head-tentacles. Its unrelated uncommitted gaze/weapon work is excluded. Managed worktree creation refused the out-of-workspace sourceRoot; a separate local shared clone isolates this change instead.

## Evidence and hypothesis

Installed EXE SHA256 D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1, preferred base 00400000. Fresh pefile/Capstone inspection on 2026-11-01 confirms:

- Graphics.MaxFrameRate: registration 00EAACC0, default float 60 at 00FC8034, live storage 010EB094.
- Graphics.FrameCapMode: initializer 00EAACA5 stores 0 at 010EB068. Reader 00798054 maps zero to output mode 2 at 0079805F; explicit mode 1 uses reciprocal MaxFrameRate at 00798087. Other predicates can force mode 1.
- Graphics.MaxLoadingFrameRate: registration 00EAAC42, default float 30 at 00F6E3B0, live storage 010EB03C.
- Graphics.VSyncMode: registration 00EA6AD2, default integer 0 written at 00EA6AF5 to 010E309C.
- Graphics.VSyncSlack: registration 00EA7AE2, default zero bits written at 00EA7B05 to 010E3C08. Type/units and consumer semantics not established.
- These registrations reference category Graphics at 00F38AD8. They do not establish a supported text configuration override; EE.cfg encoding remains unresolved.
- Main-loop virtual query at 00938A8C uses vtable offset EC. Mode 2 is converted conditionally: tests at 00938A94, 00938AA6, 00938AAD and 00938ABA matter. Reciprocal load/division at 00938AC0/00938AC8 is not unconditional gameplay proof.
- Deadline 00938AF3 onward subtracts elapsed time and 0.001 seconds, converts to milliseconds, then positive waits call 00998130 at 00938B62 (return 00938B67). Wrapper uses SleepEx(milliseconds, TRUE). Multiple callers exist.

Hypothesis: if the observed slow gameplay executes this reciprocal branch with effective 60, raising only MaxFrameRate should reduce waiting attributable to caller 00938B67. Disproof: branch absent, different effective value, or no matching waits. A 60 default alone does not explain ~63 FPS or promise 90 FPS.

## Candidate

[Render] EngineMaxFrameRate defaults to 0 (no read/write). OBSERVE logs effective MaxFrameRate/FrameCapMode once during a running XR session, without writing. AUTO waits for known physical refresh; numeric 30..1000 sets that cap. Strict parsing rejects trailing garbage/nonfinite/out-of-range values. Four instruction signatures, name/default and writable non-executable image section are verified before any access. An access exception disables further writes. No code patch/new hook. Restart to restore native configuration; no hot disable/restore protocol is offered.

Existing user-selected rendering/profile settings remain unchanged. Installed files untouched. The old source comment claiming measured ~63 FPS causality was removed.

## Verification / next gate

- MSVC x86 host parser/foreign-image-refusal test /W4 /WX PASS.
- tools/verify_engine_frame_cap.py against installed EXE: all four signatures, defaults, category, mode mapping and writable section PASS.
- Protected x86 DLL build PASS (build.ps1). Sandbox PDB LNK1101/hang resolved by stopping that local build and building outside the sandbox with a private MSPDB endpoint; no source workaround. DLL SHA256 AE64C072C02C9BAD6C63D05CADD3EC28528CC2A3205890BFD29340A728B7BF49.
- Steam runtime, active wait caller, FPS improvement and physical PSVR2: NOT TESTED. Game was not running during inspection; no launch/deployment performed.

Next controlled test: deploy a hash-backed candidate on the CURRENT accepted source integration (this isolated candidate excludes uncommitted eye/wheel changes), start with OBSERVE, then capture caller 00938B67 and wait arguments before/after fixed 90 or AUTO in the same scene. Values alone are not active-path proof. Preserve all accepted installed files/config/cache and verify restoration if unsuccessful.

Remote push blocked by automatic approval review pending explicit destination approval.
