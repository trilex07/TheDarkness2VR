

## 2026-11-17: optional movement-stick outer range

Source-only candidate adds [Input] MoveStickOuterDeadzonePercent (default 0)
and bounded [Diagnostics] MovementStickProbe (default off). Identical radial
math/test corpus is shared with the other two public Trilex mods. Reduced Pico
north amplitude is a hypothesis pending raw controller evidence; no root cause
or headset fix is claimed. This does not change the separate VDXR startup path.
Offline tests/build PASS; runtime NOT RUN. Activation, limits and validation:
docs/PICO-MOVEMENT-STICK-RANGE-20261117.md.
