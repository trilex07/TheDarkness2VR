# Consumed recorder copy MAIN test

MAIN validation uses ONE ON-only outdoor combat launch compared with retained historical measurements. Do not require a redundant OFF launch. Source defaults remain OFF; apply on.overrides.ini to the accepted protected profile for the authorized test.

Require startup receipt [ConsumedRecordCopy] enabled=1. DirectRecordCopy and ReplayBufferVersions remain OFF. Check stereo constants, geometry, independent hands, tentacles, CPU frame time and stability together. Keep exact DLL/PDB/MAP and effective INIs.

The narrower consumed path still reuses DirectCopy's PAGE_GUARD exception/re-arm helper. Own-page fault regressions do not prove concurrent protection restoration safety. Roll back the complete verified DLL/INI package on crash, hang or corruption; restart with ConsumedRecordCopy=0.

Offline call-count and loop timings are not gameplay FPS evidence. See docs/investigations/consumed-record-copy-main-20261030.md.
