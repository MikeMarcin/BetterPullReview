#!/usr/bin/env python3
"""Reject missing cases, workload, timing and configuration credit."""
from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import qualification_case_inventory as inventory
from workbench_case_inventory import expected_rows


class QualificationInventoryTests(unittest.TestCase):
    def fixture(self):
        rows = json.loads((ROOT / 'tests/qualification_case_inventory.json').read_text())['rows']
        expected = {row['name']: row['cases'] for row in rows}
        expected['qt_workbench_window_scale_100'] = expected_rows('Windows')['qt_workbench_window_scale_100']['cases']
        manifest = {'tests': [{'name': name, 'config': 'Release'} for name in expected]}
        blocks = []
        for ordinal, (name, cases) in enumerate(expected.items(), 1):
            prefix = 'WORKBENCH' if name.startswith('qt_workbench') else 'QUALIFICATION'
            text = f'{ordinal}/5 Testing: {name}\n{prefix}_BUILD:NDEBUG=1;ITERATOR_DEBUG_LEVEL=0\n'
            text += ''.join(f'{prefix}_CASE_BEGIN:{case}\n{prefix}_CASE_END:{case}\n' for case in cases)
            if name == 'core_diff_compute':
                text += 'QUALIFICATION_WORKLOAD:stop-lines=40000\n'
            if name == 'application_diff_host':
                text += 'QUALIFICATION_WORKLOAD:cancel-lines=30000\n'
            if name == 'application_github_host':
                text += 'BPR-1112 throwing shutdown: 5 allocations\nBPR-1112 start_delete: 8 allocations\n'
            if name == 'qt_composed_inline_drawer_scale_100':
                text += ''.join(f'INLINE_LATENCY_ASSERTION:{key};limit-ms=100\n' for key in ('open', 'scroll', 'tick-go-to', 'markdown-open'))
            blocks.append(text + 'TEST_RUNNER_CHILD_EXIT:0\nTest Pass Reason:\n')
        return manifest, ''.join(blocks)

    def test_complete_inventory_and_rejections(self):
        manifest, log = self.fixture()
        result = inventory.validate(manifest, log, 'Release', 'Windows')
        self.assertEqual(result['case_occurrences'], 237)
        for before, after in [('QUALIFICATION_CASE_BEGIN:', 'MISSING:'), ('QUALIFICATION_CASE_END:', 'MISSING:'),
                              ('stop-lines=40000', 'stop-lines=6000'), ('cancel-lines=30000', 'cancel-lines=6000'),
                              ('limit-ms=100', 'limit-ms=0'), ('NDEBUG=1', 'NDEBUG=0'),
                              ('ITERATOR_DEBUG_LEVEL=0', 'ITERATOR_DEBUG_LEVEL=2'), ('5 allocations', '0 allocations'),
                              ('Test Pass Reason:', 'Test Failed.'), ('TEST_RUNNER_CHILD_EXIT:0', 'TEST_RUNNER_CHILD_EXIT:9')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                inventory.validate(manifest, log.replace(before, after, 1), 'Release', 'Windows')
        unknown = {'tests': [{k: v for k, v in row.items() if k != 'config'} for row in manifest['tests']]}
        with self.assertRaises(ValueError):
            inventory.validate(unknown, log, 'Release', 'Windows')
        manifest['tests'][0]['config'] = 'Debug'
        with self.assertRaises(ValueError):
            inventory.validate(manifest, log, 'Release', 'Windows')
        with self.assertRaises(ValueError):
            inventory.validate({'tests': []}, log, 'Unknown', 'Windows')


if __name__ == '__main__':
    unittest.main()
