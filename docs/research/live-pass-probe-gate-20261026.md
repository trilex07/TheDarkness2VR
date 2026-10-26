# Gate legacy per-upload PassProbe — isolated candidate

Final comparison base: `c86c2c8` (same rendering code as resource A/B).
This commit does not include the separate last-consumer reader-fence candidate.

## Proven redundant work

The full existing `A-runtime.log` from `protected-ref-ab-20261026T145711Z`
contains 128 distinct `[PassProbe]` lines. The old live VS hook keeps querying
RT0/GetDesc/Release and searching its observed-pass vector on every eligible c0
upload while that vector has fewer than 512 entries. Seeing a known pass stops
only the log, not these repeated diagnostic operations. Their result is used
only by the probe log; camera classification and projection do not consume it.
The measured cost in milliseconds and share of the remaining native-frame gap
are UNKNOWN. Old ETL costs are not substituted for this session's hotspot data.

## Six-line runtime correction

`Diagnostics.LivePassProbe` is read once from `Darkness2VR-diagnostic.ini`, default
0. The old probe block is unchanged behind that explicit opt-in. Normal live
recording/projection no longer executes its diagnostic COM reads or vector
search. Setting the option to 1 restores the old diagnostic behavior, including
its query repetition, unique-log key and 512-entry cap.

No matrix values, recording, shadow-pass classification, resource ownership,
shader creation/reuse, sync, input, FOV, resolution or image-write policy changed.
`ClampReadableBytes`/VirtualQuery and all other recording safety remain intact.
No INI/profile was edited. Rollback: revert this commit; opt-in restores the
previous diagnostic behavior without a renderer change.

## Actual-source regression and builds

The fixture compiles the current `Hook_SetVertexShaderConstantF` body extracted
without rewriting from `src/d3d9.cpp`, using mock device/surface APIs. Both
ordinary and `D2VR_MATCHED_CAPTURE` versions pass all four cases:

- Disabled: 1,001 original forwards; zero probe RT/Desc/Release/Viewport/log calls.
- Enabled: old repeated-query versus unique-log behavior, changed dimensions/
  shader key, error/null RT and descriptor failure behavior preserved.
- Cap: 600 eligible uploads yield exactly 512 RT/Desc/Release/Viewport/log calls.
- Stereo: original constants remain recorded; compensated forwarded constants,
  arguments and success/status/failing HRESULTs match across probe OFF/ON.

Production and diagnostic x86 renderer builds pass. Existing protected-profile
and startup tests pass (3/3), integration profiles pass (6/6), and original
stereo transfer-order regression passes (3/3) on the final comparison base.

Steam/HMD: NOT RUN by SIDE. This removes proven unnecessary diagnostic work;
it does not establish an FPS gain or explain all reported 30-50 FPS. A later
separate authorized comparison must measure the actual frame-time effect.
