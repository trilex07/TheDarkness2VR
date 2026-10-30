import importlib.util
import pathlib
import sys
import tempfile
import unittest

FIXTURE=pathlib.Path(sys.argv.pop(1))
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('history',ROOT/'tools/perf/summarize_command_history.py')
history=importlib.util.module_from_spec(spec);spec.loader.exec_module(history)

class ActualHistory(unittest.TestCase):
    def test_cpp_layout_and_nested_publication(self):
        result=history.read(FIXTURE)
        self.assertEqual(result['trigger_sequence'],2)
        self.assertEqual(result['events'][0]['wall_ms'],None)
        self.assertEqual(result['events'][1]['wall_ms'],2.9)
        self.assertEqual(result['events'][1]['pipeline']['vs'],123)
        self.assertEqual(result['events'][1]['pipeline']['known'],1)
    def test_actual_worker_export_layout(self):
        exported=pathlib.Path((FIXTURE.parent/'worker-result-path.txt').read_text())
        result=history.read(exported)
        self.assertTrue(result['events'])
        self.assertGreater(result['arm_qpc'],0)
        self.assertEqual(result['trigger_sequence'],0) # explicit end, not an invented stall
    def test_reject_truncated_and_trailing_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            p=pathlib.Path(folder)/'bad.bin';raw=FIXTURE.read_bytes()
            for bad in (raw[:10],raw[:-1],raw+b'x'):
                p.write_bytes(bad)
                with self.assertRaises(ValueError):history.read(p)
    def test_no_per_draw_disk_or_renderer_wait(self):
        source=(ROOT/'src/command_history_win.inl').read_text()
        scope=source[source.index('struct Scope {'):source.index('// Called only after')]
        for forbidden in ('Log(', 'fwrite(', 'WaitFor', 'Sleep(', 'AcquireSRW', 'new '):
            self.assertNotIn(forbidden,scope)
        self.assertLess(source.index('WaitForSingleObject(exportEvent'),source.index('fopen_s('))
        self.assertIn('"CommandHistory",0,',source)
        self.assertIn("'CommandHistory'",(ROOT/'tools/assert-candidate-profile.ps1').read_text())
        self.assertIn('!Building(d)',source)
    def test_all_real_draws_and_producer_flags_preserved(self):
        source=(ROOT/'src/stereo_replay.inl').read_text()
        for call in ('DrawPrimitive','DrawIndexedPrimitive','DrawPrimitiveUP','DrawIndexedPrimitiveUP'):
            self.assertIn('return command.Finish(g_orig.'+call+'(',source)
        shared=(ROOT/'src/shared_capture.inl').read_text()
        self.assertEqual(shared.count('GetData(nullptr,0,D3DGETDATA_FLUSH)'),2)
        renderer=(ROOT/'src/d3d9.cpp').read_text()
        self.assertIn('g_origSetTexture=d2vr_replay::g_orig.SetTexture;',renderer)
    def test_first_use_counts_reset_at_each_flush(self):
        def draw(vs,ps,known=3):
            return {'name':'drawIndexed','pipeline':{'vs':vs,'ps':ps,'known':known}}
        def flush(seq,eye):
            return {'name':'producerFlush','sequence':seq,'frame':seq,'args':[eye,1,0,0,0,0],'wall_ms':0.1}
        rows=history.first_use_intervals([
            draw(1,10),draw(2,10),flush(1,0),
            draw(1,10),draw(2,10),flush(2,1),
            draw(1,11),draw(3,12),draw(4,99,known=1),flush(3,0)])
        self.assertTrue(rows[0]['window_start'])
        self.assertEqual((rows[0]['new_vs'],rows[0]['new_ps'],rows[0]['new_pairs']),(2,1,2))
        self.assertEqual((rows[1]['new_vs'],rows[1]['new_ps'],rows[1]['new_pairs'],rows[1]['draws']),(0,0,0,2))
        self.assertFalse(rows[2]['window_start'])
        # The unknown-pipeline draw is excluded rather than counted as a new shader.
        self.assertEqual((rows[2]['new_vs'],rows[2]['new_ps'],rows[2]['new_pairs'],rows[2]['draws']),(1,2,2,2))
    def test_export_after_etw_stop_and_exact_symbols_required(self):
        source=(ROOT/'tools/perf/Capture-DarknessCombat.ps1').read_text(encoding='utf-8-sig')
        self.assertLess(source.index('& $wpr -stop $etl'),source.index('$null=$historyExport.Set()'))
        self.assertIn('if ($OutdoorTransition -or $CommandHistory -or $SymbolSource)',source)

if __name__=='__main__':unittest.main()
