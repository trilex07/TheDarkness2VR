"""Source contract for pipeline-variant learning (offline; not HMD/Steam evidence)."""
from pathlib import Path
import re, unittest
ROOT = Path(__file__).resolve().parents[1]
REPLAY = (ROOT / 'src/stereo_replay.inl').read_text(encoding='utf-8')
D3D9 = (ROOT / 'src/d3d9.cpp').read_text(encoding='utf-8')
WARM = (ROOT / 'src/pipeline_warmup_d3d9.inl').read_text(encoding='utf-8')


def body(text, signature):
    start = text.index(signature)
    brace = text.index('{', start); depth = 0
    for i in range(brace, len(text)):
        depth += text[i] == '{'; depth -= text[i] == '}'
        if not depth:
            return text[start:i + 1]


HOOKS = [
    (REPLAY, 'static HRESULT STDMETHODCALLTYPE Hook_SetRenderTarget(', 'TrackTarget(idx,s)'),
    (REPLAY, 'static HRESULT STDMETHODCALLTYPE Hook_SetRenderState(', 'TrackRenderState(st,v)'),
    (REPLAY, 'static HRESULT STDMETHODCALLTYPE Hook_SetSamplerState(', 'TrackSamplerSrgb(smp,t,v)'),
    (REPLAY, 'static HRESULT STDMETHODCALLTYPE Hook_SetVertexDeclaration(', 'TrackDeclaration(d)'),
    (REPLAY, 'static HRESULT STDMETHODCALLTYPE Hook_SetFVF(', 'TrackFvf(fvf)'),
    (REPLAY, 'static HRESULT STDMETHODCALLTYPE Hook_SetStreamSourceFreq(', 'TrackFrequency(num,freq)'),
    (D3D9, 'HRESULT STDMETHODCALLTYPE Hook_SetTexture(', 'TrackTexture(Stage,pTexture)'),
    (D3D9, 'HRESULT STDMETHODCALLTYPE Hook_SetDepthStencilSurface(', 'TrackDepth(pNewZStencil)'),
]


class PipelineVariantLearning(unittest.TestCase):
    def test_tracking_after_success_and_never_while_building_stateblocks(self):
        for text, sig, call in HOOKS:
            b = body(text, sig)
            self.assertEqual(b.count('d2vr_pipeline_warmup::' + call), 1, sig)
            at = b.index('d2vr_pipeline_warmup::' + call)
            if 'd2vr_stateblock::Active' in b:
                self.assertLess(b.index('d2vr_stateblock::Active'), at, sig)
            prefix = b[:at]
            self.assertTrue('SUCCEEDED(result)' in prefix[-80:] or 'if (FAILED(result)) return result;' in prefix, sig)

    def test_learning_key_and_budget(self):
        ob = body(WARM, 'inline void ObserveDraw(')
        self.assertIn('l->live.Key(v,p)', ob)
        self.assertNotIn('(uint64_t(v)<<32)|uint32_t(p)', ob)        # no pair-only key
        self.assertLess(ob.index('l->seen.find(key)'), ob.index('capturesThisFrame>=CapturesPerFrame'))
        self.assertLess(ob.index('capturesThisFrame>=CapturesPerFrame'), ob.index('Capture(dev,record)'))
        defer = ob[ob.index('capturesThisFrame>=CapturesPerFrame'):ob.index('++l->capturesThisFrame')]
        self.assertNotIn('seen.insert', defer)                       # deferred keys are retried
        tick = body(WARM, 'static void Tick(')
        self.assertLess(tick.index('l->capturesThisFrame=0;'), tick.index('if(now<l->nextTick) return;'))

    def test_render_state_filter_is_compile_inputs_only(self):
        rs = body(WARM, 'inline void TrackRenderState(')
        for st in ('D3DRS_SRGBWRITEENABLE', 'D3DRS_ALPHATESTENABLE', 'D3DRS_ALPHAFUNC', 'D3DRS_FOGENABLE', 'D3DRS_CLIPPLANEENABLE'):
            self.assertIn(st, rs)
        self.assertLess(rs.index('default: return;'), rs.index('Tracking()'))  # no work for other states

    def test_capture_records_real_device_state(self):
        cap = body(WARM, 'static bool Capture(')
        self.assertNotIn('live.', cap)                               # signature never feeds the record
        self.assertNotIn('return false;', cap.replace('#define', ''))


if __name__ == '__main__':
    unittest.main()
