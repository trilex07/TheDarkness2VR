# Handoff: comic texture storage and omitted Polish fonts

**Task ID**: texture-font-repair-20261101  
**Date**: 2026-11-01  
**Role**: MAIN  
**Branch**: `codex/texture-font-repair-20261101`  
**Worktree**: `C:/Users/Trilex/Darkness2VR/.main-camera-shake`  
**Baseline**: Git main `564f8342`; camera/model DLL9AF9A51E accepted. User rejects newly installed texture appearance (rainbow noise on demon arms/hands) and reports missing Polish letters in gameplay captions.

## Scope and status

Fix the data-only regressions; preserve accepted camera/haptics/model DLL,
configuration and new dialogue audio/text. Built private14-file repair under
`build/texture-font-repair-v3`; installation pending at initial source commit.

Read BASELINE, previous per-task asset handoff, relevant failure index and real
archive/payload bytes. No guessed native offsets or executable changes.

## Confirmed texture evidence

- Rejected texture manifest SHA256 `eb915af2260c9bef5189823752f916c3271ec6ecc462d7abca23d163da19752d`:59 BC1 +5 BC3 assets. Its encoder emitted 16KB planar chunks but installed them as uncompressed TOC entries (`packed==size`).
- Native original uncompressed `D2/Objects/Props/Items/Street/StreetSignsA_d.png` B.Texture21864 bytes decodes correctly as **standard interleaved BC1** in Pillow; applying planar conversion produces noise. Original compressed Grabby B.Texture extraction is planar and needs conversion. Rejected uncompressed Grabby interpreted directly reproduces rainbow noise; repaired standard blocks decode coherently.
- Fix retains encoded colors/alpha, converts BC1's two uint32 planes and BC3's2/6/4/4-byte planes into standard blocks, preserving H headers already configured for uncompressed F streaming. No texture art regeneration.
- Independent Pillow native BC decoder compared all64 repaired B top mips against intended PNGs:64/64 PASS, maximum mean absolute RGB error7.837/255 (DarknessStaff). Report `build/texture-font-repair-v3/image-audit.json`.
- Diagnostic sheets: `test-output/texture-font-repair/{dxt-layout,native-uncompressed}.png`.

## Confirmed font evidence

- Installed six font families had18 Polish codepoints each. **19 other Latin fonts had none**:10 Corbel,5 Arial, Requiem, YouMurderer, CommentFont and DebugFont. This omission supports the user's gameplay caption symptom; actual font used by a particular caption was not instrumented.
- Native compressed Corbel26 atlas is BC2,1024x64,65536 decoded bytes, four16384-byte compression chunks. Each chunk has **four separate uint32 planes**, including two separate alpha halves. Eight-byte alpha-plane and BC3 assumptions decode incorrectly. Four-plane conversion produces legible original glyphs. Already-fixed six atlases are uncompressed standard BC2 and remain unchanged.
- Added18 visibly accented glyphs to each omitted Latin font,342 total. Extend English atlas height, preserve original block bytes exactly, rescale English V coordinates, preserve metrics and auxiliary-language bodies. Native H count arrays paired32 bytes apart are updated for language0 only (offsets recorded individually in manifest). Texture suffix repeats dimensions at2/4 and6/8; both height fields updated.
- Build verifies original decoded atlas region unchanged, all old metrics and auxiliary languages exact, all new glyphs nonempty after independent Pillow BC2 decode. Corbel26/Arial24 proof banners visually inspected.

## Tools and rollback

- `tools/probe-texture-font-assets.py`: read-only selective private extraction via existing QuickBMS. Only diagnostic assets remain local and are never committed.
- `tools/build-texture-font-repair.py`: offline build from current archives, extracted fonts and rejected texture payloads. Four Japanese-only font bodies skipped; all25 Latin families now have Polish glyphs.
- `tools/deploy-asset-repair.py`: hash-gated14-file transactional install, verified preimages, exact rollback,13 protected DLL/EXE/INI/audio/text identities.
- Current bad package's prior verified archive preimages remain in `C:/Users/Trilex/AppData/Local/Darkness2VR/backups/polish-comic-main-20261101-accepted-camera`. Do not blindly rerun old comic installer or overwrite the private source packages; they remain rejected evidence.

## Next steps

Install with game closed and verify rollback backups and installed hashes.
Startup smoke and user headset acceptance are separate from offline proof.
User should inspect hands/tentacles/weapons and gameplay captions containing
Polish letters. Audio/camera/haptics must remain unchanged.
