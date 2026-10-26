"""Stock gesture opt-in never admits native pose/targeting hooks."""
import configparser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "profiles" / "protected-gesture-actions"


class ProtectedGestureProfileTest(unittest.TestCase):
    def test_stock_actions_keep_unsafe_hooks_and_double_head_rotation_off(self):
        config = configparser.ConfigParser()
        config.read(PROFILE / "Darkness2VR.overrides.ini")
        self.assertEqual(config.getint("Input", "EnableVRGamepad"), 1)
        for key in ("HeadLookMode", "NativeLeftAim", "NativeWeaponPose", "NativeTrackedHands",
                    "GestureGamepadBridge", "TentacleGestureBridge"):
            self.assertEqual(config.getint("Input", key), 0)
        for key in ("HeadGeometry", "NativeStereoHeadGeometry", "FovFrustumHook"):
            self.assertEqual(config.getint("Render", key), 0)
        for key in ("Width", "Height", "NISMode", "StereoIPD"):
            self.assertFalse(config.has_option("Render", key))

    def test_opt_in_requires_protected_input_and_guard(self):
        config = configparser.ConfigParser()
        config.read(PROFILE / "Darkness2VR-diagnostic.ini")
        for key in ("ImagePreservingPresent", "ImagePreservingInput", "BlockNativeImagePatches", "ProtectedGestureActions"):
            self.assertEqual(config.getint("Diagnostics", key), 1)
        for key in ("MatchedFrameCapture", "EnginePresentCallsite", "RestoreTrackedGameplay",
                    "NativeHandsPackHook", "NativeCameraCover", "ProductionResolution"):
            self.assertEqual(config.getint("Diagnostics", key), 0)
        source = (ROOT / "src" / "d3d9.cpp").read_text(encoding="utf-8-sig")
        self.assertIn('"ProtectedGestureActions",0,', source)
        self.assertIn('protectedInputAllowed && protectedGestureAllowed', source)


if __name__ == "__main__":
    unittest.main()
