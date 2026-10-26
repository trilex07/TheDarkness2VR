# Protected graphics tracking candidate

Base 85eea69; old accepted rendering package is preserved. No Steam launch or
installation. This is an IMPLEMENTED/OFFLINE VERIFIED geometry correction,
not native camera 6DoF or HMD acceptance.

The old VS path applied eye translation before the head delta. At roll it
rotated baseline disparity vertically. The corrected order is native view,
head delta, eye baseline, projection. Deferred inverse reconstruction now
derives the corresponding eye offset after transforming its basis.
Tracking translation now uses GameUnitsPerMeter, matching XR-driven IPD,
and subtracts the reference origin before multiplication.

The OFF-by-default ProtectedHeadTracking opt-in enables the existing geometric
render path only when both geometry flags, translation stereo, projection
compensation and head-center submission agree. Mouse/stick head look remains
OFF, so the pose is not applied twice. Both eyes retain one frozen frame pose.
Invalid tracking closes layer admission as before. The original protected
render-only profile remains unchanged. The test compiles the actual PS
compensation function and exercises complete roll sweeps, combined rotations,
translation, metric scales, inverse reconstruction and recenter.

Concrete remaining gate: CPU visibility was built for the native camera
before graphics interception. The old selected-camera callback at 00986139
is forbidden because it patches EXE instructions. Shader transforms cannot
restore objects rejected before submission, update native collision, or prove
wide peripheral coverage. Do not enable this profile as full native tracking
or claim the culling requirement passed. A safe pre-visibility selected-camera
access point with verified owner lifetime is still required.

Offline results: 116,988 tracking/reconstruction checks PASS; existing frame
suite 2,327,551 assertions PASS; baseline profile 2 tests PASS; production and
diagnostic x86 builds PASS. Native visibility/Steam/PSVR2: NOT RUN.
