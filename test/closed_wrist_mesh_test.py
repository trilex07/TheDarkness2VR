"""Geometry checks use the user's private extracted asset; no game data in Git."""
import collections
import pathlib
import struct
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).parents[1]/'tools'))
import closed_wrist_mesh as mesh
import hands_only_asset as stock
PRIVATE_PACKAGE = None


class Geometry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package=PRIVATE_PACKAGE
        if cls.package is None:raise unittest.SkipTest('Supply private original-header.bin/body package')
        cls.header=(cls.package/'original-header.bin').read_bytes()
        cls.body=(cls.package/'original-body.bin').read_bytes()
        cls.result,cls.stats=mesh.patch(cls.header,cls.body)

    def test_all_lods_cut_closed_and_inside_original_draw_bounds(self):
        vertices=[self.result[stock.VB_OFFSET+i*40:stock.VB_OFFSET+(i+1)*40] for i in range(stock.VB_SIZE//40)]
        positions=[struct.unpack_from('<3f',v) for v in vertices]
        for lod,(start,count) in enumerate(stock.ranges(self.header,b'/HandRevised1')):
            original=struct.unpack_from('<'+'H'*count,self.body,stock.IB_OFFSET+start*2)
            edges=collections.Counter();oriented=collections.Counter()
            for at in range(start,start+count,3):
                tri=struct.unpack_from('<3H',self.result,stock.IB_OFFSET+2*at)
                if len(set(tri))<3:continue
                for i in tri:
                    self.assertTrue(min(original)<=i<=max(original))
                    p=positions[i];origin,axis=self.stats['planes'][0 if p[0]<0 else 1]
                    self.assertGreaterEqual(mesh.dot(mesh.sub(p,origin),axis),-1e-6)
                keys=[tuple(round(x,5) for x in positions[i]) for i in tri]
                for a,b in zip(keys,keys[1:]+keys[:1]):
                    edges[tuple(sorted((a,b)))]+=1;oriented[(a,b)]+=1
            # LOD0 stock seams weld exactly; it must be fully closed, including
            # the original wrist opening. Lower stock LODs already have holes.
            if lod==0:
                self.assertTrue(all(n==2 for n in edges.values()))
                self.assertTrue(all(oriented[e]==1 for e in edges))
            self.assertGreater(self.stats['lods'][lod]['capTriangles'],0)

    def test_retained_geometry_and_asset_layout(self):
        self.assertEqual(len(self.result),len(self.body))
        self.assertEqual(self.result[:stock.VB_OFFSET],self.body[:stock.VB_OFFSET])
        for i in range(stock.VB_SIZE//40):
            at=stock.VB_OFFSET+i*40;old=self.body[at:at+40];new=self.result[at:at+40]
            if new!=old:
                self.assertEqual(sum(new[36:40]),255)
                self.assertTrue(all(b<42 for b,w in zip(new[32:36],new[36:40]) if w))
            p=struct.unpack_from('<3f',old);origin,axis=self.stats['planes'][0 if p[0]<0 else 1]
            hand_referenced=any(i in struct.unpack_from('<'+'H'*c,self.body,stock.IB_OFFSET+s*2)
                                for s,c in stock.ranges(self.header,b'/HandRevised1'))
            if hand_referenced and mesh.dot(mesh.sub(p,origin),axis)>=0:self.assertEqual(old,new)
        for start,count in stock.ranges(self.header,b'/SleeveRevised'):
            for at in range(start,start+count,3):
                tri=struct.unpack_from('<3H',self.result,stock.IB_OFFSET+at*2)
                self.assertEqual(tri,(tri[0],)*3)

    def test_deterministic_and_preimage_refusal(self):
        self.assertEqual(mesh.patch(self.header,self.body)[0],self.result)
        for h,b in [(self.header+b'x',self.body),(self.header,self.result)]:
            with self.assertRaisesRegex(ValueError,'preimage'):mesh.patch(h,b)


if __name__=='__main__':
    # Keep unittest's own argument parser separate from the private asset path.
    if len(sys.argv)>1 and pathlib.Path(sys.argv[-1]).is_dir():
        PRIVATE_PACKAGE=pathlib.Path(sys.argv.pop())
    unittest.main()
