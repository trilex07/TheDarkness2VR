"""Actual pack-builder regressions; synthetic fixtures contain no game data."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"tools"))
from polish_pack import Cache, audio_format, chunks, patch_xwma, translate
from polish_fonts import patch_font, LETTERS


def native_audio(resident=False):
    h = bytearray(220)
    p, count, align, samples = 32, 2, 1487, 2048
    struct.pack_into("<I", h, p, 44100)
    h[p+4:p+6] = bytes((16, 1))
    struct.pack_into("<III", h, p+6, samples, 4000, align)
    struct.pack_into("<II", h, p+26, count, count*align)
    h[p+37] = 0 if resident else 2
    b = b"annotation"+struct.pack("<II", 2048, 4096)
    if resident:
        b += b"x"*(count*align)
    struct.pack_into("<III", h, p+55, len(b), len(b), 10 if not resident else 123456)
    struct.pack_into("<II", h, p+67, count*align, count*align)
    return bytes(h), b, count*align


def riff(dpds=(2048, 4096, 6144)):
    parts = [(b"fmt ", struct.pack("<HHIIHHH", 0x161, 1, 44100, 4000, 1487, 16, 0)),
             (b"dpds", struct.pack("<"+str(len(dpds))+"I", *dpds)),
             (b"data", b"p"*(1487*len(dpds)))]
    payload = b"XWMA"+b"".join(tag+struct.pack("<I",len(b))+b+(b"\0" if len(b)&1 else b"") for tag,b in parts)
    return b"RIFF"+struct.pack("<I",len(payload))+payload


class PolishPackTests(unittest.TestCase):
    def test_real_xwma_updates_only_serialized_contract(self):
        h,b,n = native_audio()
        hh,bb,ff = patch_xwma(h,b,n,riff())
        self.assertEqual(bb[:10],b[:10])
        self.assertEqual(len(ff),1487*3)
        self.assertEqual(audio_format(hh,bb,len(ff))[6],3)
        allowed = {i for r in (6,26,30,55,59,67,71) for i in range(32+r,36+r)}
        self.assertTrue(all(a==b or i in allowed for i,(a,b) in enumerate(zip(h,hh))))

    def test_zero_padding_is_not_audio_size(self):
        h,b,n = native_audio()
        self.assertEqual(audio_format(h,b,n)[-1],10)
        self.assertRaises(ValueError,audio_format,h,b,0)

    def test_asf_packet_rejected(self):
        self.assertRaises(ValueError,chunks,bytes.fromhex("3026b2758e66cf11")+bytes(200))

    def test_truncated_riff_rejected(self):
        self.assertRaises(ValueError,chunks,riff()[:-1])

    def test_bad_packet_table_rejected(self):
        h,b,n = native_audio()
        self.assertRaises(ValueError,patch_xwma,h,b,n,riff((4096,2048,6144)))

    def test_resident_validation_does_not_use_streaming_offset(self):
        h,b,n = native_audio(True)
        self.assertEqual(audio_format(h,b,n,True)[6],2)
        self.assertRaises(ValueError,audio_format,h,b,n)

    def test_duplicate_format_candidate_rejected(self):
        h,b,n = native_audio()
        self.assertRaises(ValueError,audio_format,h+h,b,n)

    def test_utf8_translation_keeps_tokens_and_indices(self):
        marker=b"/D2/Language/Menu/"
        text="<GO>Load %s".encode()
        entry=struct.pack("<I",3)+b"key"+struct.pack("<IIBH",42,0,0,len(text))
        source=b"prefix"+marker+struct.pack("<III",1,1,len(text)+1)+text+b"\0"+entry+b"tail"
        result,report=translate(source,marker,{"key":"<GO>Załaduj %s"})
        self.assertIn("Załaduj".encode(),result)
        self.assertEqual(report["replaced"],1)
        self.assertTrue(result.endswith(b"tail"))
        retained,report=translate(source,marker,{"key":"Załaduj"})
        self.assertEqual(retained,source)
        self.assertEqual(report["retained_for_tokens"],["key"])

    def test_font_aliases_preserve_all_existing_uv_and_other_languages(self):
        codes=sorted(set(map(ord,"OoAaCcEeLlNnSsZz ")))
        n=len(codes);h=bytearray(180)
        for off in (16,48):struct.pack_into("<7I",h,off,n,0,0,0,0,0,0)
        uv=[struct.pack("<4f",i/100,0,(i+1)/100,1) for i in range(n)]
        metrics=[struct.pack("<7f",*([float(i)]*7)) for i in range(n)]
        body=b"".join(uv)+b"".join(metrics)+struct.pack("<"+str(n)+"H",*codes)+b"auxiliary"
        hh,bb=patch_font(bytes(h),body,16,n)
        new=struct.unpack_from("<"+str(n+18)+"H",bb,(n+18)*44)
        self.assertEqual(len(new),len(set(new)))
        for letter,donor in LETTERS:
            i,j=new.index(ord(letter)),new.index(ord(donor))
            self.assertEqual(bb[i*16:(i+1)*16],bb[j*16:(j+1)*16])
        self.assertEqual(bb[(n+18)*46:],body[n*46:])

    def test_cache_stale_entries_and_unrelated_payload_preserved(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);out=root/"out"
            def entry(off,stamp,name):
                return struct.pack("<QQIIII",off,stamp,3,3,0,0)+name.ljust(64,b"\0")
            toc=bytes.fromhex("4ec6671800000000")+entry(0,1,b"a")+entry(3,0,b"a")+entry(6,1,b"other")
            (root/"T.toc").write_bytes(toc);(root/"T.cache").write_bytes(b"oldSTAkeep")
            cache=Cache(root,"T");cache.write(out,{"a":b"translated"})
            result=(out/"T.toc").read_bytes()
            self.assertEqual(result[104:],toc[104:])
            self.assertTrue((out/"T.cache").read_bytes().startswith(b"oldSTAkeep"))

    def test_planar_dxt3_roundtrip(self):
        from polish_fonts import decode_planar_dxt3, encode_planar_dxt3
        raw = bytes(16384)
        alpha = decode_planar_dxt3(raw, 512, 32)
        self.assertEqual(alpha.shape, (32, 512))
        alpha[10:20, 20:30] = 255
        encoded = encode_planar_dxt3(alpha, 512, 32, raw)
        self.assertEqual(len(encoded), 16384)
        decoded = decode_planar_dxt3(encoded, 512, 32)
        self.assertTrue((decoded[10:20, 20:30] == 255).all())

    def test_visual_diacritics_drawn_with_distinct_uvs(self):
        import numpy as np
        from polish_fonts import encode_planar_dxt3, decode_planar_dxt3
        codes = sorted(set(map(ord, "OoAaCcEeLlNnSsZz ")))
        n = len(codes)
        h = bytearray(180)
        for off in (16, 48):
            struct.pack_into("<7I", h, off, n, 0, 0, 0, 0, 0, 0)
        uv = [struct.pack("<4f", i*24/1024, 0, (i*24+16)/1024, 24/64) for i in range(n)]
        metrics = [struct.pack("<7f", *([float(i)] * 7)) for i in range(n)]
        body = b"".join(uv) + b"".join(metrics) + struct.pack("<" + str(n) + "H", *codes) + b"auxiliary"
        alpha=np.zeros((64,1024),dtype=np.uint8)
        for i in range(n):alpha[6:19,i*24+4:i*24+12]=255
        raw_tex=encode_planar_dxt3(alpha,1024,64,bytes(65536))
        hh, bb, tt = patch_font(bytes(h), body, 16, n, raw_tex, (1024,64,0,0,0))
        self.assertEqual(len(tt), len(raw_tex))
        new = struct.unpack_from("<" + str(n + 18) + "H", bb, (n + 18) * 44)
        self.assertEqual(len(new), n + 18)
        for letter, donor in LETTERS:
            i, j = new.index(ord(letter)), new.index(ord(donor))
            pl_u0, pl_v0, pl_u1, pl_v1 = struct.unpack_from("<4f", bb, i * 16)
            self.assertNotEqual(bb[i*16:(i+1)*16],bb[j*16:(j+1)*16])
            crop=decode_planar_dxt3(tt,1024,64)[round(pl_v0*64):round(pl_v1*64),round(pl_u0*1024):round(pl_u1*1024)]
            self.assertGreater(np.count_nonzero(crop),np.count_nonzero(alpha[:24,j*24:j*24+16]))
        rebuilt=decode_planar_dxt3(tt,1024,64)
        self.assertTrue(np.array_equal(rebuilt[alpha>0],alpha[alpha>0]))

    def test_native_alpha_dword_planes_are_not_interleaved(self):
        from polish_fonts import decode_planar_dxt3
        raw=bytearray(16384)
        struct.pack_into('<I',raw,4,0x0000000f)
        struct.pack_into('<I',raw,4096+4,0x000000f0)
        alpha=decode_planar_dxt3(raw,512,32)
        self.assertEqual(int(alpha[0,4]),255)
        self.assertEqual(int(alpha[2,5]),255)
        self.assertEqual(int(alpha[0,8]),0)


if __name__ == "__main__":
    unittest.main()
