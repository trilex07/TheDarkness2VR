# Darkness2VR native menu fallback

The opt-in Flash cursor reported `REFUSED: external Steam COM method/private
owner table admission` on the installed x86 Steam game. The menu code still
forced Flash mouse mode. New Game reached the difficulty menu, where repeated
stock A presses did not proceed. The fail-closed right-trigger route also
could not produce A with an unavailable cursor.

Attempt cursor installation before requesting mouse mode. On refusal, retain
the native pad route, including stick navigation and A. The legacy cursor path
and an admitted native Flash cursor still use their existing confirmation
rules. The branch builds on the paired depth capture fix in `5c8e2179`.

## Verification

- `build.ps1 -DiagnosticCapture` produced x86 candidate SHA-256
  `E23CDE3B34B34AC7CFD88CBE82BC0C370502D51E614039144EA2E7EA6574D702`.
- `test/build_native_flash_cursor.ps1` passed 51,218 checks, including
  51,005 actual native transform comparisons. The expected legacy control
  failed its coordinate check.
- MAIN simulator PID 26812 using build `F7DD2B35` remained on the difficulty
  screen after stock A. Candidate PID 27212 logged the same cursor admission
  refusal. Stock A on New Game and difficulty led to brightness (native flags
  `4`). A confirmed brightness, and one down-stick pulse plus A declined the
  optional recap. The game then entered the opening and restaurant (flags `0`).
  Both launches used empty private save folders and the same installed INI.
- Local evidence in the simulator-fixes worktree under
  `captures/simulator-20261128/chapter-new-story` and
  `captures/simulator-20261128/chapter-new-story-menu-fallback`.
