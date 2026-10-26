"""Protected-profile inheritance regression; no game or runtime is required."""
import configparser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "profiles" / "image-preserving-vr"


class ProtectedProfileTest(unittest.TestCase):
    def test_inherited_head_clip_is_disabled_without_changing_quality(self):
        for inherited in (0, 1):
            with self.subTest(inherited=inherited):
                config = configparser.ConfigParser()
                config.read_dict({"Render": {
                    "HeadGeometry": "1", "NativeStereoHeadGeometry": str(inherited),
                    "StereoIPD": "0.065", "Width": "2048", "Height": "2091",
                    "FovWidenX": "1.78", "NISMode": "0",
                }})
                config.read(PROFILE / "Darkness2VR.overrides.ini")
                self.assertEqual(config.getint("Render", "HeadGeometry"), 0)
                self.assertEqual(config.getint("Render", "NativeStereoHeadGeometry"), 0)
                self.assertEqual(config.getint("Render", "SubmitMode"), 4)
                self.assertEqual(config.getint("Render", "StereoMode"), 1)
                self.assertEqual(config.getint("Render", "DisableOpenXR"), 0)
                for key, value in {"StereoIPD": "0.065", "Width": "2048",
                                   "Height": "2091", "FovWidenX": "1.78", "NISMode": "0"}.items():
                    self.assertEqual(config.get("Render", key), value)

    def test_native_image_guard_and_unsafe_hook_restrictions_remain(self):
        config = configparser.ConfigParser()
        config.read(PROFILE / "Darkness2VR-diagnostic.ini")
        for key in ("ImagePreservingPresent", "BlockNativeImagePatches"):
            self.assertEqual(config.getint("Diagnostics", key), 1)
        for key in ("MatchedFrameCapture", "EnginePresentCallsite", "RestoreTrackedGameplay",
                    "NativeHandsPackHook", "NativeCameraCover", "ProductionResolution"):
            self.assertEqual(config.getint("Diagnostics", key), 0)

    def test_cpu_timing_enabled_before_present_callbacks_are_latched(self):
        source = (ROOT / "src" / "d3d9.cpp").read_text()
        # HookDevice is already protected by its once-only g_devHooked gate.
        # Leaving Enable in the later legacy installer latches null callbacks
        # in the protected observer even when CpuSyncTiming=1.
        self.assertEqual(source.count("d2vr_cpu_sync::timing.Enable("), 1)
        hook = source[source.index("void HookDevice(IDirect3DDevice9* dev)"):]
        enabled = hook.index("d2vr_cpu_sync::timing.Enable(")
        self.assertLess(hook.index("InterlockedCompareExchange(&g_devHooked"), enabled)
        self.assertLess(enabled, hook.index("InstallImagePresent(dev)"))
        self.assertLess(enabled, hook.index("InstallDiagnosticEnginePresent()"))
        self.assertIn('"CpuSyncTiming",0,', hook[:hook.index("InstallImagePresent(dev)")])


if __name__ == "__main__":
    unittest.main()