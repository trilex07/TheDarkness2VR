# Selected player ownership and independent arm calibration

## Fixed

The user-run log from the installed `05919dd` candidate repeatedly rejects
`ProtectedViewOwner` with `AvatarType`. The original `985F75` getter supplies
a scene reference, not an avatar. Its result cannot activate the protected
hands, aim, throw or held-enemy context.

The owned function adapter now preserves the original RegionMgr receiver.
It verifies that this is the manager from the selected renderer's context,
then uses the separate native local-player getter while retaining its returned
strong reference through all camera/throw/hand preparation. The original scene
getter, output reference, comparison, stack cleanup and native scene-reference
release remain unchanged. No native type checks were relaxed.

The two human arms now initialize calibration and retain pristine poses
independently. An invalid peer wrist/socket or a native animation update to only
one arm no longer rejects or recalibrates the other arm from already solved IK.
Resource identity is included in both calibration and pose-cache keys.

## Native contract

- Exact installed EXE: `d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
- `985E6D`: context virtual `+CC`, concrete `603ED0`, reads context `+64` reference.
- `985F75`: RegionMgr virtual `+174`, concrete `B0EF20`, retains manager `+1C8` **scene** reference.
- RegionMgr `FC7D00` virtual `+18C`, concrete `964570`, selects the local Player
  using `+5C`, then `43BCB0` retains Player `+74` avatar reference.
- `964570`, length `0x56`, SHA-256:
  `0464a12d76e2f874a147fb000bc1adf928510143079a82e053fb944c3bdd471d`.
- Static slots and getter code are admitted once. Each callback rereads the
  selected ownership graph. Native strong-count/weak-count release semantics
  are retained; no raw avatar address is cached as ownership proof.

Read-only Ghidra exports with matching EXE identity and final completion marker:
private `d2vr-owner-route-audit/view-getter-receiver.txt`,
`selected-player-getters.txt`, and `native-skin-matrix-pack.txt` under local TEMP.
Native bytes/decompilation remain outside Git.

## Validation

- x86 owner graph: 4,167 checks PASS; actual callback: 119 checks PASS.
- Actual private adapter: 1,000 forwarded scene getters, distinct original
  receiver preserved even when the getter clobbers ECX, x87/SSE/LastError,
  empty/nonempty comparison and stack cleanup PASS; source unchanged.
- Actual protected hands plus exact private `B1E500` arithmetic: 58,145 checks
  PASS. Nonidentity bind poses and mesh remap, both independent packed outputs,
  six rotating pose/skin slots, native animation updates and repeated IK tested.
  This is a fixture mesh, not proof of actual rendered mesh membership.
- `test/native_aim_getter_contract.py`: full EXE identity, distinct selected
  scene/local-player call chain, code digest and immediate aim consumer PASS.
- Production and diagnostic x86 DLL builds PASS.

## Integration and remaining work

Build diagnostic with `build.ps1 -DiagnosticCapture`; do not use `-Install`
because it can replace the current quality configuration. MAIN alone deploys
the DLL with a preserved rollback while the game is closed. Keep the existing
2676x2728 per-eye profile and protected feature flags. This change requires no
INI modification, EXE patch, additional debug-register hook or Steam launch.

Runtime activation of the corrected local-player route, visible controller
hands and precise throws remain unverified. Independent weapon shooting is a
separate missing protected path. Right-eye flicker, enemy-grab crash and stable
90 FPS are not claimed fixed by these changes. CULL acceptance is unchanged.

The retained user-run log proves the earlier native caller-stack copy path was
used (650,955 active-stack copies, zero recorded direct-copy faults). It does
not establish an FPS improvement or eliminate the remaining stereo artifacts.
