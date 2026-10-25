"""Read-only receipt for the actual swapchain path and rejected vptr experiment.

No process access, hooks, module loading, or patches. A PASS authenticates the
static evidence ONLY; it never authorizes a public-length COM vtable clone.
"""
import argparse
import hashlib
import json
from pathlib import Path
from audit_camera import PE, audit

ENGINE_RANGES = {
    "additional_swapchain_creation": (0x964C40, 311, "da849181409882729c5c8ba4927243c2e069563b7bcc741d02c8d05941cb637c"),
    "presentation_selection": (0x920EE0, 163, "21b33d7fc2c9474e9292b006cd74da891347ff6cf1898c25cd4dbd0a27a18d5d"),
    "swapchain_present_dispatch": (0xA94020, 112, "da879fa2c3a0a8e29178324bf8fa596d872efd3ccb4b199d989fe37fcb240860"),
    "release_and_recreate": (0x8B63C0, 96, "4721dc9a617139f43fd3282bbbc5cb4d7b30d6084a2a4035cfc366aabafb66ee"),
    "owner_constructor": (0xB6A670, 68, "39f427c0330a4d93b9b066943a41de1930e1e523c36d155a365b2868e0223a04"),
}
D3D9_SHA = "fd72e021c128a98a5d688eb9acc07e3eb1948ca6b630b9fa5f328898e78c7dbc"
D3D9_BASE, D3D9_TIMESTAMP, D3D9_SIZE = 0x10000000, 0x3F41597F, 0x179000
PRIVATE_CALL_RVA = 0x488AA
# mov eax,[edi]; push edi; mov esi,[eax+270h]; mov ecx,esi;
# call CFG dispatch check; call esi. Return at module+488BD.
PRIVATE_CALL_BYTES = bytes.fromhex("8b07578bb0700200008bceff1590471610ffd6")


def engine_evidence(blob):
    identity = audit(blob)
    pe = PE(blob)
    ranges = []
    for label, (va, size, expected) in ENGINE_RANGES.items():
        try:
            digest = hashlib.sha256(pe.at_va(va, size)).hexdigest()
        except ValueError:
            digest = None
        ranges.append(dict(label=label, va=f"{va:08X}", length=size,
                           sha256=digest, match=digest == expected))
    return dict(identity=identity, ranges=ranges,
                verified=identity["known_identity"] and identity["all_sites_match"]
                and identity["all_function_ranges_match"] and all(r["match"] for r in ranges))


def runtime_evidence(blob):
    pe = PE(blob)
    digest = hashlib.sha256(blob).hexdigest()
    identity = (digest == D3D9_SHA and pe.machine == 0x14C and pe.base == D3D9_BASE
                and pe.timestamp == D3D9_TIMESTAMP and pe.image_size == D3D9_SIZE)
    try:
        observed = pe.at_va(pe.base + PRIVATE_CALL_RVA, len(PRIVATE_CALL_BYTES))
    except ValueError:
        observed = b""
    verified = identity and observed == PRIVATE_CALL_BYTES
    return dict(sha256=digest, known_identity=identity, instruction_match=observed == PRIVATE_CALL_BYTES,
                verified=verified, private_slot=156 if verified else None,
                public_device9ex_slot_count=134, complete_native_vtable_extent=None,
                public_length_shadow_safe=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--d3d9", type=Path, required=True,
                        help="x86 system D3D9 file; on x64 Windows use SysWOW64/d3d9.dll")
    args = parser.parse_args()
    try:
        engine = engine_evidence(args.exe.read_bytes())
        runtime = runtime_evidence(args.d3d9.read_bytes())
    except (OSError, ValueError) as error:
        print(json.dumps(dict(status="BLOCKED", error=str(error))))
        return 2
    ok = engine["verified"] and runtime["verified"]
    print(json.dumps(dict(status="STATIC_EVIDENCE_PASS" if ok else "BLOCKED",
                          engine=engine, d3d9=runtime, live_present_observed=False,
                          install_observer_authorized=False), indent=2))
    return 0 if ok else 3


if __name__ == "__main__":
    raise SystemExit(main())
