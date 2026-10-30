"""Native raw DXT3 regressions; independent BC2 decoder is the upload oracle."""
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from polish_fonts import FONTS, require_font_inventory, planar_to_native_dxt3, decode_native_dxt3

class NativeFontTests(unittest.TestCase):
    def test_inventory_rejects_omitted_corbel_and_arial(self):
        entries = {font[1]: (0, 0, 1024, 1024) for font in FONTS}
        entries['Temp/JapaneseGlyph_en_f.tga'] = (0, 0, 16, 16)
        require_font_inventory(SimpleNamespace(entries=entries))
        for family in ('Corbel_26', 'Arial_20'):
            incomplete = {k: v for k, v in entries.items() if family not in k}
            with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
                require_font_inventory(SimpleNamespace(entries=incomplete))

    def test_known_bc2_block_color_and_alpha(self):
        block=bytes.fromhex('1032547698badcfe00f8000000000000')
        rgba=decode_native_dxt3(block,4,4)
        self.assertEqual(rgba[:,:,3].flatten().tolist(),[i*17 for i in range(16)])
        self.assertTrue(np.all(rgba[:,:,:3]==[255,0,0]))

    def test_four_planes_scatter_to_native_stride_across_chunks(self):
        native=bytes((i*71+i//251)%256 for i in range(32768))
        # Independent scalar fixture, matching native DWORD scatter contract.
        planar=b''.join(native[c+b*16+p:c+b*16+p+4]
                        for c in range(0,len(native),16384)
                        for p in (0,4,8,12) for b in range(1024))
        self.assertEqual(planar_to_native_dxt3(planar),native)
        self.assertNotEqual(planar,native)

    def test_unfiltered_payload_reproduces_pixel_corruption(self):
        blocks=[bytes([i%256])*8+bytes.fromhex('ffff000000000000') for i in range(1024)]
        native=b''.join(blocks)
        planar=b''.join(block[p:p+4] for p in (0,4,8,12) for block in blocks)
        correct=decode_native_dxt3(native,128,128)
        broken=decode_native_dxt3(planar,128,128)
        self.assertFalse(np.array_equal(correct,broken))
        self.assertTrue(np.array_equal(correct,decode_native_dxt3(planar_to_native_dxt3(planar),128,128)))

    def test_reject_incomplete_filter_chunk_and_atlas(self):
        for data in (b'',bytes(16383),bytes(16385)):
            with self.assertRaises(ValueError):planar_to_native_dxt3(data)
        with self.assertRaises(ValueError):decode_native_dxt3(bytes(16),8,4)

if __name__=='__main__':unittest.main()
