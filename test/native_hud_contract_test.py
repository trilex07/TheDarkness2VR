"""Offline production pin check against the user's stock EXE (no game launch)."""
import hashlib
import re
import sys
from pathlib import Path
import pefile

exe = Path(sys.argv[1])
data = exe.read_bytes()
assert hashlib.sha256(data).hexdigest() == "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
pe = pefile.PE(data=data)
source = (Path(__file__).resolve().parents[1] / "src/native_hud.inl").read_text()
arrays = {}
for name, values in re.findall(r"static const unsigned char (\w+)\[\]=\{([^}]+)\}", source):
    arrays[name] = bytes(int(value.strip(), 0) for value in values.split(","))
pins = re.findall(r"\{(0x[0-9a-f]+),(\w+),sizeof\(\2\)\}", source)
assert len(pins) == 6
for address, name in pins:
    expected = arrays[name]
    observed = pe.get_data(int(address, 16) - pe.OPTIONAL_HEADER.ImageBase, len(expected))
    assert observed == expected, f"{name} at {address}: {observed.hex()} != {expected.hex()}"
print(f"Native HUD contract: SHA256 + {len(pins)} production pin ranges PASS")
