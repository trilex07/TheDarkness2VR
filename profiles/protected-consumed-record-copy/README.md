# Consumed recorder copy A/B

Same DLL, scene, resolution and INIs in both arms; only `Render/ConsumedRecordCopy`
0/1 differs. Restart between arms.

Require the startup receipt `[ConsumedRecordCopy] enabled=1` in the ON arm. Compare
render-thread CPU busy and frame time in the same outdoor combat route. In an ETW
capture, `RecPushLocked -> ReadProcessMemory` should drop to the few undeclared
payloads (transform, viewport, rects, out-of-range uploads). Right-eye constants,
hands, tentacles and FOV must be unchanged.

Rollback: select the OFF overlay and restart.
