import contextlib
import io
import json
from pathlib import Path
import runpy
import subprocess
import tempfile
import unittest
from unittest import mock


API = runpy.run_path(str(Path(__file__).resolve().parents[1] / "benchmark"))
HISTORY = API["HISTORY"]
CACHE = API["CACHE"]


class BenchmarkTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Benchmark test")
        self.git("config", "user.email", "test@example.invalid")
        files = {
            ".gitignore": ".profile/\n/calls\n",
            "ucc/main.c": "100\n",
            "ucc/Makefile": "# build\n",
            "uas/main.c": "// assembler\n",
            "cpu/cpu.sv": "// cpu\n",
            "cpu/verilator/sim_main.cpp": "// simulator\n",
            "ucc/benchmarks/fib.c": "// fib\n",
            "ucc/benchmarks/printf.h": "// header\n",
            "ucc/benchmarks/targets.json": '[{"source":"fib.c","function":"fib"}]\n',
            "ucc/benchmark": "# benchmark tool\n",
            "README.md": "Documentation\n",
        }
        for name, content in files.items():
            self.write(name, content)
        self.write("ucc/profile", """#!/usr/bin/python3
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
with (root / 'calls').open('a') as f:
    f.write('run\\n')
value = int((root / 'ucc/main.c').read_text())
print('progress: 123 cycles', file=sys.stderr, flush=True)
print('profile_function: ' + sys.argv[-1], file=sys.stderr)
print('profile_status: ' + ('incomplete' if value < 0 else 'complete'), file=sys.stderr)
print('instructions: ' + str(abs(value)), file=sys.stderr)
print('clock_cycles: ' + str(abs(value) * 4), file=sys.stderr)
print('00')
sys.exit(2 if value < 0 else 0)
""")
        (self.root / "ucc/profile").chmod(0o755)
        self.commit("Initial")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], text=True).strip()

    def commit(self, message):
        self.git("add", ".")
        self.git("-c", "commit.gpgsign=false", "commit", "-qm", message)
        return self.git("rev-parse", "HEAD")

    def record(self, **kwargs):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            API["record"](self.root, **kwargs)
        return out.getvalue(), err.getvalue()

    def history(self):
        return json.loads((self.root / HISTORY).read_text())

    def calls(self):
        return (self.root / "calls").read_text().splitlines()

    def capture(self, function, name, **kwargs):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            API[function](self.root, name, **kwargs)
        return output.getvalue()

    def test_initial_record_and_unchanged_worktree_skip(self):
        _, progress = self.record()
        self.assertIn("progress: 123 cycles", progress)
        self.assertEqual(set(self.history()), {"HEAD"})
        self.assertEqual(self.history()["HEAD"]["results"][0]["instructions"], 100)
        path = self.root / HISTORY
        original = path.read_bytes(), path.stat().st_mtime_ns
        self.assertIn("Skipped", self.record()[0])
        self.assertEqual(len(self.calls()), 1)
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), original)

    def test_cold_cache_skip_across_unrelated_commit(self):
        self.record()
        measured = self.commit("Measured")
        self.write("README.md", "Updated docs\n")
        requested = self.commit("Docs only")
        (self.root / CACHE).unlink()
        before = (self.root / HISTORY).read_bytes()
        self.assertIn("Skipped", self.record()[0])
        self.assertEqual(len(self.calls()), 1)
        self.assertEqual((self.root / HISTORY).read_bytes(), before)
        output = self.capture("show", requested)
        self.assertIn("Requested: " + requested, output)
        self.assertIn("Measured:  " + measured, output)

    def test_relevant_changes_and_promote_actual_measurement_commit(self):
        self.record()
        first = self.commit("Baseline")
        self.write("README.md", "Docs\n")
        unrelated = self.commit("Unrelated")
        self.write("ucc/main.c", "80\n")
        output, _ = self.record()
        self.assertIn("-20", output)
        self.assertIn("-80", output)
        self.assertEqual(self.history()[first][0]["instructions"], 100)
        self.assertNotIn(unrelated, self.history())
        self.assertEqual(self.history()["HEAD"]["results"][0]["instructions"], 80)
        self.assertIn("Skipped", self.record()[0])
        self.assertEqual(len(self.calls()), 2)
        # Pending measurements must not be attributed to a committed revision.
        self.assertEqual(API["nearest"](self.root, unrelated)[1][0]["instructions"], 100)
        second = self.commit("Optimization")
        self.assertEqual(API["nearest"](self.root, second)[1][0]["instructions"], 80)

    def test_input_categories_and_ignored_docs_and_tests(self):
        self.record()
        self.commit("Baseline")
        relevant = ["ucc/main.c", "ucc/Makefile", "uas/main.c", "cpu/cpu.sv",
                    "cpu/verilator/sim_main.cpp", "ucc/benchmarks/fib.c", "ucc/benchmarks/printf.h"]
        for name in relevant:
            with self.subTest(name=name):
                before = len(self.calls())
                original = (self.root / name).read_text()
                self.write(name, "101\n" if name == "ucc/main.c" else original + "// changed\n")
                self.record()
                self.assertEqual(len(self.calls()), before + 1)
        for name in ("ucc/benchmarks/PROFILE.md", "cpu/verilator/test_profile.py", "cpu/cpu_tb.sv"):
            self.write(name, "Docs or tests\n")
        self.git("add", ".")
        self.assertIn("Skipped", self.record()[0])

    def test_target_list_change_and_multiple_functions(self):
        self.record()
        self.write("ucc/benchmarks/targets.json", json.dumps([
            {"source": "fib.c", "function": "fib"},
            {"source": "fib.c", "function": "buntan_main"},
        ]))
        self.record()
        self.assertEqual(len(self.calls()), 3)
        self.assertEqual(len(self.history()["HEAD"]["results"]), 2)

    def test_failure_does_not_modify_history_or_cache(self):
        self.record()
        history = (self.root / HISTORY).read_bytes()
        cache = (self.root / CACHE).read_bytes()
        self.write("ucc/main.c", "-1\n")
        with self.assertRaisesRegex(ValueError, "Benchmark failed"):
            self.record()
        self.assertEqual((self.root / HISTORY).read_bytes(), history)
        self.assertEqual((self.root / CACHE).read_bytes(), cache)

    def test_input_changed_during_measurement_preserves_history(self):
        self.record()
        original = (self.root / HISTORY).read_bytes()
        original_measure = API["measure"]

        def changing_measure(root, target, limit):
            result = original_measure(root, target, limit)
            self.write("ucc/benchmarks/printf.h", "// edited while measuring\n")
            return result

        with mock.patch.dict(API["record"].__globals__, measure=changing_measure):
            with self.assertRaisesRegex(ValueError, "changed during measurement"):
                self.record(force=True)
        self.assertEqual((self.root / HISTORY).read_bytes(), original)

    def test_second_target_failure_preserves_entire_previous_run(self):
        self.record()
        original = (self.root / HISTORY).read_bytes()
        self.write("ucc/benchmarks/targets.json", json.dumps([
            {"source": "fib.c", "function": "fib"},
            {"source": "fib.c", "function": "buntan_main"},
        ]))
        original_measure = API["measure"]

        def failing_measure(root, target, limit):
            if target["function"] == "buntan_main":
                raise ValueError("second target failed")
            return original_measure(root, target, limit)

        with mock.patch.dict(API["record"].__globals__, measure=failing_measure):
            with self.assertRaisesRegex(ValueError, "second target failed"):
                self.record()
        self.assertEqual((self.root / HISTORY).read_bytes(), original)

    def test_new_untracked_sources_and_deletions_invalidate_cache(self):
        self.record()
        self.write("ucc/benchmarks/new.h", "// newly added header\n")
        self.record()
        self.assertEqual(len(self.calls()), 2)
        (self.root / "ucc/benchmarks/printf.h").unlink()
        self.record()
        self.assertEqual(len(self.calls()), 3)

    def test_timestamp_only_change_does_not_measure(self):
        self.record()
        (self.root / "ucc/main.c").touch()
        self.assertIn("Skipped", self.record()[0])
        self.assertEqual(len(self.calls()), 1)

    def test_force_and_missing_cache(self):
        self.record()
        self.record(force=True)
        self.assertEqual(len(self.calls()), 2)
        self.commit("Measured")
        (self.root / CACHE).unlink()
        self.assertIn("Skipped", self.record()[0])

    def test_chronological_log_includes_inherited_results_and_deltas(self):
        self.record()
        first = self.commit("Baseline")
        self.write("README.md", "Changed docs\n")
        middle = self.commit("Docs only")
        self.write("ucc/main.c", "75\n")
        self.record()
        last = self.commit("Optimization")
        output = self.capture("log", "HEAD")
        self.assertLess(output.index(first[:12]), output.index(last[:12]))
        self.assertNotIn(middle[:12], output)
        self.assertIn("… (1 commits skipped)", output)
        self.assertIn("* Optimization", output)
        self.assertIn("-25", output)
        self.assertIn("-100", output)
        self.assertNotIn(last[:12], self.capture("log", middle))
        full = self.capture("log", "HEAD", all_commits=True)
        self.assertLess(full.index(first[:12]), full.index(middle[:12]))
        self.assertLess(full.index(middle[:12]), full.index(last[:12]))
        self.assertIn("= Docs only", full)
        self.assertNotIn("commits skipped", full)

    def test_identical_counts_still_record_a_measurement_commit(self):
        self.record()
        self.commit("Baseline")
        self.write("uas/main.c", "// assembler change with identical counts\n")
        self.record()
        last = self.commit("Same counts")
        self.assertEqual(API["nearest"](self.root, last)[0], last)
        self.assertNotIn(last[:12], self.capture("log", "HEAD"))
        self.assertIn("… (1 commits skipped)", self.capture("log", "HEAD"))
        self.assertIn("* Same counts", self.capture("log", "HEAD", all_commits=True))

    def test_changed_only_log_ignores_target_order(self):
        entries = [{"source": "fib.c", "function": "fib"},
                   {"source": "fib.c", "function": "buntan_main"}]
        self.write("ucc/benchmarks/targets.json", json.dumps(entries))
        self.record()
        self.commit("Baseline")
        self.write("ucc/benchmarks/targets.json", json.dumps(entries[::-1]))
        self.record()
        last = self.commit("Reorder targets")
        self.assertNotIn(last[:12], self.capture("log", "HEAD"))

    def test_lookup_does_not_use_results_from_another_branch(self):
        self.record()
        first = self.commit("Baseline")
        self.git("checkout", "-qb", "other")
        self.write("ucc/main.c", "60\n")
        self.record()
        self.commit("Other branch optimization")
        self.assertEqual(API["nearest"](self.root, first), (first, self.history()[first]))

    def test_invalid_targets_and_history_are_not_overwritten(self):
        self.record()
        original = (self.root / HISTORY).read_bytes()
        for entries in ([{"source": "../main.c", "function": "x"}], [],
                        [{"source": "fib.c", "function": "fib"}] * 2):
            self.write("ucc/benchmarks/targets.json", json.dumps(entries))
            with self.assertRaises(ValueError):
                self.record()
            self.assertEqual((self.root / HISTORY).read_bytes(), original)
        with self.assertRaises(ValueError):
            API["read_history"]('{"HEAD": []}')

    def test_no_results_and_invalid_revision(self):
        with self.assertRaisesRegex(ValueError, "No committed"):
            self.capture("show", "HEAD")
        with self.assertRaises(subprocess.CalledProcessError):
            self.capture("show", "missing-revision")


if __name__ == "__main__":
    unittest.main()
