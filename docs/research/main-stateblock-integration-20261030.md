# MAIN StateBlock integration

Integrated 83afd6ef onto 69b02db1 as 7dbd6d5c, without production conflicts. Existing recorder/driver deadlock recovery, successful texture/depth binding publication, CPU filters, FOV and all VR paths remain intact. No automatic merge.

Reviewed exact-device construction guard, successful/failed Begin/End transitions and Reset/ResetEx cleanup. Capturable setters forward raw native arguments while constructing a StateBlock, without publishing phantom live state, shader metadata or constants. Apply/Capture synchronization remains separate. No added GPU waits.

Both standard and protected-native x86 builds PASS. Actual-source regressions PASS: construction Begin/End (12004), texture/depth (1679), live setters (5880), native constant source, pixel shader metadata, live StateBlock synchronization, replay state/surface receipts, binding cache, VB/IB consistency and eleven bounded reentrant callback probes, frame resource ownership, UP geometry, projection and eye-color history, tracked hands, left aim, tentacles and native menu input snapshot.

Integration exposed one fixture dependency: replay_buffer_versions_test.cpp extracts the newly guarded stream/index setters and needed stateblock_recording.h. Initial compile failed with missing namespace; adding the real production header restored PASS. No production test failure was waived.

Candidate protected-native DLL SHA256: D05C61B763E4B68C92923E3BE2A94517A2E8C73320EF2BAE548B6CC66439D644.

Retain deployed INIs, ReplayBufferVersions=0, base font package ca3e75ae and official DLC font overlay bd3ea08d. Copy only the verified DLL after backup and process/profile/hash checks. Runtime flicker, font glyphs and CPU/FPS are pending one manual consolidated PSVR2 session; offline results do not establish visual acceptance or runtime savings.
