# Accepted baseline

Accepted by the user on 2026-10-30 as the best candidate tested so far.
Source: 5ef2106c64a28e0c1c7f1c40bd45ea2394d932fa
Tag: codex/accepted-baseline-20261030-5ef2106c
Protected-native x86 DLL SHA256:
C1022E58F987191E91E3EB200951BE355A71FC7C74A586308A608A06B8BC82E7

Private recovery package:
%LOCALAPPDATA%/Darkness2VR/baselines/accepted-20261030-5ef2106c
Independent copies of 25 files: current DLL, four INIs and twenty localization
archive files. All copied and installed SHA-256 values matched after snapshot.
manifest.json records destinations, sizes and hashes; SHA256SUMS.txt supports
independent verification. restore.py verifies all backups and the native EXE,
refuses a running game, saves current targets, then stages replacements.
No baseline restoration was run during acceptance.

Preserve this baseline while developing the next candidate. Keep StateBlock
construction exclusion, texture/depth replay correction, recorder deadlock
recovery, accepted constant/binding optimizations, native FOV coverage and
working VR/input/haptics. ReplayBufferVersions remains 0. NativeOwnedConstantCopy
and NativeOwnedMaterialCopy are enabled; DirectRecordCopy remains disabled.
Base Polish fonts plus the official DLC font override repair are retained.

Acceptance means best user-tested candidate, not proof of sustained 90 FPS,
all visual defects resolved, complete native-menu laser or implemented runtime
half-rate control. Quantified FPS/CPU and remaining visual acceptance require
the user's consolidated headset feedback. SteamVR global settings were not
changed or captured by this recovery package. Native EXE is unchanged and not
included. Do not commit private game archives, binaries or recovery data.
