# ConsumedRecordCopy MAIN integration (2026-10-30)

Source f9185c8e0ede91dd79f94e06667129052e02a289 was integrated as 4a26aabdce15903eb5efe48bd707d47e200175e5 on top of accepted 020d6799 and the preserved PipelineWarmup runtime result. No integration conflicts or unrelated renderer changes. Shipped defaults remain OFF; the authorized private Steam test profile selects Render/ConsumedRecordCopy=1. DirectRecordCopy=0 and ReplayBufferVersions=0 remain quarantined.

## Reviewed contract

Six VS/PS F/I/B setters declare their input only for nonzero in-range uploads (VS F 256, PS F 224, I/B 16). Subtraction-based bounds avoid start+count overflow. Out-of-range/zero ranges and undeclared transform/UP/other payloads retain the prior path. The declarations occur after StateBlock/concurrency early exits and before the native forward; no new native calls are made under the recorder lock. VS/PS F record the original bytes before left-eye substitution. Existing raw register history, partial WVP/STP handling and live HRESULT rejection remain intact. No borrowed payload survives the call: replay owns its bytes in the recorder arena.

The optimization assumes a normal valid native setter consumes the entire admitted payload synchronously, or the stereo substitution copies it before forwarding. Register validity alone is not proof that an arbitrary failing/lost-device runtime consumes a pointer. Incomplete copies reject the replay stream via existing overflow handling. This is an opt-in candidate, not a general memory-ownership proof.

The implementation reuses DirectCopy's SEH/PAGE_GUARD re-arm helper. Own-page guard/noaccess and cross-page prefix/zero-tail regressions PASS; concurrent protection changes, guard restoration races and arbitrary application exception handlers remain unproven. Keeping DirectRecordCopy OFF does not remove this residual risk from ConsumedRecordCopy's narrower path. Preserve the exact working DLL/INIs for immediate restoration on a crash, hang or corrupted constants. Do not describe offline tests as runtime safety acceptance.

## Offline verification

Both ordinary and DiagnosticCapture x86 builds PASS. Eleven actual-source runners PASS: small-record-copy (10,729 byte/range checks), native_constant_source, live_record_status, up_geometry_capture, projection_history, stateblock_recording, live_stateblock_sync, replay_buffer_versions (including reentrant callback/Reset), shader_register_bank, replay_binding_cache and replay_surface_status. No new failures in these runners.

The actual-recorder fixture observed four eligible payloads changing from four RPM calls to zero, while undeclared/out-of-range payloads retained RPM and byte-identical replay ownership. The synthetic 1,697-payload loop measured 1.036 ms with RPM and 0.061 ms consumed copy on this run; its 0.975 ms difference is not a runtime gain or guaranteed lower bound. No measured FPS, CPU improvement or headset visual acceptance exists for this candidate yet.

## Runtime gate and next work

One ON-only outdoor gameplay test, against retained measurements without another baseline launch. Retain DLL/PDB/MAP and effective INI hashes; use existing performance/command evidence only when needed. Account for the previous collector's user-reported approximately 20 FPS penalty; do not invent corrected FPS or a precise causal saving from incomparable runs. Verify both eyes, peripheral geometry, firing, independent hands and tentacles together. Current warmup compiler dependency remains unresolved; do not hide it behind average CPU gains.

Private package: LocalAppData/Darkness2VR/packages/consumed-record-copy-20261030. Frozen artifacts, regression logs, deployment receipt and SHA-verified rollback are local. Further sustained-CPU work must use retained evidence and preserve this ON-only validation boundary.
