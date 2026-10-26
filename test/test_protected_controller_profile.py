"""Protected input profile inheritance; neither Steam nor a headset is used."""
import configparser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "profiles" / "protected-controller-input"


class ProtectedControllerProfileTest(unittest.TestCase):
    def test_input_enabled_independently_of_native_gameplay(self):
        config = configparser.ConfigParser()
        config.read_dict({"Render": {"Width": "1920", "Height": "1071", "NISMode": "0"},
                          "Input": {"EnableVRGamepad": "0", "HeadLookMode": "3"}})
        config.read(PROFILE / "Darkness2VR.overrides.ini")
        self.assertEqual(config.getint("Input", "EnableVRGamepad"), 1)
        self.assertEqual(config.getint("Input", "HeadLookMode"), 0)
        for key in ("NativeLeftAim", "NativeWeaponPose", "NativeTrackedHands",
                    "GestureGamepadBridge", "TentacleGestureBridge"):
            self.assertEqual(config.getint("Input", key), 0)
        for key in ("HeadGeometry", "NativeStereoHeadGeometry", "FovFrustumHook"):
            self.assertEqual(config.getint("Render", key), 0)
        for key, value in {"Width": "1920", "Height": "1071", "NISMode": "0"}.items():
            self.assertEqual(config.get("Render", key), value)

    def test_guard_and_external_observer_required_native_hooks_off(self):
        config = configparser.ConfigParser()
        config.read(PROFILE / "Darkness2VR-diagnostic.ini")
        for key in ("ImagePreservingPresent", "ImagePreservingInput", "BlockNativeImagePatches"):
            self.assertEqual(config.getint("Diagnostics", key), 1)
        for key in ("MatchedFrameCapture", "EnginePresentCallsite", "RestoreTrackedGameplay",
                    "NativeHandsPackHook", "NativeCameraCover", "ProductionResolution"):
            self.assertEqual(config.getint("Diagnostics", key), 0)


if __name__ == "__main__":
    unittest.main()
