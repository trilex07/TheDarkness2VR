# Diagnostic shadow atlas draw omission

`[Render] SkipShadowAtlasDraws=1` opts into an image-changing A/B test.
Default is 0; restart after changing it. This is not a production optimization.

The existing classifier recognizes an RGB565 square RT with a nonzero square
viewport smaller than the RT. It is a heuristic for the observed shadow atlas,
not proof of all shadow passes. Four common draw wrappers omit matching draws
in live and replay paths. Atlas clears, state changes, constant recording and
native CPU object preparation remain intact. No stale atlas is deliberately
reused or shadow sampling shader rewritten. Other/unrecognized shadow passes
can remain; lighting artifacts are possible. Do not interpret a small gain as
proof that the complete native shadow subsystem is cheap.

Compare the same location, facing the same cars/enemies, with the previous
profile. Check `[ShadowDrawTest] skippedLive=... skippedReplay=...` to establish
that omission actually occurred. Zero counters make the experiment inconclusive.
Report FPS and visible lighting changes. Roll back with 0 and restart or use
the deployment's rollback script.

Offline: production and diagnostic x86 builds PASS; actual four wrapper and
classifier regression PASS (default OFF, non-atlas forwarding, native failure
HRESULT, live and both replay eyes); actual replay float-loop regression PASS.
These do not prove scene classification, visual acceptance or FPS gain.
