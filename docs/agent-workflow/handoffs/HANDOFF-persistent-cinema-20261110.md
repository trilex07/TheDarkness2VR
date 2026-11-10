# Persistent Cinema/VR preference — CPU review candidate

Base: accepted producer `2567d04072e9a38c66faec017e3b2779471935e5`.
Worktree: `.persistent-cinema-20261110`, branch `codex/persistent-cinema-20261110`.
Scope: one product behavior, independent of A/B/C, horizon and simulator tooling.

## Actual limitation and change

Accepted `RequestOpposite` encodes an expiry of now+60000ms. `TemporaryOverride`
then returns Auto. The existing L3+R3 hold therefore cannot persist the user's
chosen mode across a long problem section or restart. This is source-proved UX
behavior, not a proposed explanation for horizon drift.

New `[VRSettings] CinemaMode=0` retains accepted automatic behavior. Values 1/2
select VR/Cinema until changed, including after restart. The Controls tab's third
row cycles Automatic -> VR -> Cinema -> Automatic and draws the saved state in
the selected UI language. The existing third-row space replaces its help text;
other buttons, hit rectangles, close, entry and neutral-release ownership remain.

The existing L3+R3 short recenter/long temporary override is preserved. A temporary
override takes precedence for its original 60 seconds, then returns to the saved
preference. Explicit UI selection cancels that temporary override immediately
only after successful INI write. Write failure restores the preference and
preserves both the published mode and temporary request. Missing/invalid values
use Auto. Render-side mode is published atomically after successful save.

The new mode uses the existing same-frame stereo cinema path, not a new renderer.
Scene cinema still requires SubmitMode=4, StereoMode=1 and the tracked render
frame. An explicit persistent selection enables the existing cinema mechanism
even if automatic `Render/CinematicTheater` was disabled. Auto with that switch
disabled remains disabled. Frontend menus keep their existing 2D menu handling;
the setting concerns gameplay presentation, not native menu navigation.

## Verification

CPU command (small fixtures only; no mod DLL build or GPU/runtime):

```powershell
./test/build_vr_settings.ps1 -OutputDir "$PWD/test-output/cinema-cpu"
```

Fresh terminal result: seven executables passed, 24268 assertions:

| Fixture | Checks |
|---|---:|
| Exact extracted production CinemaOverride/Enabled/Scene predicates | 35 |
| Actual panel input and laser | 5789 |
| Actual renderer, mocked XR/upload, real GDI including both-language labels | 1877 |
| Actual input host routing | 50 |
| Actual submission ownership | 7215 |
| Actual config/action persistence and write-failure rollback | 54 |
| Existing weapon laser gates | 9248 |

Persistence controls cover 60s/1h/1day, restart from real temporary INI, invalid
values, opposite temporary chord both ways, render thread/frame/mode refusal,
failed save preserving temporary state, and successful selection clearing it.
The test also demonstrates the accepted temporary-only policy expires at 60s.
It does not simulate a native game or establish headset comfort/visual quality.

Private evidence: `test-output/cinema-cpu.log`, `test-output/cinema-review.json`.
Captures/test outputs are ignored. No binaries, captures or native content are
committed. `git diff --check` passed. No install, game, input, GPU, XR or lease.

## Candidate and runtime limits

Product source implementation: READY FOR REVIEW. Product DLL build: NOT RUN
(preserve low-memory/source-only policy). Runtime and headset: NOT RUN. Horizon
root-cause fix: NOT IMPLEMENTED; this preference is not a drift correction.

Do not replace accepted39D with this source-only candidate. A later authorized
CPU product build must pin all inputs/artifact, then a separately granted runtime
must show the actual Settings row, persistent VR/Cinema switch in both eyes,
return to Auto, existing recenter/chord and restart persistence. A fresh rollback
must restore that session's original DLL/INI/settings/saves/XR/Steam; never use
historical saves. No A/B/C merge or installer changes are included.
