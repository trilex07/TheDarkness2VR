"""Read-only, exact-build receipt for the native Cinematic control flags."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile

SHA = 'd46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1'

def audit(path):
    blob = Path(path).read_bytes()
    if hashlib.sha256(blob).hexdigest() != SHA:
        raise ValueError('Unsupported executable; no layout inference')
    pe = pefile.PE(data=blob)
    base = pe.OPTIONAL_HEADER.ImageBase
    def data(address, count):
        value = pe.get_data(address-base, count)
        if len(value) != count:
            raise ValueError('Truncated native range')
        return value
    def word(address):
        return struct.unpack('<I', data(address, 4))[0]
    def string(address):
        value = data(address, 256)
        end = value.find(b'\0')
        if end < 0:
            raise ValueError('Unterminated descriptor')
        return value[:end].decode('ascii')
    if word(0xFCA810) != 0x47FC00 or word(0xFCAA18) != 0xB7E570:
        raise ValueError('Cinematic serializer/state getter mismatch')
    serializer = data(0x480140, 0x63)
    if hashlib.sha256(serializer).hexdigest() != '6bc9c063780409106d45797853b8c49f0d3333b75178f2c758777b610a82437c':
        raise ValueError('Three-field serializer changed')
    getter = data(0xB7E570,13)
    if hashlib.sha256(getter).hexdigest() != '44efa516ae91b16dc7382ca5cad24c8864750aa44f05a788b83a8f79dd78cc48':
        raise ValueError('IsPlaying changed')
    properties=[]
    for offset, expected_name, expected_field in [(0,'mCanDamagePlayer',0x7BC),(0x21,'mFreeCam',0x7BD),(0x42,'mOnRails',0x7BE)]:
        descriptor = struct.unpack_from('<I',serializer,offset+9)[0]
        field = struct.unpack_from('<I',serializer,offset+24)[0]
        name = string(word(descriptor+4))
        if name != expected_name or field != expected_field:
            raise ValueError('Property name/offset mismatch')
        properties.append(dict(name=name,offset=hex(field),descriptor=hex(descriptor),description=string(word(descriptor+8))))
    return dict(status='PASS',exe_sha256=SHA,cinematic_table='0xfca7e0',playing_state_offset='0x708',playing_state=2,properties=properties)

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe',type=Path)
    args=parser.parse_args()
    print(json.dumps(audit(args.exe),indent=2))
