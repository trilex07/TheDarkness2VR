import tempfile, unittest, pathlib, struct
from pack import build, valid

class PackTests(unittest.TestCase):
 def test_valid_vs_ps_and_comments(self):
  for v in (0xfffe0300,0xffff0300):
   self.assertTrue(valid(struct.pack('<4I',v,0x0001fffe,65535,65535)))
 def test_truncation_and_trailing(self):
  raw=struct.pack('<3I',0xfffe0300,0,65535)
  for i in range(len(raw)):self.assertFalse(valid(raw[:i]))
  self.assertFalse(valid(raw+b'\0\0\0\0'))
 def test_byte_exact_dedup(self):
  with tempfile.TemporaryDirectory() as d:
   files=[pathlib.Path(d)/n for n in ('a','b')]
   for f in files:f.write_bytes(struct.pack('<2I',0xfffe0300,65535))
   package,sources=build(files);self.assertEqual(len(sources),1);self.assertEqual(len(package),20)
 def test_no_empty_or_excess_count(self):
  with self.assertRaises(ValueError):build([])
  with tempfile.TemporaryDirectory() as d:
   files=[]
   for i in range(257):
    p=pathlib.Path(d)/str(i);p.write_bytes(struct.pack('<4I',0xfffe0300,0x0001fffe,i,65535));files.append(p)
   with self.assertRaises(ValueError):build(files)
 def test_integration_boundaries(self):
  root=pathlib.Path(__file__).resolve().parents[2];s=(root/'src/d3d9.cpp').read_text();body=s[s.index('void HookDevice('):s.index('HRESULT STDMETHODCALLTYPE Hook_CreateDevice(')]
  self.assertIn('"StartupShaderWarmup", 0, chosenIni',s)
  self.assertLess(body.index('InterlockedCompareExchange(&g_devHooked'),body.index('StartupShaderWarmup(dev)'))
  self.assertLess(body.index('StartupShaderWarmup(dev)'),body.index('InstallReplayHooks(dev)'))
  self.assertEqual(s.count('StartupShaderWarmup(dev)'),1)
  adapter=(root/'src/startup_shader_warmup.inl').read_text()
  for forbidden in ['SetVertexShader(', 'SetPixelShader(', 'GetFunction(', 'g_orig.', 'CreateThread(', 'VirtualProtect(']:self.assertNotIn(forbidden,adapter)
  self.assertEqual(adapter.count('shader->Release()'),2)
if __name__=='__main__':unittest.main()
