# Shader register validity updates

The active recorder and replay maintain a raw device-wide VS float bank.
`Bank::Write` previously marked each register with its own 64-bit shift/OR;
`Invalidate` similarly looped over each row. A 68-row palette therefore executed
68 bitmap updates despite touching at most three of the four bitmap words.

The implementation now updates each intersected bitmap word once. Payload copy,
range rejection, unknown-row handling, raw/adjusted separation and locking are
unchanged. No permission, pointer, shader or driver state is cached. Every valid
range touches at most four words, including zero-length and full-bank writes.
The private helper is called only after the existing bounds checks. Its shifts
are restricted to 0..63, including the complete 64-row case.

Validation uses the actual header at baseline `1ff0500` under a renamed namespace,
not a reimplementation. All 33,153 legal ranges, random initial masks, 100,000
mixed writes/invalidations, invalid bounds/bytes/null input, matrix reads and
record/replay history transitions pass 661,419 equivalence checks. Five further
checksum comparisons are performed for each benchmark mode.

Optimized x86 CPU fixture, five-run medians for 1,048,576 synthetic packets:

- Write only: baseline 68.638 ms, current 17.646 ms.
- Write plus invalidation: baseline 133.304 ms, current 27.271 ms.

The synthetic equal-frequency sizes are 1, 4, 6, 17, 68, 64 and 256 rows, with
varied valid starting registers. This is a function benchmark, not a captured
game workload. It does not establish frame savings or improved FPS. In
particular it is not evidence of the approximately 1 ms/frame saving sought
from higher-impact native producer changes, or a solution for right-eye flicker.
The retained aggregate RecPush samples cannot isolate this bank's cost.

Existing actual-source tests also pass: projection history 143,200 checks,
recorder/copy 6,133 checks and both replay constant-window fixture flavors
(58,286 checks, 576,096 records each). Production and diagnostic x86 DLLs build.
No game, SteamVR setting or installed candidate was changed by this optimization.
