import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from verify_pack import verify, assert_dag, markdown_links, frontmatter

class PackTests(unittest.TestCase):
    def test_complete_archive(self):
        report=verify(ROOT)
        self.assertEqual(report['rfcs'],9)
        self.assertEqual(report['cards'],52)
        self.assertEqual(report['milestones'],11)
        self.assertEqual(report['acceptance_requirements'],90)
    def test_link_integrity(self):
        self.assertGreater(markdown_links(ROOT),150)
    def test_dag_cycle_refused(self):
        with self.assertRaises(ValueError):assert_dag({'a':['b'],'b':['a']},'test')
    def test_dag_unknown_refused(self):
        with self.assertRaises(ValueError):assert_dag({'a':['missing']},'test')
    def test_dag_valid(self):
        assert_dag({'a':[],'b':['a'],'c':['a','b']},'test')
    def test_all_cards_backlog(self):
        for p in (ROOT/'cards').glob('PACK-CYCLE-*.md'):
            self.assertEqual(frontmatter(p)['status'],'Backlog')

if __name__=='__main__':unittest.main()
