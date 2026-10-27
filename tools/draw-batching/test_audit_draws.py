import unittest
from audit_draws import audit


def trace(rows):
    return '\n'.join(f'{i} op={op} a=' + ','.join(map(str, (tuple(a)+(0,)*5)[:5]))
                     for i, (op, a) in enumerate(rows))

BIND = [(17,(10,)),(19,(20,)),(26,(30,)),(23,(0,40,0,32)),(25,(50,))]
DRAW = (14,(4,0,0,24,0))


class EligibilityTests(unittest.TestCase):
    def test_matching_geometry_never_admits_missing_primitive_count(self):
        r=audit(trace(BIND+[DRAW,DRAW]))
        self.assertEqual(r['adjacent_pair_partition'],{'same_visible_geometry_signature_only':1})
        self.assertEqual(r['proven_safe_merge_draws'],0)
        self.assertEqual(r['true_eligible_draw_count'],'UNKNOWN')

    def test_upload_offset_equality_is_not_value_equality(self):
        r=audit(trace(BIND+[DRAW,(20,(0,4,0,0,64)),DRAW]))
        self.assertTrue(r['pairs'][0]['opaque_state_changed'])
        self.assertEqual(r['proven_safe_merge_draws'],0)

    def test_stateblock_forgets_bindings_and_scalar_history(self):
        r=audit(trace(BIND+[DRAW,(36,(1,)),(23,(0,40,0,32)),DRAW]))
        self.assertFalse(r['draws'][-1]['geometry_binding_known'])
        self.assertEqual(r['repeated_inline_setter_attempts'],{})

    def test_up_unbinds_stream_zero(self):
        r=audit(trace(BIND+[(15,(4,1,32,0)),DRAW]))
        self.assertFalse(r['draws'][-1]['geometry_binding_known'])

    def test_indexed_up_unbinds_indices(self):
        r=audit(trace(BIND+[(16,(4,0,3,1,101)),(23,(0,40,0,32)),DRAW]))
        self.assertFalse(r['draws'][-1]['geometry_binding_known'])

    def test_frequency_requires_both_markers_and_resets(self):
        r=audit(trace(BIND+[(24,(0,0x40000008)),DRAW,
                           (24,(1,0x80000001)),DRAW,(24,(0,1)),(24,(1,1)),DRAW]))
        self.assertEqual([d['instance_count'] for d in r['draws']],[None,8,None])

    def test_resource_barrier_prevents_candidate(self):
        r=audit(trace(BIND+[DRAW,(30,(1,2)),DRAW]))
        self.assertEqual(r['pairs'][0]['category'],'ordering_or_resource_barrier')

    def test_repeated_inline_is_only_attempt_count(self):
        r=audit(trace([(8,(27,1)),(8,(27,1)),(8,(27,0)),(8,(27,0))]))
        self.assertEqual(r['repeated_inline_setter_attempts'],{8:2})

    def test_invalid_or_incomplete_stream_rejected(self):
        for s in ['1 op=14 a=4,0,0,24,0','0 op=14 a=4,0','0 op=99 a=0,0,0,0,0','0 op=x a=0']:
            with self.assertRaises(ValueError): audit(s)

    def test_unknown_start_is_not_default_state(self):
        r=audit(trace([DRAW,DRAW]))
        self.assertEqual(r['pairs'][0]['category'],'unknown_geometry_binding')


if __name__=='__main__': unittest.main()
