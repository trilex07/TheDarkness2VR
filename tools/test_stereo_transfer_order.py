"""Recording-boundary regression and adversarial two-queue interleaving.

This is an offline ordering test, not a D3D driver/headset acceptance test.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def pair_body():
    source = (ROOT / 'src/d3d9.cpp').read_text(encoding='utf-8')
    start = source.index('static void RenderEyePairFromReplay(IDirect3DDevice9* dev) {')
    return source[start:source.index('    d2vr_replay::DeviceSnapshot liveEnd;', start)]


def delayed_left_read(sealed_before_capture):
    # The D3D11 left Draw is queued, but has not consumed shared texture yet.
    # Replay runs on the other device. It must contain ENGINE commands only.
    recording = not sealed_before_capture
    commands = ['engine_draw_right']
    shared = 'left'
    if recording:
        commands.append('internal_copy_rt0_to_shared')
    rt0 = 'left'
    for command in commands:
        if command == 'engine_draw_right':
            rt0 = 'right'
        else:
            shared = rt0  # replay bypasses the explicit capture reader fence
    left = shared  # WaitSharedReader now completes the delayed left read
    shared = rt0  # the explicitly fenced second capture
    return left, shared, commands


class StereoTransferOrder(unittest.TestCase):
    def test_real_pipeline_seals_before_any_internal_capture(self):
        body = pair_body()
        seal = body.index('d2vr_replay::g_recActive = false;')
        for operation in ('EnsureSwapchains(', 'EnsureSharedCapture(',
                          'matched_capture::Source(', 'CaptureBackbufferToShared('):
            self.assertLess(seal, body.index(operation), operation)
        self.assertNotIn('g_recActive = true', body)

    def test_old_interleaving_reproduces_right_image_in_both_eyes(self):
        left, right, commands = delayed_left_read(False)
        self.assertEqual((left, right), ('right', 'right'))
        self.assertIn('internal_copy_rt0_to_shared', commands)

    def test_sealed_interleaving_preserves_both_eye_sources(self):
        left, right, commands = delayed_left_read(True)
        self.assertEqual((left, right), ('left', 'right'))
        self.assertEqual(commands, ['engine_draw_right'])


if __name__ == '__main__':
    unittest.main()
