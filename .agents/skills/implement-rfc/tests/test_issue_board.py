"""Public bookkeeping behavior against disposable, differently named boards."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import board_model as board
import issue_board as cli
import project_config


class BoardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.config = copy.deepcopy(project_config.load())
        self.config.update(project='fixture', display_name='Fixture', session_prefix='fx',
                           origin_repo_name='Fixture', issue_root='tracker/cards',
                           rfc_root='tracker/rfcs', rfc_link_field='spec',
                           next_id={'mode': 'single-file', 'path': 'tracker/cards/NEXT_ID'},
                           closeout_card={'front_matter': {'closeout': 'true'}})
        self.config['lanes'] = {'backlog': 'backlog', 'ready': 'ready',
            'platform_ready': {'mac': 'mac-ready', 'win': 'win-ready', 'linux': 'linux-ready'},
            'in_progress': 'working', 'parked': 'parked', 'blocked': 'blocked', 'done': 'done'}
        self.config['lane_statuses'] = {'backlog': 'Backlog', 'ready': 'Ready',
            'mac-ready': 'Ready', 'win-ready': 'Ready', 'linux-ready': 'Ready',
            'working': 'In Progress', 'parked': 'Parked', 'blocked': 'Blocked', 'done': 'Done'}
        self.config['bookkeeping'] = {'deferring_lanes': ['backlog', 'blocked', 'mac-ready', 'win-ready', 'linux-ready'],
            'parked_on': ['awaiting-person'], 'blocked_by_forbidden_prefixes': ['platform-'],
            'link_roots': ['tracker', 'README.md'],
            'protected_globs': ['tracker/captured/**'],
            'editable_globs': ['tracker/captured/*.LIVE_IMPORT.md']}
        for lane in self.config['lane_statuses']:
            (self.root / 'tracker/cards' / lane).mkdir(parents=True)
        (self.root / 'tracker/cards/NEXT_ID').write_text('XYZ-0099\n')
        (self.root / 'tracker/rfcs').mkdir()
        (self.root / 'tracker/rfcs/RFC-XYZ-0001-feature.md').write_text('---\nid: RFC-XYZ-0001\nstatus: Proposed\n---\n')
        self.config_path = self.root / 'project.json'
        self.config_path.write_text(json.dumps(self.config))
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True, capture_output=True)

    def card(self, number=1, lane='backlog', deps=(), extra='', body='# Body\n'):
        path = self.root / 'tracker/cards' / lane / f'XYZ-{number:04d}-fixture.md'
        path.write_text(f'---\nid: XYZ-{number:04d}\ntitle: Fixture\n'
                        f'status: {self.config["lane_statuses"][lane]}\nspec: RFC-XYZ-0001\n'
                        f'depends_on: [{", ".join(deps)}]\n{extra}---\n\n{body}')
        return path

    def public(self, *args, expected=0):
        result = subprocess.run([sys.executable, str(Path(cli.__file__)),
            '--repo-root', str(self.root), '--project-config', str(self.config_path), *args, '--json'],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def plan(self, lane, number=1, **kwargs):
        return cli.plan_move(self.root, self.config, f'XYZ-{number:04d}', lane,
                             kwargs.get('parked_on'), kwargs.get('blocked_by'))

    def apply(self, lane, **kwargs):
        changes, originals = self.plan(lane, **kwargs)
        cli.apply_changes(changes, originals)
        return changes

    def errors(self):
        return '\n'.join(map(str, board.validate_board(self.root, self.config)[1]))

    def test_public_validate_queue_and_complete_lifecycle(self):
        source = self.card(lane='ready', extra='milestone: Later\ncompleted: 2026-09-05\n')
        self.assertTrue(self.public('validate')['valid'])
        self.assertEqual(self.public('queue', '--rfc', 'RFC-XYZ-0001')['next'], 'XYZ-0001')
        self.public('move', 'XYZ-0001', '--to', 'working')
        self.assertFalse(source.exists())
        self.public('move', 'XYZ-0001', '--to', 'parked', '--parked-on', 'awaiting-person')
        self.public('move', 'XYZ-0001', '--to', 'ready')
        self.assertNotIn('parked_on', source.read_text())
        self.public('move', 'XYZ-0001', '--to', 'working')
        self.public('move', 'XYZ-0001', '--to', 'done')
        result = self.public('move', 'XYZ-0001', '--to', 'done')
        self.assertEqual(result['changed_paths'], [])
        self.public('move', 'XYZ-0001', '--to', 'ready', expected=1)
        self.assertEqual(self.public('queue', '--rfc', 'RFC-XYZ-0001')['cards'], [])
        self.assertEqual(len(self.public('queue', '--rfc', 'RFC-XYZ-0001', '--all')['cards']), 1)

    def test_schema_rejects_missing_unknown_duplicate_and_wrong_types(self):
        source = self.card()
        original = source.read_text()
        mutations = [('title: Fixture\n', '', 'title'),
                     ('status: Backlog', 'status: Backlog\nstatus: Done', 'duplicate front-matter'),
                     ('title: Fixture', 'title: [Fixture]', 'expected string'),
                     ('depends_on: []', 'depends_on: XYZ-0002', 'expected array'),
                     ('title: Fixture', 'title: Fixture\ntyop: value', 'unknown card field'),
                     ('spec: RFC-XYZ-0001', 'spec: gibberish', 'invalid value format'),
                     ('depends_on: []', 'depends_on: [XYZ-0002, XYZ-0002]', 'duplicate dependency')]
        for before, after, reason in mutations:
            with self.subTest(reason=reason):
                source.write_text(original.replace(before, after))
                self.assertIn(reason, self.errors())
                self.public('move', 'XYZ-0001', '--to', 'ready', expected=1)
                self.assertEqual(source.read_text(), original.replace(before, after))

    def test_parser_rejects_multiline_and_ambiguous_yaml(self):
        for content in ('title: A: B', 'title: >\n  text', 'title: test # comment',
                        'title: "unterminated', 'depends_on:\n  - XYZ-0001'):
            with self.subTest(content=content), self.assertRaises(board.BoardError):
                board.parse_matter(('---\n' + content + '\n---\n').encode(), Path('card'))

    def test_no_rfc_and_quoted_fields_are_supported(self):
        path = self.card()
        path.write_text(path.read_text().replace('spec: RFC-XYZ-0001', 'spec: none')
                        .replace('title: Fixture', 'title: "Colon: title"'))
        self.assertEqual(self.errors(), '')
        self.apply('ready')
        self.assertIn('title: "Colon: title"', (path.parent.parent / 'ready' / path.name).read_text())

    def test_identity_missing_rfc_and_cycles(self):
        source = self.card(deps=('XYZ-0002',))
        self.card(2, deps=('XYZ-0001',))
        self.assertIn('cycle', self.errors())
        source.write_text(source.read_text().replace('XYZ-0002]', 'XYZ-0009]'))
        self.assertIn('not found', self.errors())
        source.write_text(source.read_text().replace('RFC-XYZ-0001', 'RFC-XYZ-0009'))
        self.assertIn('has no RFC file', self.errors())
        source.rename(source.with_name('XYZ-0008-wrong.md'))
        self.assertIn('filename/id mismatch', self.errors())

    def test_duplicate_ids_refuse_move(self):
        self.card()
        self.card(lane='ready')
        self.assertIn('duplicate card id', self.errors())
        with self.assertRaises(board.BoardError):
            self.plan('working')

    def test_dependency_and_reason_rules(self):
        source = self.card(deps=('XYZ-0002',))
        self.card(2)
        for lane in ('ready', 'working', 'parked', 'done'):
            with self.subTest(lane=lane), self.assertRaises(board.BoardError):
                self.plan(lane, parked_on='awaiting-person' if lane == 'parked' else None)
        self.apply('win-ready')
        self.apply('blocked', blocked_by='missing-design')
        with self.assertRaises(board.BoardError):
            self.plan('blocked', blocked_by='platform-win')
        self.apply('backlog')
        self.assertNotIn('blocked_by', source.read_text())
        self.assertEqual(self.errors(), '')

    def test_parked_requires_known_reason(self):
        self.card()
        for reason in (None, 'unknown', 'bad\nstatus: Done'):
            with self.subTest(reason=reason), self.assertRaises(board.BoardError):
                self.plan('parked', parked_on=reason)
        self.apply('parked', parked_on='awaiting-person')

    def test_queue_platform_backlog_and_explicit_extra_ids(self):
        self.card(1, lane='win-ready', deps=('XYZ-0004',))
        self.card(2, lane='ready')
        self.card(3, lane='mac-ready')
        self.card(4)
        extra = self.card(5, lane='ready')
        extra.write_text(extra.read_text().replace('spec: RFC-XYZ-0001', 'spec: none'))
        result = self.public('queue', '--rfc', 'RFC-XYZ-0001', '--platform', 'mac', '--include', 'XYZ-0005')
        self.assertEqual(result['next'], 'XYZ-0002')
        rows = {r['id']: r for r in result['cards']}
        self.assertEqual(rows['XYZ-0001']['disposition'], 'deferred_platform')
        self.assertEqual(rows['XYZ-0001']['unmet_dependencies'], ['XYZ-0004'])
        self.assertEqual(rows['XYZ-0004']['disposition'], 'backlog_candidate')
        self.assertIn('XYZ-0005', rows)
        self.public('queue', '--rfc', 'RFC-XYZ-0001', '--include', 'XYZ-9999', expected=1)

    def test_queue_ambiguity_and_closeout(self):
        self.card(1, lane='working')
        self.card(2, lane='working', extra='closeout: true\n')
        result = self.public('queue', '--rfc', 'RFC-XYZ-0001')
        self.assertIsNone(result['next'])
        self.assertEqual(result['ambiguous_in_progress'], ['XYZ-0001', 'XYZ-0002'])
        self.assertEqual(len(result['closeout_paths']), 1)

    def test_dry_run_has_no_filesystem_or_git_effect(self):
        self.card(lane='ready')
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        result = self.public('move', 'XYZ-0001', '--to', 'working', '--dry-run')
        self.assertEqual(len(result['changed_paths']), 2)
        after = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        self.assertEqual(before, after)

    def test_crlf_body_metadata_and_live_links_preserved(self):
        source = self.card(lane='ready', body='# Body\n[peer](XYZ-0002-fixture.md#section)\n')
        self.card(2, lane='ready')
        source.write_bytes(source.read_bytes().replace(b'\n', b'\r\n'))
        readme = self.root / 'README.md'
        target = 'tracker/cards/ready/XYZ-0001-fixture.md'
        readme.write_text(f'[card]({target}#section "Title")\n[ref]: <{target}#section>\n'
                          f'`[code]({target})`\n```md\n[code]({target})\n```\n'
                          f'[[XYZ-0001]]\nhttps://example.invalid/{target}\n')
        original = source.read_bytes()
        self.public('move', 'XYZ-0001', '--to', 'working')
        dest = source.parent.parent / 'working' / source.name
        expected = original.replace(b'status: Ready', b'status: In Progress').replace(
            b'(XYZ-0002-fixture.md#section)', b'(../ready/XYZ-0002-fixture.md#section)')
        self.assertEqual(dest.read_bytes(), expected)
        content = readme.read_text()
        self.assertIn('[card](tracker/cards/working/XYZ-0001-fixture.md#section "Title")', content)
        self.assertIn('[ref]: <tracker/cards/working/XYZ-0001-fixture.md#section>', content)
        self.assertIn(f'`[code]({target})`', content)
        self.assertIn(f'```md\n[code]({target})\n```', content)
        self.assertIn('[[XYZ-0001]]', content)

    def test_protected_link_refuses_but_live_import_is_editable(self):
        self.card(lane='ready')
        directory = self.root / 'tracker/captured'
        directory.mkdir()
        path = directory / 'evidence.md'
        path.write_text('[card](../cards/ready/XYZ-0001-fixture.md)\n')
        with self.assertRaisesRegex(board.BoardError, 'protected'):
            self.plan('working')
        live = path.with_name('pack.LIVE_IMPORT.md')
        path.rename(live)
        self.apply('working')
        self.assertIn('../cards/working/', live.read_text())

    def test_symlink_and_destination_collision_refused(self):
        source = self.card(lane='ready')
        dest = source.parent.parent / 'working' / source.name
        dest.symlink_to(source)
        with self.assertRaisesRegex(board.BoardError, 'symlink'):
            self.plan('working')
        dest.unlink()
        dest.write_bytes(source.read_bytes())
        with self.assertRaises(board.BoardError):
            self.plan('working')

    def test_lock_refuses_concurrent_move(self):
        self.card(lane='ready')
        with cli.board_lock(self.root):
            result = self.public('move', 'XYZ-0001', '--to', 'working', expected=1)
            self.assertIn('another move is active', result['errors'][0]['reason'])
        self.public('move', 'XYZ-0001', '--to', 'working')
        self.assertFalse((self.root / '.git/implement-rfc-board.lock').exists())

    def test_stale_preflight_preserves_new_edit(self):
        source = self.card(lane='ready')
        changes, originals = self.plan('working')
        source.write_text(source.read_text() + '\nUser edit\n')
        with self.assertRaisesRegex(board.BoardError, 'changed since preflight'):
            cli.apply_changes(changes, originals)
        self.assertTrue(source.read_text().endswith('User edit\n'))

    def test_write_failure_restores_card_links_and_removes_new_lane(self):
        source = self.card(lane='ready')
        (source.parent.parent / 'working').rmdir()
        readme = self.root / 'README.md'
        readme.write_text('[card](tracker/cards/ready/XYZ-0001-fixture.md)\n')
        changes, originals = self.plan('working')
        actual_write = cli.write_file
        calls = 0
        def fail_second(*args, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError('injected write failure')
            return actual_write(*args, **kwargs)
        with patch.object(cli, 'write_file', side_effect=fail_second), self.assertRaisesRegex(OSError, 'injected'):
            cli.apply_changes(changes, originals)
        for path, data in originals.items():
            self.assertEqual(path.read_bytes() if path.exists() else None, data)
        self.assertFalse((source.parent.parent / 'working').exists())
        self.assertFalse(list(self.root.rglob('.issue-board-*')))

    def test_delete_failure_rolls_back_all_written_files(self):
        source = self.card(lane='ready')
        changes, originals = self.plan('working')
        actual_unlink = Path.unlink
        def fail_source(path, *args, **kwargs):
            if path == source:
                raise OSError('injected unlink failure')
            return actual_unlink(path, *args, **kwargs)
        with patch.object(Path, 'unlink', fail_source), self.assertRaisesRegex(OSError, 'injected'):
            cli.apply_changes(changes, originals)
        self.assertEqual(source.read_bytes(), originals[source])
        self.assertFalse((source.parent.parent / 'working' / source.name).exists())

    def test_configuration_rejects_unsafe_paths_and_rules(self):
        for key, value in [('link_roots', ['../escape']), ('deferring_lanes', ['unknown']),
                           ('parked_on', ['bad token']), ('protected_globs', ['/absolute/**'])]:
            config = copy.deepcopy(self.config)
            config['bookkeeping'][key] = value
            with self.subTest(key=key), self.assertRaises(project_config.ProjectConfigError):
                project_config.validate(config)

    def test_schema_file_is_executed_not_only_documentation(self):
        self.card()
        schema = json.loads(board.SCHEMA_PATH.read_text())
        schema['properties']['title']['pattern'] = '^Required title$'
        path = self.root / 'candidate.schema.json'
        path.write_text(json.dumps(schema))
        with patch.object(board, 'SCHEMA_PATH', path):
            self.assertIn('title: invalid value format', self.errors())

    def test_custom_closeout_marker_and_rfc_field_collision(self):
        self.config['closeout_card'] = {'front_matter': {'final_check': 'yes'}}
        self.card(extra='final_check: yes\n')
        self.assertEqual(self.errors(), '')
        result = cli.queue(self.root, self.config, 'RFC-XYZ-0001', 'mac', [], False)
        self.assertEqual(len(result['closeout_paths']), 1)
        self.config['rfc_link_field'] = 'depends_on'
        with self.assertRaises(project_config.ProjectConfigError):
            project_config.validate(self.config)

    def test_code_examples_and_historical_strings_are_never_rewritten(self):
        source = self.card(lane='ready')
        dest = source.parent.parent / 'working' / source.name
        path = self.root / 'README.md'
        target = 'tracker/cards/ready/XYZ-0001-fixture.md'
        examples = [f'    [card]({target})\n', f'~~~md\n[card]({target})\n~~~~\n',
                    f'````md\n```\n[card]({target})\n```\n````\n',
                    f'`old path: {target}`\n', f'```md\n[card]({target})\n',
                    f'``[card]({target}) ` code``\n']
        for text in examples:
            with self.subTest(text=text):
                self.assertEqual(cli.rewrite_links(text.encode(), path, path, source, dest, self.root), text.encode())

    def test_url_encoding_root_relative_links_and_self_links(self):
        source = self.card(lane='ready', body='[self](XYZ-0001-fixture.md#here)\n')
        path = self.root / 'README.md'
        path.write_text('[card](/tracker/cards/ready/XYZ-0001-fixture.md?view=1#here)\n'
                        '[encoded](tracker/cards/ready/XYZ%2D0001-fixture.md)\n')
        self.apply('working')
        self.assertIn('/tracker/cards/working/XYZ-0001-fixture.md?view=1#here', path.read_text())
        self.assertIn('[encoded](tracker/cards/working/XYZ-0001-fixture.md)', path.read_text())
        dest = source.parent.parent / 'working' / source.name
        self.assertIn('[self](XYZ-0001-fixture.md#here)', dest.read_text())

    def test_moving_card_preserves_file_mode(self):
        source = self.card(lane='ready')
        source.chmod(0o600)
        self.apply('working')
        dest = source.parent.parent / 'working' / source.name
        self.assertEqual(dest.stat().st_mode & 0o777, 0o600)

    def test_move_does_not_stage_anything(self):
        self.card(lane='ready')
        self.public('move', 'XYZ-0001', '--to', 'working')
        staged = subprocess.check_output(['git', '-C', str(self.root), 'diff', '--cached', '--name-only'])
        self.assertEqual(staged, b'')

    def test_repeated_move_does_not_normalize_links(self):
        self.card(lane='ready')
        path = self.root / 'README.md'
        original = b'[card](./tracker/cards/ready/XYZ%2D0001-fixture.md)\n'
        path.write_bytes(original)
        self.assertEqual(self.public('move', 'XYZ-0001', '--to', 'ready')['changed_paths'], [])
        self.assertEqual(path.read_bytes(), original)

    def test_error_after_publication_still_rolls_back(self):
        source = self.card(lane='ready')
        changes, originals = self.plan('working')
        actual_write = cli.write_file
        calls = 0
        def fail_after_publication(*args, **kwargs):
            nonlocal calls
            calls += 1
            actual_write(*args, **kwargs)
            if calls == 1:
                raise OSError('injected after publication')
        with patch.object(cli, 'write_file', side_effect=fail_after_publication), self.assertRaises(OSError):
            cli.apply_changes(changes, originals)
        self.assertEqual(source.read_bytes(), originals[source])
        self.assertFalse((source.parent.parent / 'working' / source.name).exists())

    def test_rollback_preserves_external_edit_and_reports_incomplete(self):
        self.card(lane='ready')
        readme = self.root / 'README.md'
        readme.write_text('[card](tracker/cards/ready/XYZ-0001-fixture.md)\n')
        changes, originals = self.plan('working')
        actual_write = cli.write_file
        calls = 0
        def fail_with_external_edit(*args, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2:
                readme.write_text('External edit\n')
                raise OSError('injected failure')
            return actual_write(*args, **kwargs)
        with patch.object(cli, 'write_file', side_effect=fail_with_external_edit), self.assertRaisesRegex(board.BoardError, 'incomplete rollback'):
            cli.apply_changes(changes, originals)
        self.assertEqual(readme.read_text(), 'External edit\n')


if __name__ == '__main__':
    unittest.main()
