# Sliding projection estimator

The WVP upload path recomputed two insertion sorts over the same sliding eight
accepted projection samples at every publication. Maintain two sorted multisets
incrementally instead. The ring still advances on every accepted input; invalid
inputs do not advance it. Publication still occurs after every accepted input
from the third sample onward, including unchanged samples, because an exact
inverse-projection observation can overwrite the published estimate meanwhile.

This changes CPU-local arithmetic only. Device calls, projection sample order,
HMD poses, eye transforms, validity publication and all input remain unchanged.

Validation: `tools/stereo-math-tests/build-projection-estimate.ps1` compiles the
previous production publisher extracted from commit `4665e3f` and compares exact
float bits and publication readiness after every upload, including external
output overwrites, startup, ring wrap, repeated samples, boundaries, infinities
and NaNs. PASS: 6,291,500 comparisons. Existing actual-source replay regressions:
58,510 checks in each of production and diagnostic x86. Both full x86 DLL builds
passed.

Synthetic optimized x86 benchmark, best of seven runs of 1,048,576 uploads:
constant inputs 17.79 -> 4.92 ns/upload, cycling inputs 18.18 -> 6.94,
pseudorandom inputs 98.48 -> 42.42. These are CPU fixture timings, not game FPS.
Even 1,000 uploads save only about 0.01-0.06 ms in this fixture: this is a bounded
small improvement, not the substantial remaining outdoor bottleneck or evidence
of reaching 90 Hz. No new profiler or driver-state cache was introduced.
