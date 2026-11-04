# Handoff: VR settings and Polish fonts

Date: 2026-11-04. MAIN. Task ID: vr-settings-polish-fonts-20261104.
Branch: codex/vr-settings-polish-fonts-20261104.
Worktree: C:/Users/Trilex/Darkness2VR/.main-camera-shake.
Base source d1140a32; installed accepted4A/3A/68EE (see BASELINE).

## Goal / plan
User requests menu entry VR settings, separate Polish voice/text, NIS toggle
and percentage, supersampling selection and controls information. Then observe
and repair remaining Polish glyph defects in emulator. No further gaze-SS work.

Reuse existing compositor panel/input ownership and Singularity synthetic menu
entry pattern. Add menu-only entry (exact native movie identities), localized
Unicode settings pages. NIS live; resolution on restart. Language files are
already-loaded caches: switch only after owning process exits via a bounded,
hash-checked helper and verified EN/PL profiles; never copy live archives.
No arbitrary game memory offsets, native menu coordinate assumptions or image
patches. Existing camera/input/CPU fixes retained; new material experiments OFF.

First inspect installed glyph assets and emulator title/menu, preserve original
UV/metrics/atlas dimensions/non-PL pixels. Rejected atlas resizing is indexed in
FAILED-APPROACHES; prior font-metrics-six-v1 acceptance is not current correctness.

## Status
IMPLEMENTED, emulator tested and installed to Steam. Source324a2278.
Source read: BASELINE, WORKFLOW, failed approaches, prior font repair handoff,
vr_settings.h/renderer, native action movie owner, polish cache tools and
Singularity spikes/view_matrix settings/pause entry implementations.

## Remaining acceptance
User headset check of menu pointer/legibility and dialogue captions. Emulator
results and exact source/deployment evidence below. No headset acceptance claim.

## Implemented and tested (2026-11-04)
Menu-only VR settings entry uses exact retained MainMenu.swf plus existing pause
state; stable panel layer index, trigger laser, neutral-release input ownership.
Unicode pages Obraz/Jezyk/Sterowanie. NIS live updates global/foveated strength.
SS scales each axis100/125/150% on restart, skips unsupported >4096 or >16M pixels.
Independent Polish text/voice: hidden helper validates owner PID/path/creation,
waits exit, mutex and8 exclusive handles before writes, hashes/backup/readback/
ordinary-error rollback. Private EN/PL profiles, no copyrighted assets in Git.
EN Languages.cl Shared_Back checked; original voices from private
runtime/polish-side-completion/baseline. No power-loss recovery claim.

## Proven font cause
Official Cache.DLC/PCPatchManifests/Cache.Windows overrides25 native font pairs.
DLC includes PL codepoints but stale PL UV/metrics against repaired base atlases.
For Corbel19/26, darknessheader19/32: all18 PL UVs differ, non-PL UVs identical.
Evidence test-output/vr-settings-fonts-20261104/dlc-font-comparison.json.
Synchronize all25 complete H/B.Font pairs from base to DLC. No atlas resize,
texture mutation, generated geometry, EXE patch or guessed transform.
Rejected/rolled back: base font alias substitutions and menu font reference
experiment in H.AnimRetarget; neither corrected objective text. All base Font
and AnimRetarget bytes restored. Experiments only under private build/.

## PASS evidence
Both x86 builds; diagnostic DLL95BB1C3706D95CA0AE4A659BC497216E8C1EC757AED7CE692FA61B0BA122F286.
Settings4555, renderer995 (GDI1->1), actual host50, submission7215 checks;
actual XInput bridge207 production/261 diagnostic. Hidden language helper4 EN/PL
combinations, wait-before-exit and tampered-profile rejection passed.
Evidence test-output/vr-settings-fonts-20261104/tests-v4 and language-tests-v2.
MAIN emulator PSVR2 output2676x2728: main and pause entry, all3 tabs, NIS off/on
100->90->100, voice EN->PL, text EN then PL. Actual helper applied English text
with PL voices, PRESS ENTER KEY title confirmed. Restart SS125 native geometry,
depth and output3345x3410 verified in ss125-restart.log. Cycle100->125->100 and
right-secondary close confirmed final-emulator.log. Fixed layer index avoids
simulator stale-surface rejection when hover adds laser before click.
Font A/B: polish-menu-zoom.png BEFORE vs dlc-font-after.png AFTER confirms missing
letters restored in objective. Test-only FOV .38rad zoom, then normal PSVR2 FOV.
Headset/exhaustive dialogue-font inspection NOT RUN; no FPS benefit claim.

Session evidence test-output/vr-settings-menu-v2-20261104. Owned game PID/path/
start checked each stop. Simulator override restored; all4 user save/config
files restored against user-backup.json. Final SS100/PL/PL/NIS100 defaults,
accepted diagnostic profile retained; collectors/simulator OFF.

## Packaging
Source tools/prepare-vr-settings-candidate.py freezes33 targets with original
rollback, EXE/loader/base font/texture/script protection. Installer validates
preimages, saves current files before writes, restores on ordinary errors.
New profile files removed individually on rollback; transactions preserved.
Package build/vr-settings-fonts-candidate-20261104/Install.ps1.
Frozen source324a227841156ed9c9d84718460d0f7120f06d68. Install VerifyOnly PASS,
full33-file Restore PASS, restored baseline VerifyOnly PASS, reinstall PASS,
final VerifyOnly PASS. installed.json records INSTALLED, simulator OFF.
Game remains closed, production runtime restored. User saves/config4 exact hashes
confirmed in test-output/vr-settings-fonts-20261104/final-user-files.json.
