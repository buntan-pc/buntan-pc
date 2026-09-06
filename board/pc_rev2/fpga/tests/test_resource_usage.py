import contextlib
import io
from pathlib import Path
import subprocess
import tempfile
import unittest

from resource_usage import parse_report, read_history, update_history


def report(logic=2552, registers=1182, cls=1663, logic_total=8640):
    return "<table>" + "".join(
        f'<tr><td class="label"><b>{name}</b></td><td>{used}/{total}</td></tr>'
        for name, used, total in (
            ("Logic", logic, logic_total), ("Register", registers, 6693), ("CLS", cls, 4320)
        )
    ) + "</table>"


class ResourceHistoryTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Resource test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("-c", "commit.gpgsign=false", "commit", "--allow-empty", "-qm", "Initial")
        project = self.root / "nested" / "fpga"
        project.mkdir(parents=True)
        self.report = project / "report.html"
        self.history = project / "resource-history.json"

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], text=True).strip()

    def build(self, **counts):
        self.report.write_text(report(**counts), encoding="utf-8")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            update_history(self.report, self.history)
        return read_history(self.history.read_text()), output.getvalue()

    def commit(self):
        self.git("add", str(self.history))
        self.git("-c", "commit.gpgsign=false", "commit", "-qm", "Resources")
        return self.git("rev-parse", "HEAD")

    def test_initial_and_repeated_uncommitted_builds(self):
        history, output = self.build()
        self.assertEqual(set(history), {"HEAD"})
        self.assertEqual(output.count("N/A"), 3)
        history, _ = self.build(logic=2600)
        self.assertEqual(set(history), {"HEAD"})
        self.assertEqual(history["HEAD"]["Logic"]["used"], 2600)

    def test_committed_baseline_survives_repeated_builds_and_next_commit(self):
        original, _ = self.build()
        first_commit = self.commit()
        history, output = self.build(logic=2562, registers=1180)
        self.assertEqual(history[first_commit], original["HEAD"])
        self.assertIn("+10", output)
        self.assertIn("-2", output)
        history, output = self.build(logic=2572)
        self.assertEqual(history[first_commit], original["HEAD"])
        self.assertIn("+20", output)
        second_commit = self.commit()
        history, output = self.build(logic=2575)
        self.assertEqual(history[first_commit], original["HEAD"])
        self.assertEqual(history[second_commit]["Logic"]["used"], 2572)
        self.assertIn("+3", output)
        self.assertEqual(set(history), {"HEAD", first_commit, second_commit})

    def test_unchanged_result_does_not_rewrite_history(self):
        self.build()
        self.commit()
        self.build()
        timestamp = self.history.stat().st_mtime_ns
        self.build()
        self.assertEqual(self.history.stat().st_mtime_ns, timestamp)

    def test_capacity_change_has_no_delta(self):
        self.build()
        self.commit()
        _, output = self.build(logic_total=9000)
        self.assertIn("N/A", next(line for line in output.splitlines() if line.startswith("Logic")))

    def test_bad_report_preserves_history(self):
        self.build()
        original = self.history.read_bytes()
        self.report.write_text("<table></table>")
        with self.assertRaises(ValueError):
            update_history(self.report, self.history)
        self.assertEqual(self.history.read_bytes(), original)

    def test_bad_history_is_not_overwritten(self):
        self.build()
        self.history.write_text('{"HEAD": {}}')
        with self.assertRaises(ValueError):
            self.build()
        self.assertEqual(self.history.read_text(), '{"HEAD": {}}')

    def test_parse_report(self):
        self.assertEqual(parse_report(report())["Register"], {"used": 1182, "total": 6693})
        with self.assertRaises(ValueError):
            parse_report(report(logic_total=0))


if __name__ == "__main__":
    unittest.main()
