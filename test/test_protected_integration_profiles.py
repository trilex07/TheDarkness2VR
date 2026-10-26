"""Compose public protected profiles; no installation or runtime is used."""
import configparser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def compose(*profiles):
    config = configparser.ConfigParser()
    config.read_dict({"Render": {"SourceQuality": "3", "UniqueReplayResourceRefs": "1",
                               "StereoIPD": "0.065", "RenderWidth": "2048"},
                      "Diagnostics": {"ProtectedHeadTracking": "1", "ProtectedGestureActions": "1"}})
    for profile in profiles:
        for filename in ("Darkness2VR.overrides.ini", "Darkness2VR-diagnostic.ini"):
            config.read(ROOT / "profiles" / profile / filename)
    return config


class ProtectedIntegrationProfiles(unittest.TestCase):
    def test_input_does_not_inherit_other_experiments(self):
        c = compose("protected-controller-input")
        for key in ("SourceQuality", "UniqueReplayResourceRefs", "HeadGeometry", "NativeStereoHeadGeometry"):
            self.assertEqual(c.getint("Render", key), 0)
        for key in ("ProtectedHeadTracking", "ProtectedGestureActions"):
            self.assertEqual(c.getint("Diagnostics", key), 0)
        self.assertEqual(c.getint("Input", "EnableVRGamepad"), 1)
        self.assertEqual(c.get("Render", "StereoIPD"), "0.065")
        self.assertEqual(c.get("Render", "RenderWidth"), "2048")

    def test_head_overlay_preserves_input_and_guard(self):
        c = compose("protected-controller-input", "protected-head-tracking")
        for key in ("ProtectedHeadTracking", "ImagePreservingInput", "ImagePreservingPresent", "BlockNativeImagePatches"):
            self.assertEqual(c.getint("Diagnostics", key), 1)
        for key in ("HeadGeometry", "NativeStereoHeadGeometry"):
            self.assertEqual(c.getint("Render", key), 1)
        self.assertEqual(c.getint("Input", "HeadLookMode"), 0)
        for key in ("SourceQuality", "UniqueReplayResourceRefs"):
            self.assertEqual(c.getint("Render", key), 0)
        for key in ("NativeTrackedHands", "NativeLeftAim", "NativeWeaponPose", "TentacleGestureBridge", "GestureGamepadBridge"):
            self.assertEqual(c.getint("Input", key), 0)

    def test_gesture_overlay_retains_separation(self):
        c = compose("protected-controller-input", "protected-gesture-actions")
        for key in ("ProtectedGestureActions", "ImagePreservingInput", "ImagePreservingPresent", "BlockNativeImagePatches"):
            self.assertEqual(c.getint("Diagnostics", key), 1)
        self.assertEqual(c.getint("Diagnostics", "ProtectedHeadTracking"), 0)
        for key in ("SourceQuality", "UniqueReplayResourceRefs", "HeadGeometry", "NativeStereoHeadGeometry"):
            self.assertEqual(c.getint("Render", key), 0)
        for key in ("NativeTrackedHands", "NativeLeftAim", "NativeWeaponPose", "TentacleGestureBridge", "GestureGamepadBridge"):
            self.assertEqual(c.getint("Input", key), 0)

    def test_combined_candidate_owns_reconstruction_without_mutating_saved_choice(self):
        c = compose("protected-controls-head-gestures")
        for key in ("ProtectedGestureActions", "ProtectedHeadTracking", "ImagePreservingInput", "ImagePreservingPresent", "BlockNativeImagePatches"):
            self.assertEqual(c.getint("Diagnostics", key), 1)
        for key in ("HeadGeometry", "NativeStereoHeadGeometry"):
            self.assertEqual(c.getint("Render", key), 1)
        self.assertEqual(c.getint("Render", "RestoreSavedLighting"), 0)
        for key in ("SourceQuality", "UniqueReplayResourceRefs", "StereoShiftMode", "StereoLayerPose"):
            self.assertEqual(c.getint("Render", key), 0)
        self.assertEqual(c.getint("Render", "ProjectionFix"), 3)
        for key in ("HeadLookMode", "NativeTrackedHands", "NativeLeftAim", "NativeWeaponPose", "TentacleGestureBridge", "GestureGamepadBridge"):
            self.assertEqual(c.getint("Input", key), 0)

    def test_quality_remains_separate(self):
        c = compose("protected-controller-input")
        c.read(ROOT / "profiles/image-preserving-source-quality.overrides.ini")
        self.assertEqual(c.getint("Render", "SourceQuality"), 2)
        for key in ("UniqueReplayResourceRefs", "HeadGeometry", "NativeStereoHeadGeometry"):
            self.assertEqual(c.getint("Render", key), 0)
        self.assertEqual(c.getint("Diagnostics", "ProductionResolution"), 0)

    def test_performance_remains_separate(self):
        c = compose("protected-controller-input", "protected-replay-performance")
        self.assertEqual(c.getint("Render", "UniqueReplayResourceRefs"), 1)
        for key in ("SourceQuality", "HeadGeometry", "NativeStereoHeadGeometry"):
            self.assertEqual(c.getint("Render", key), 0)


if __name__ == "__main__":
    unittest.main()
