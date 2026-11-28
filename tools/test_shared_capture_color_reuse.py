"""Source-order contract for ReuseSharedCaptureColor (offline, not HMD evidence).

The reused slot copies are only correct if (1) the pair serial predates both
captures, (2) the left copy is validated before any replay command overwrites
RT0, (3) the right history is completed after the right capture and BEFORE the
live colour restore overwrites RT0 again, and (4) a capture can only certify a
copy after its StretchRect succeeded.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def pair_body():
    source = (ROOT / 'src/d3d9.cpp').read_text(encoding='utf-8')
    start = source.index('static void RenderEyePairFromReplay(IDirect3DDevice9* dev) {')
    return source[start:source.index('// ---------------------------------------------------------------- XInput', start)]


class SharedCaptureColorReuse(unittest.TestCase):
    def test_pair_ordering(self):
        b = pair_body()
        slots = b.index('const unsigned rightCapture=leftCapture+(splitCapture?1u:0u);')
        serial = b.index('const uint64_t pairCaptureSerial = g_sharedCaptureSerial;')
        left = b.index('StartStereoTransfer(dev,0,leftTransfer,leftJob,leftCapture)')
        pump = b.index('PumpStereoProducer(leftTransfer,leftJob,leftCapture)')
        decide = b.index('const bool colorReuse =')
        backup = b.index('!colorReuse && !BackupLiveColor(')
        seed = b.index('SeedRightEyeBackground(dev, liveEnd.targets[0]')
        defer = b.index('g_rightHistoryFromCapture = colorReuse;')
        replay = b.index('d2vr_replay::ReplayStream(dev, 1, sp1)')
        publish = b.index('FinishRightEyeHistory(dev, liveEnd.targets[0]')
        right = b.index('StartStereoTransfer(dev,1,rightTransfer,rightJob,rightCapture)')
        complete = b.index('CompleteRightEyeHistory(dev, liveEnd.targets[0], rightCopy)')
        restore = b.index('dev->StretchRect(liveBackup, nullptr, liveEnd.targets[0]')
        apply_ = b.index('if(!liveEnd.Apply(dev))')
        self.assertTrue(slots < serial < left < pump < decide < backup < seed < defer <
                        replay < publish < right < complete < restore < apply_)
        # Seed must run with the flag cleared, and the flag is cleared before the right capture.
        self.assertLess(b.index('g_rightHistoryFromCapture = false;'), left)
        self.assertLess(b.index('const bool deferredHistory = g_rightHistoryFromCapture && eye1Ok;'), right)
        self.assertIn('splitCapture && eye0Ok', b[decide:backup])
        self.assertIn('Mirrors(g_sharedCapture[leftCapture].mirror, liveEnd.targets[0], pairCaptureSerial,',
                      b[decide:backup])
        self.assertIn('Mirrors(g_sharedCapture[rightCapture].mirror, liveEnd.targets[0], liveBackupSerial,',
                      b[right:complete])
        self.assertIn('rightCopy = g_sharedCapture[rightCapture].surface9;', b[right:complete])
        self.assertIn('Unchanged(g_sharedCapture[leftCapture].mirror, liveEnd.targets[0], liveBackupSerial)',
                      b[complete:restore])

    def test_capture_certifies_only_successful_copy(self):
        s = (ROOT / 'src/shared_capture.inl').read_text(encoding='utf-8')
        body = s[s.index('static bool CaptureBackbufferToShared('):]
        clear = body.index('slot.mirror={};')
        wait = body.index('WaitSharedReader(slot)')
        copy = body.index('dev->StretchRect(src, nullptr, slot.surface9')
        cert = body.index('if (SUCCEEDED(hr)) slot.mirror={src,++g_sharedCaptureSerial};')
        rel = body.index('src->Release();')
        self.assertTrue(clear < wait < copy < cert < rel)

    def test_history_never_written_through_adopted_slot(self):
        s = (ROOT / 'src/d3d9.cpp').read_text(encoding='utf-8')
        h = s[s.index('struct EyeColorHistory {'):s.index('static IDirect3DSurface9* g_liveColorBackup')]
        self.assertIn('if (target != output || adopted) Release();', h)
        self.assertIn('adopted = false;', h[h.index('void Release()'):h.index('void Invalidate()')])

    def test_reset_releases_history_before_shared_slots(self):
        s = (ROOT / 'src/d3d9.cpp').read_text(encoding='utf-8')
        for hook in ('HRESULT STDMETHODCALLTYPE Hook_Reset(', 'HRESULT STDMETHODCALLTYPE Hook_ResetEx('):
            body = s[s.index(hook):]
            body = body[:body.index('g_origReset')]
            self.assertLess(body.index('g_rightColorHistory.Release();'), body.index('ReleaseSharedCapture()'), hook)

    def test_default_off_and_startup_only(self):
        s = (ROOT / 'src/d3d9.cpp').read_text(encoding='utf-8')
        self.assertIn('static bool g_reuseSharedCaptureColor = false;', s)
        self.assertIn('"ReuseSharedCaptureColor", 0, chosenIni', s)
        self.assertEqual(s.count('g_reuseSharedCaptureColor ='), 2)  # declaration + LoadConfig only
        ini = (ROOT / 'Darkness2VR.ini').read_text(encoding='utf-8-sig')
        self.assertIn('\nReuseSharedCaptureColor=0\n', ini)


if __name__ == '__main__':
    unittest.main()
