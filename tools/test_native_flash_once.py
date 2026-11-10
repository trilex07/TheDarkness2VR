"""CPU control-flow checks, no native APIs/process reads."""
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch
import native_flash_once as n


class OnceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);(self.root/'session').mkdir();(self.root/'bootstrap').mkdir()
        self.owned=dict(pid=42,hwnd=16,created_ticks=123,exe='EXACT.exe',token='CURRENT',runtime='runtime',log='log')
        self.expected={k:self.owned[k] for k in ('pid','hwnd','created_ticks','exe','token')}
        self.expected.update(deadline=1240,session_id='42-current-1')
        (self.root/'session/owned-window.json').write_text(json.dumps(self.owned))
        (self.root/'bootstrap/focus-expected.json').write_text(json.dumps(self.expected))
        self.packet=dict(runtime_json='runtime.json',native_helper='C:/Users/Trilex/Darkness2VR/.unattended-audit-20261110/tools/native-camera/flash_native_receipt.py')
        self.calls=[];self.foreground=16;self.phases=[1,1]
        test=self
        class Phase:
            def __init__(self,path,marker,check):self.check=check
            def read(self):self.check();return dict(flags=test.phases.pop(0))
        self.phase=Phase

    def invoke(self,returncode=0,guard=None):
        def owned(e,b,a):self.assertEqual(e['deadline'],1230)
        def call(args,check):self.calls.append(args);self.assertFalse(check);return SimpleNamespace(returncode=returncode)
        return n.invoke(self.root,self.packet,backend=object(),
            api=SimpleNamespace(foreground=lambda:self.foreground),phase_type=self.phase,
            owner_guard=guard or owned,call=call)

    def test_inert_default(self):
        with patch.object(Path,'read_bytes',side_effect=AssertionError('file access')):
            self.assertEqual(n.main([]),0)

    def test_exact_one_call_output_and_current_args(self):
        self.assertEqual(self.invoke(),0);self.assertEqual(len(self.calls),1)
        args=self.calls[0]
        for name,value in (('--pid','42'),('--hwnd','16'),('--created-ticks','123'),
                           ('--session-id','42-current-1'),('--owner-token','CURRENT'),
                           ('--output',str(self.root/'native-once.json'))):
            self.assertEqual(args[args.index(name)+1],value)
        self.assertFalse(any('image' in a or 'ttl' in a for a in args))

    def test_native_refusal_is_returned_once_without_retry(self):
        self.assertEqual(self.invoke(returncode=2),2);self.assertEqual(len(self.calls),1)

    def test_unknown_or_changed_phase_never_calls(self):
        for phases in ([0],[2],[4],[1,2]):
            self.phases=list(phases)
            with self.assertRaises(ValueError):self.invoke()
        self.assertFalse(self.calls)

    def test_foreign_foreground_and_owner_refuse(self):
        self.foreground=17
        with self.assertRaises(ValueError):self.invoke()
        self.foreground=16
        def lost(*args):raise ValueError('lost token/creation/path')
        with self.assertRaises(ValueError):self.invoke(guard=lost)
        self.assertFalse(self.calls)

    def test_lifecycle_stop_refuses(self):
        (self.root/'session/REQUEST-CLOSE').touch()
        with self.assertRaises(ValueError):self.invoke()
        self.assertFalse(self.calls)

    def test_bootstrap_identity_mismatch_refuses(self):
        self.expected['created_ticks']=456
        (self.root/'bootstrap/focus-expected.json').write_text(json.dumps(self.expected))
        with self.assertRaises(ValueError):self.invoke()
        self.assertFalse(self.calls)

    def test_no_capture_age_dependency(self):
        # This path requires no image file/time at all: actual pixel review is
        # performed by the agent, current scene/identity is checked at dispatch.
        self.assertEqual(self.invoke(),0)
        self.assertEqual(len(self.calls),1)


if __name__=='__main__':unittest.main()
