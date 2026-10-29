# Remaining right-eye PS inheritance defect

## Installed provenance

Read back from the Steam installation on 2026-10-29:

- DLL SHA-256 `C25CDA1ED2CF9B01AC8B9CA21F869526AAE13783C0078641586E418FA3F34D1F`.
- Source `caa0bb0fba3f84d755207de5c524a265e49fd0c7`, with diagnostic DirectRecordCopy=1.
- `git merge-base --is-ancestor 4e180e9 caa0bb0` succeeds: this is already the
  integrated WVP/STP eyeCarry build, not the original standalone CPU branch.
- Subsequently installed wrist asset correction is `f2b8c2e`; it did not replace
  this DLL or its INIs. The source branch for this fix inherits that correction.

## Reproduced failure

Frame N-1 uploads a compensated inverse projection/view-extents PS packet.
Live-end restoration and the next frame-start D3DSBT_ALL restore the left eye's
materialized pixel constants. Frame N draws with no replacement PS upload.
VS eyeCarry repairs only vertex registers; the right eye therefore consumes the
left eye's PS values. The same problem applies to replay-adjusted light positions.

The extended existing actual-replay-loop harness fails on **caa0bb0's source**:
`Bank mismatch: inheritedVariant=100 mode=0 draw=0`. This is the first inherited
PS draw, with constant grouping disabled. No GPU or headset is involved in that
reproduction. It proves a concrete replay defect, not that this is the sole cause
of the user's intermittent artifacts.

## Minimal correction

Keep two PS materialized register banks, under the existing recorder lock.
Remember only uploads that receive eye compensation. Before replay commands,
restore valid carried PS rows in contiguous runs bounded by device PS caps.
These are the previous right-eye bytes; do not transform them again using the
next HMD pose. Fresh native uploads still use the current eye parameters.

Ordinary PS uploads invalidate overlapping carried rows. Successful unrecorded
live uploads invalidate their ranges; failed live setters do not change carry.
Successful unrecorded StateBlock Apply, recorded replay Apply, skipped prior
replay, rejected replay, Reset and failed live-end restoration invalidate the
corresponding history. A rejected carried API call keeps the stereo pair invalid.

DirectRecordCopy, VS transformations, recording order, GPU resource lifetimes,
hands, weapons, tentacles, input and OpenXR submission remain unchanged.
No new fence, resource readback, sleep, arbitrary clear or profiler is introduced.

## Offline evidence

- Actual float replay, production and diagnostic: 96 streams / 576096 records /
  58510 checks each. Eight PS inheritance/invalidation/failure variants across
  all four grouping modes; previous VS inheritance cases retained.
- Actual inverse-projection/view-extents mathematics: 64 changing pose/FOV cases
  with and without head geometry, inherited bytes, fresh replacement and Reset.
  Existing projection-history suite totals 145248 checks.
- Actual live StateBlock/shader synchronization: 296163 checks, including PS
  carry invalidation and failed Apply preservation.
- Actual native PS setter/producer ABI: 9359 checks, including successful
  unrecorded invalidation, failed-setter preservation and original HRESULT.
- Actual PS packet case: 576 cases in each build flavor; unchanged payloads,
  light/projection compensation, invalid ranges and 224-vector bound.
- DirectRecordCopy/actual RecPush: 10681 checks; implementation unchanged.
- Actual replay surface receipts: 128 streams / 896 calls.
- Both production and diagnostic x86 DLL builds pass.
- Pixel metadata publication/binding, right-eye color-history simulation,
  independent left aim (461 checks), tentacle head scope (26629), native hands
  geometry (ten seeds, 50240 cases each) and both XInput bridge flavors pass.

The test scaffolds were extended for the new PS bank. Their initial missing-bank
compile errors were fixture omissions, not new runtime failures. New policy tests
against the old source intentionally fail as described above.

## Performance and remaining scope

Fixed storage: two 4 KiB value banks plus validity masks. Two bounded bank copies
per replay and contiguous uploads of carried compensated PS rows; ordinary-only
streams have no extra PS uploads. Recorded native PS uploads acquire no additional
lock. Runtime CPU/GPU/FPS impact has not been measured.

Dynamic VB/IB/texture content versions and arbitrary recorded StateBlocks that
embed left-adjusted constants remain separate correctness gaps. Clearing history
does not convert such a block into right-eye state. The existing right-eye color
history/copy rules are not changed. No new capture evidence establishes those
other gaps as the user's present cause.

Keep the accepted Steam CPU baseline and immediate DLL/INI rollback. Validate this
candidate in one consolidated authorized PSVR2 session: menu/pause flicker,
stationary and slow head motion, peripheral geometry, shadows, busy-scene frame
pacing, both hands/aim and tentacle interactions. HMD correctness and FPS impact
remain unverified until that session.
