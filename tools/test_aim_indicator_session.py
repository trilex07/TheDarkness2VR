"""CPU-only binding/entrypoint negatives; no native backend or live process."""
import contextlib
from datetime import datetime, timezone
import io
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch
import aim_indicator_session as s


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        game = self.root/'game'; game.mkdir()
        ini = '[Input]\nDoubleShakeReload=1\nDeferredGripRetry=1\nThrowSolverFailElevation=1\n'
        for name, text in [('d3d9.dll','CURRENT81FD'),('Darkness2VR.ini',ini),
                           ('Darkness2VR-diagnostic.ini','CURRENTDIAG')]:
            (game/name).write_text(text)
        candidate = self.root/'candidate.dll'; candidate.write_text('DOTONLY')
        simulator = self.root/'sim.dll'; simulator.write_text('A76')
        runtime = self.root/'runtime.json'
        runtime.write_text(json.dumps({'runtime':{'library_path':str(simulator)}}))
        self.packet = dict(game=str(game),local='local',roaming='roaming',tools='tools',
            settings='settings',native_helper='native',source_commit='DOTSOURCE',
            candidate=str(candidate),candidate_sha256=s.digest(candidate),
            runtime_json=str(runtime),simulator=str(simulator),
            profile={p.name:s.digest(p) for p in game.iterdir()},protected_files={},
            pins=[dict(path=str(simulator),sha256=s.digest(simulator))])
        self.packet_file = self.root/'packet.json'
        self.save_packet()
        self.now = 1000
        self.grant = dict(parent_granted_runtime=True,token='CURRENT_TOKEN',
                         acquired_utc=datetime.fromtimestamp(990,timezone.utc).isoformat())

    def save_packet(self):
        self.packet_file.write_text(json.dumps(self.packet))
        self.packet_sha = s.digest(self.packet_file)

    def test_default_is_inert(self):
        with patch.object(s,'load',side_effect=AssertionError('file access')):
            self.assertEqual(s.main([]),0)

    def test_cli_requires_complete_run_arguments_before_load(self):
        with patch.object(s,'load',side_effect=AssertionError('file access')):
            with self.assertRaises(SystemExit):s.main(['--run','--packet','p','--packet-sha256','h'])

    def test_current_profile_passes(self):
        self.assertEqual(s.load(self.packet_file,self.packet_sha),self.packet)

    def test_wrong_packet_rejects(self):
        with self.assertRaisesRegex(ValueError,'packet SHA'):s.load(self.packet_file,'0'*64)

    def test_changed_user_ini_refuses_without_refresh(self):
        (Path(self.packet['game'])/'Darkness2VR.ini').write_text('USER_CHANGED')
        with self.assertRaisesRegex(ValueError,'Current profile changed'):s.load(self.packet_file,self.packet_sha)

    def test_changed_pin_rejects(self):
        Path(self.packet['simulator']).write_text('WRONG')
        with self.assertRaisesRegex(ValueError,'Pinned file differs'):s.load(self.packet_file,self.packet_sha)

    def test_disabled_user_option_refuses_even_if_packet_rehashed(self):
        ini=Path(self.packet['game'])/'Darkness2VR.ini'
        ini.write_text(ini.read_text().replace('DoubleShakeReload=1','DoubleShakeReload=0'))
        self.packet['profile'][ini.name]=s.digest(ini); self.save_packet()
        with self.assertRaisesRegex(ValueError,'option1'):s.load(self.packet_file,self.packet_sha)

    def module(self, refusal=False, failure=False):
        old_profile={'d3d9.dll':'HISTORICAL39D'}
        module=SimpleNamespace(BFE='HISTORICALBFE',backup=SimpleNamespace(
            PROFILE_HASHES=old_profile,check_path=lambda p:Path(p)))
        test=self
        class FakeAdapter:
            def __init__(self,**kw):self.kw=kw;module.kw=kw
            def lease(self,no_games):
                test.assertTrue(no_games)
                test.assertEqual(module.BFE,test.packet['candidate_sha256'])
                test.assertEqual(module.backup.PROFILE_HASHES,test.packet['profile'])
                test.assertEqual(self.MAX_INPUT_SECONDS,360)
                test.assertEqual(self.MAX_TOTAL_SECONDS,480)
                if refusal:raise ValueError('foreign token/game')
            def run(self):
                module.called=True
                if failure:raise ValueError('native lifecycle refusal')
        module.Adapter=FakeAdapter
        return module,old_profile

    def test_exact_current_binding_original_deadlines_restored_after_callback(self):
        m,old=self.module(); root=self.root/'unused'
        s.run(self.packet,self.grant,root,m,now=lambda:self.now,backend=object())
        self.assertTrue(m.called)
        self.assertEqual(m.kw['stop_input'],1340)
        self.assertEqual(m.kw['cleanup'],1470)
        self.assertEqual(m.BFE,'HISTORICALBFE');self.assertIs(m.backup.PROFILE_HASHES,old)
        self.assertEqual(s.load(self.packet_file,self.packet_sha),self.packet)

    def test_foreign_lease_or_game_refuses_before_new_root(self):
        m,old=self.module(refusal=True);root=self.root/'unused'
        with self.assertRaisesRegex(ValueError,'foreign'):
            s.run(self.packet,self.grant,root,m,now=lambda:self.now,backend=object())
        self.assertFalse(root.exists());self.assertIs(m.backup.PROFILE_HASHES,old)

    def test_callback_refusal_restores_private_globals(self):
        m,old=self.module(failure=True)
        with self.assertRaisesRegex(ValueError,'lifecycle'):
            s.run(self.packet,self.grant,self.root/'unused',m,now=lambda:self.now,backend=object())
        self.assertEqual(m.BFE,'HISTORICALBFE');self.assertIs(m.backup.PROFILE_HASHES,old)

    def test_expired_future_missing_grant_and_reused_root_refuse(self):
        for kind in ('expired','future','notgranted','emptytoken','naive','usedroot'):
            with self.subTest(kind=kind):
                m,_=self.module();g=dict(self.grant);root=self.root/kind
                if kind=='expired':g['acquired_utc']=datetime.fromtimestamp(900,timezone.utc).isoformat()
                if kind=='future':g['acquired_utc']=datetime.fromtimestamp(1100,timezone.utc).isoformat()
                if kind=='notgranted':g['parent_granted_runtime']=False
                if kind=='emptytoken':g['token']=''
                if kind=='naive':g['acquired_utc']='1970-01-01T00:16:30'
                if kind=='usedroot':root.mkdir()
                with self.assertRaises(ValueError):s.run(self.packet,g,root,m,now=lambda:self.now,backend=object())
                self.assertFalse(getattr(m,'called',False))


if __name__=='__main__':
    with contextlib.redirect_stdout(io.StringIO()):unittest.main()
