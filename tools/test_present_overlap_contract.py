"""Source contract for OverlapNativePresent (offline; not HMD/Steam evidence)."""
from pathlib import Path
import re, unittest
ROOT = Path(__file__).resolve().parents[1]
D3D9 = (ROOT / 'src/d3d9.cpp').read_text(encoding='utf-8')
OBS = (ROOT / 'src/image_present_observer.h').read_text(encoding='utf-8')


def between(text, start, end):
    i = text.index(start)
    return text[i:text.index(end, i)]


class PresentOverlapContract(unittest.TestCase):
    def test_pair_ordering(self):
        b = between(D3D9, 'static void RenderEyePairFromReplay(IDirect3DDevice9* dev) {',
                    '// ---------------------------------------------------------------- XInput')
        right = b.index('StartStereoTransfer(dev,1,rightTransfer,rightJob,rightCapture)')
        restore = b.index('dev->StretchRect(g_liveColorBackup, nullptr, liveEnd.targets[0]')
        apply_ = b.index('if(!liveEnd.Apply(dev))')
        overlap = b.index('OverlapNativePresent(rightTransfer)')
        finish = b.index('eye1Ok=FinishStereoTransfer(rightTransfer,rightJob,rightCapture)')
        observe = b.index('ObservePresentOverlap(presentOverlap')
        self.assertTrue(right < restore < apply_ < overlap < finish < observe)
        self.assertEqual(b.count('OverlapNativePresent('), 1)

    def test_openxr_order_unchanged(self):
        body = between(D3D9, 'static void VrFrameBody(IDirect3DDevice9* dev, const char* caller) {',
                       'static void ApplyBoundaryDecision(')
        self.assertTrue(body.index('RenderEyePairFromReplay(dev);') < body.index('SubmitXR(dev);')
                        < body.index('BeginTrackedFrame();'))
        self.assertNotIn('xrEndFrame', between(D3D9, 'static PresentOverlapSample OverlapNativePresent(',
                                               'static void RenderEyePairFromReplay('))

    def test_gate(self):
        g = between(D3D9, 'static PresentOverlapSample OverlapNativePresent(', 'static void ObservePresentOverlap(')
        for need in ('!g_overlapNativePresent', '!g_imagePreservingPresent', 'g_stereoTransferMode!=4',
                     '!right.Staged()', 'matched_capture::active', 'g_presentOverlap.Allow()',
                     'right.Poll()!=S_FALSE', 'Observer::ForwardEarly()'):
            self.assertIn(need, g)
        self.assertLess(g.index('g_presentOverlap.Allow()'), g.index('Observer::ForwardEarly()'))
        self.assertIn('static bool g_overlapNativePresent = false;', D3D9)
        self.assertIn('"OverlapNativePresent", 0, chosenIni', D3D9)
        ini = (ROOT / 'Darkness2VR.ini').read_text(encoding='utf-8-sig')
        self.assertIn('\nOverlapNativePresent=0\n', ini)
        # Whole feature compiled only into the protected diagnostic build.
        pre = D3D9[:D3D9.index('static d2vr_present_overlap::Policy g_presentOverlap;')]
        self.assertTrue(pre.rstrip().endswith('#ifdef D2VR_MATCHED_CAPTURE') or
                        pre.rfind('#ifdef D2VR_MATCHED_CAPTURE') > pre.rfind('#endif'))

    def test_observer_single_forward(self):
        on = between(OBS, 'HRESULT OnPresent(', 'public:')
        self.assertNotIn('next(sc,a,b,w,r,flags)', on)          # only Forward performs it
        self.assertEqual(on.count('d2vr_present_overlap::Finish(early.slot,forward)'), 1)
        self.assertIn('early.slot.armed=outer && qualified && ready;', on)
        arm = on.index('early_=&early;')
        before = on.index('cb_.before(sc,caller,qualified,ready);')
        disarm = on.index('early_=previousEarly;', before)
        finish = on.index('d2vr_present_overlap::Finish(')
        self.assertTrue(arm < before < disarm < finish)
        fwd = between(OBS, 'struct Forward {', 'struct Early {')
        self.assertTrue(fwd.index('++self->forwarded;') < fwd.index('next(sc,a,b,w,r,flags)') < fwd.index('++self->returned;'))


if __name__ == '__main__':
    unittest.main()
