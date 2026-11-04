import unittest
from summarize_world_cadence import summarize


def cadence(layers=90, world=90):
    return f"[XRCadence] layers={layers:.1f}/s game-boundaries={layers:.1f}/s world-bearing={world:.1f}/s"


class CadenceTests(unittest.TestCase):
    def test_transitions_and_video_cannot_inflate_normal_3d(self):
        report = summarize("\n".join([
            "[CinematicTheater] mode=0 frame=1", cadence(40, 40), cadence(41, 41),
            "[CinematicTheater] mode=2 frame=2", cadence(), cadence(),
            "[CinematicTheater] mode=0 frame=3", cadence(70, 70), cadence(42, 42)]))
        self.assertEqual(report["groups"]["normal_3d"], dict(windows=2, minimum=41, maximum=42, mean=41.5))
        self.assertEqual(report["groups"]["video_theater"]["windows"], 1)

    def test_unknown_mixed_and_return_transition_are_rejected(self):
        report = summarize("\n".join([cadence(), cadence(),
            "[CinematicTheater] mode=0 frame=1", cadence(), cadence(90, 40),
            "[CinematicTheater] mode=2 frame=2", "[CinematicTheater] mode=0 frame=3",
            cadence(), cadence(80, 80)]))
        self.assertEqual(report["groups"]["normal_3d"]["windows"], 1)
        self.assertEqual([r["rejection"] for r in report["rows"]],
                         ["initial window", "unknown mode", "mode transition", "mixed boundaries", "mode transition", None])


if __name__ == "__main__":
    unittest.main()
