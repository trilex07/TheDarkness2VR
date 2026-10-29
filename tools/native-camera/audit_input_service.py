"""Read-only receipt for the pinned native client/input initialization ABI.

No process access, factories, hooks or input injection. Static field provenance
does not establish a live framework address, menu state or rendered menu plane.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from audit_camera import PE

SHA256 = "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"


def inspect(blob):
    digest = hashlib.sha256(blob).hexdigest()
    pe = PE(blob)
    if digest != SHA256 or pe.machine != 0x14C or pe.base != 0x400000:
        raise ValueError("unrecognized native image; no service contract admitted")
    # The static initializer selects this exact global and constructor. The
    # constructor writes FBAC40; slot +1C is a no-stack-arguments field getter.
    initializer = pe.at_va(0xEA4FD0, 10)
    if initializer[:6] != bytes.fromhex("b9f8e30d01e8"):
        raise ValueError("service-global initializer changed")
    target = 0xEA4FDA + struct.unpack("<i", initializer[6:])[0]
    if target != 0xC5CC40:
        raise ValueError("service constructor changed")
    if pe.at_va(target, 9) != bytes.fromhex("8bc157c70040acfb00"):
        raise ValueError("service vtable assignment changed")
    getter = struct.unpack("<I", pe.at_va(0xFBAC40 + 0x1C, 4))[0]
    if getter != 0x74B5F0 or pe.at_va(getter, 4) != bytes.fromhex("8d4150c3"):
        raise ValueError("service getter is not the verified no-pop getter")
    # Initializer C2C1A0 reserves 2C and saves four registers: entry ESP-3C.
    # Its reads at +48/+4C/+50 therefore name arguments 3/4/5, respectively.
    # Seven pushed words survive the getter; RET 1C consumes all seven.
    if pe.at_va(0xC2C1A0, 7) != bytes.fromhex("83ec2c53555657"):
        raise ValueError("client initializer frame changed")
    if pe.at_va(0xC2CC18, 3) != bytes.fromhex("c21c00"):
        raise ValueError("client initializer argument cleanup changed")
    return {
        "status": "STATIC_CONTRACT_PASS", "exe_sha256": digest,
        "framework_client_reference_offset": "0xA8",
        "reference_assignment": "0x7A8570; pointer to intrusive reference, not object",
        "client_initializer": "0xC2C1A0; this=client, seven stack arguments",
        "arguments": ["framework", "service-global+0x50", "graphics reference address",
                      "sound reference address", "input reference address",
                      "framework+0x10 reference value", "0x1102080"],
        "client_reference_fields": {"graphics": "0x2D0", "sound": "0x2C4", "input": "0x374"},
        "input_object_path_from_valid_framework": [
            "client_ref = load_ptr(framework+0xA8)", "client = load_ptr(client_ref)",
            "input_ref = load_ptr(client+0x374)", "input_object = load_ptr(input_ref)"],
        "live_framework_address_verified": False, "live_input_type_verified": False,
        "menu_visibility_verified": False, "menu_surface_verified": False,
        "input_injection_admitted": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    args = parser.parse_args()
    try:
        result = inspect(args.exe.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        print(json.dumps({"status": "BLOCKED", "error": str(error)}))
        return 2
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
