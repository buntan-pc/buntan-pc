"""Exercise the history workflow with real Git, compiler and CPU simulation."""
import contextlib
import io
import json
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile
import unittest


UCC = Path(__file__).resolve().parents[1]
API = runpy.run_path(str(UCC / "benchmark"))
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
        self.write(".gitignore", ".profile/\n")
        self.write("README.md", "Documentation\n")
        self.write("ucc/benchmarks/program.c", """int fib() {
  int sum = 0;
  for (int i = 0; i < 3; ++i) sum += i;
  return sum;
}
int buntan_main() { return fib(); }
""")
        self.set_targets("fib")
        # Delegate to the real tool, which builds and runs the real CPU RTL.
        self.write("ucc/profile", f'#!/bin/sh\nexec {shlex.quote(str(UCC / "profile"))} "$@"\n')
        (self.root / "ucc/profile").chmod(0o755)
        self.commit("Initial")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def set_targets(self, *functions):
        self.write(API["TARGETS"], json.dumps([
            {"source": "program.c", "function": function} for function in functions
        ]))

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], text=True).strip()

    def commit(self, message):
        self.git("add", ".")
        self.git("-c", "commit.gpgsign=false", "commit", "-qm", message)
        return self.git("rev-parse", "HEAD")

    def run_tool(self, function="record", *args, **kwargs):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            API[function](self.root, *args, **kwargs)
        return output.getvalue(), errors.getvalue()

    def history(self):
        return json.loads((self.root / HISTORY).read_text())

    def optimize(self):
        self.write("ucc/benchmarks/program.c",
                   "int fib() { return 3; }\nint buntan_main() { return fib(); }\n")

    def test_record_skip_and_remeasure(self):
        self.run_tool()
        original = self.history()["HEAD"]["results"]
        path = self.root / HISTORY
        before = path.read_bytes(), path.stat().st_mtime_ns
        output, errors = self.run_tool()
        self.assertIn("Skipped", output)
        self.assertEqual(errors, "")
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)

        measured = self.commit("Baseline")
        self.write("README.md", "Docs only\n")
        self.commit("Docs only")
        (self.root / CACHE).unlink()
        self.assertIn("Skipped", self.run_tool()[0])
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)

        self.optimize()
        self.run_tool()
        history = self.history()
        self.assertEqual(history[measured], original)
        self.assertLess(history["HEAD"]["results"][0]["instructions"], original[0]["instructions"])

    def test_lookup_and_changes_only_log(self):
        self.run_tool()
        first = self.commit("Baseline")
        source = self.root / "ucc/benchmarks/program.c"
        source.write_text(source.read_text() + "// Same generated code\n")
        self.run_tool()
        same = self.commit("Same counts")
        self.write("README.md", "Changed docs\n")
        docs = self.commit("Docs only")
        self.optimize()
        self.run_tool()
        # Pending measurements must not be attributed to committed revisions.
        output, _ = self.run_tool("show", docs)
        self.assertIn("Requested: " + docs, output)
        self.assertIn("Measured:  " + same, output)
        last = self.commit("Optimization")

        output, _ = self.run_tool("log", "HEAD")
        self.assertLess(output.index(first[:12]), output.index(last[:12]))
        self.assertNotIn(same[:12], output)
        self.assertNotIn(docs[:12], output)
        self.assertIn("… (2 commits skipped)", output)
        previous = self.history()[same][0]
        current = self.history()["HEAD"]["results"][0]
        self.assertIn(str(current["instructions"] - previous["instructions"]), output)
        full, _ = self.run_tool("log", "HEAD", all_commits=True)
        self.assertIn("* Same counts", full)
        self.assertIn("= Docs only", full)
        self.assertNotIn("commits skipped", full)

    def test_target_list_remeasures_multiple_functions(self):
        self.run_tool()
        self.set_targets("fib", "buntan_main")
        self.run_tool()
        results = self.history()["HEAD"]["results"]
        self.assertEqual([row["function"] for row in results], ["fib", "buntan_main"])
        self.assertGreater(results[1]["instructions"], results[0]["instructions"])

    def test_failed_target_preserves_history_and_cache(self):
        self.run_tool()
        before = {name: (self.root / name).read_bytes() for name in (HISTORY, CACHE)}
        self.set_targets("fib", "missing_function")
        with self.assertRaisesRegex(ValueError, "Benchmark failed"):
            self.run_tool()
        for name, contents in before.items():
            self.assertEqual((self.root / name).read_bytes(), contents)


if __name__ == "__main__":
    unittest.main()
