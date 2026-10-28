# Shader metadata publication and binding lifetime

Parent: `ad6ca8a` on `feature/render-command-worker`. No Steam deployment.

## Reproduced defect

The actual extracted `Hook_CreatePixelShader`, before this change, reproduced:

1. A pixel shader at an address declares InverseProjection c27 and ViewExtents c38.
2. Another successfully created shader reuses the address.
3. Its GetFunction fails. The hash changes, but the old projection registers survive.

The x86 fixture printed `REPRODUCED actual CreatePixelShader hook: recycled COM
address retains prior InverseProjection/ViewExtents after new shader reflection
fails`. This proves a source defect, not that a real driver failure occurred in
the reported HMD flicker.

Separately, creation wrote three pixel metadata maps with different locking;
live binding, snapshot restoration and diagnostic readers could access them
without synchronizing with insertion/rehash. Vertex snapshot restoration also
read its map without the recorder lock used by creation. Vertex replacement
modified the metadata node that an active binding could still reference.

## Correction

- Publish pixel hash/inverse/extents together after reflection. Each successful
  creation starts with unknown registers; failed/invalid reflection cannot inherit
  the preceding object's registers. Returned bytecode length and token boundaries
  are validated before reflection, within the owned buffer.
- Read one coherent pixel snapshot at live binding, replay binding and snapshot
  restoration. Readback, matched captures and frame dumps use the same registry.
  The right-eye light-position upload now uses the restored active shader hash,
  including inherited bindings before the first recorded SetPixelShader. The
  previous replay-pointer hash could instead describe the preceding replay's
  last shader, since snapshot restoration did not update that pointer.
- Publish vertex analysis as a new owned node under a separate registry lock.
  Active bindings retain their node and ordered projection history across map
  rehash/replacement. Full bytecode equality preserves a deduplicated shader's
  history; pointer/hash equality alone does not.
- Creation analysis no longer acquires the recorder's whole-replay lock. Registry
  locks cover only metadata access, never driver calls/reflection/replay/logging.
  Repeated bindings to the same vertex node avoid atomic reference-count changes.
- Make vertex creation dump/log counters atomic, so multiple creation threads do
  not race on diagnostic counters or use the same reserved dump index.

Native/preload creation paths and shader rewrites are unchanged. No new shader
object cache, asynchronous creation, extra D3D calls, EXE write or profile change.
Render bindings and projection histories remain owned by the existing render
thread; this does not make arbitrary concurrent device-state mutation supported.

## Offline validation

Final consolidated x86 run:

- `test/build_pixel_shader_metadata.ps1`: actual creation/bind hook; address reuse,
  reflection failures, invalid sizes/token boundaries, original HRESULT/call count,
  null binding, actual snapshot-sync body (fixture device) and inherited-light
  upload branch. 100,000 concurrent publications with two readers and 16,383 extra
  identities forcing rehash; coherent snapshots only.
- `test/build_projection_history.ps1`: 24,486 checks; existing actual-source
  live/replay projection regression plus 20,000 concurrent vertex replacements,
  retained binding/history, exact identity, rehash, retired-node reclamation and
  creation-analysis independence from an occupied recorder lock.
- `test/build_tentacle_head_scope.ps1`: 19,062 checks / 1,890 scopes PASS.
- `test/build_replay_surface_status.ps1`: 96 streams / 576 calls PASS.
- Production and diagnostic x86 DLL builds PASS.

No real GPU/game/HMD run, FPS improvement or observed flicker resolution is
claimed. Additional metadata synchronization has not been timed in game.
The user's CULL OFF report remains a separate unresolved visibility-admission
problem; this change does not bypass its checks or claim to fix side disappearance.

## Integration

MAIN can cherry-pick this commit after `ad6ca8a`, or build the branch containing
the selected-player (`b17208d`) and projection-history (`ad6ca8a`) corrections.
Use the existing native 2676x2728-per-eye profile. Only MAIN deploys to Steam,
after backing up the installed DLL and confirming the actual deployed SHA.
Consolidate validation of image correctness, CULL status, right-eye artifacts,
controller ownership and frame times in one later headset run.
# Rejected binding correction

The actual-hook fixture reproduced a rejected null pixel-shader bind clearing
the active metadata and recording an unsuccessful command. Live VS/PS hooks
now forward once and commit metadata/recording only on success (including
S_FALSE). Replay VS/PS cases likewise retain prior metadata on failure and
mark the eye incomplete. Actual extracted live and replay cases cover failed
null/non-null binds and successful transitions. The 100,000-publication and
24,486 projection-history regressions and both x86 builds pass. This repairs
a proven error path; its contribution to observed HMD flicker is unmeasured.

