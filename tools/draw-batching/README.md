# Bounded draw eligibility audit

This tool only reads existing `run_*_trace.txt` five-argument dumps. It does not
capture, profile, emulate D3D or propose safe merges. Counts describe attempted
commands. UNKNOWN is preserved; every merge admission remains blocked because
this format omits essential payload, primitive-count, query and resource history.

Run from this checkout:

```powershell
python tools/draw-batching/audit_draws.py C:\Users\Trilex\Darkness2VR\test-output\run_43052_frame_003_trace.txt --output "$env:TEMP\draw-eligibility.json"
python -m unittest discover -s tools/draw-batching -v
```

The committed result adds an `inspection` section with private shader provenance
and manually checked candidate runs; the command regenerates the core audit.
Never compare arena offsets as constant values, treat unknown state as defaults,
or interpret `proven_safe_merge_draws=0` as proof that actual eligibility is zero.
No game data is needed for synthetic regressions. See the decision report under
`docs/research/gpu-driven-rendering-decision-20261027.md` for evidence limits.
