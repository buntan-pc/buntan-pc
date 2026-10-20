#!/usr/bin/env python3
"""End-to-end profiling tests using the actual CPU RTL and assembler."""
import pathlib
import contextlib
import io
import runpy
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SIM = ROOT / "cpu/verilator/obj_dir/Vsim_top"
AS = ROOT / "uas/as"
CC = ROOT / "ucc/cc"


class ProfileTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="buntan-profile-")
        self.addCleanup(self.temp.cleanup)
        self.work = pathlib.Path(self.temp.name)

    def assemble(self, body, setup="", calls="call subject"):
        source = f"""section .data
section .text
start:
{setup}
{calls}
st 6
fin:
jmp fin
{body}
"""
        (self.work / "app.s").write_text(source)
        self.run_tool([AS, "--pmem", "app.pmem.hex", "--dmem", "app.dmem.hex",
                       "--map", "app.map", "app.s"])

    def run_tool(self, command):
        result = subprocess.run(command, cwd=self.work, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result

    def simulate(self, function="subject", extra=()):
        command = [SIM, "+pmem=app.pmem.hex", "+dmem=app.dmem.hex", *extra]
        if function is not None:
            command += ["+map=app.map", f"+profile={function}"]
        result = subprocess.run(command, cwd=self.work, text=True,
                                capture_output=True, timeout=10)
        lines = [line for line in result.stderr.splitlines()
                 if line.startswith(("profile_function:", "profile_status:", "instructions:", "clock_cycles:", "hardware_time:"))]
        self.assertIn(len(lines), (0, 5))
        stats = dict((key, value.strip()) for key, value in (line.split(":", 1) for line in lines))
        if stats:
            duration, unit, clock = stats.pop("hardware_time").split()
            self.assertEqual(clock, "(clock=27MHz)")
            scale = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1}[unit]
            self.assertAlmostEqual(float(duration) * scale, int(stats["clock_cycles"]) / 27_000_000,
                                   delta=0.00051 * scale)
        return result, stats

    def assert_complete(self, count, clock_cycles=None, output="07\n", function="subject"):
        result, stats = self.simulate(function)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, output)
        self.assertEqual(stats, dict(profile_function=function, profile_status="complete",
                                    instructions=str(count), clock_cycles=str(clock_cycles or 4 * count)))

    def test_nop_load_store_and_branch(self):
        self.assemble("""subject:
nop
push 7
st 256
ld 256
push 0
jz taken
push 99
taken:
ret
""")
        self.assert_complete(7)
        result, stats = self.simulate(None)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "07\n")
        self.assertEqual(stats, {})

    def test_progress_preserves_results(self):
        self.assemble("subject:\npush 7\nret\n")
        plain, plain_stats = self.simulate(extra=("+max-cycles=1000",))
        progress, stats = self.simulate(extra=("+max-cycles=1000", "+progress"))
        self.assertEqual(progress.returncode, plain.returncode)
        self.assertEqual(progress.stdout, plain.stdout)
        self.assertEqual(stats, plain_stats)
        updates = [line for line in progress.stderr.splitlines() if line.startswith("progress: ")]
        self.assertEqual(updates, [])
        timeout, _ = self.simulate(extra=("+max-cycles=40", "+progress"))
        self.assertEqual(timeout.returncode, 2)
        self.assertNotIn("progress: ", timeout.stderr)

    def test_loop(self):
        self.assemble("""subject:
push 3
again:
dup
push 0
neq
jz done
push 1
sub
jmp again
done:
pop
push 7
ret
""")
        self.assert_complete(29)

    def test_recursion(self):
        self.assemble("""subject:
dup
push 0
neq
jz base
push 1
sub
call subject
ret
base:
pop
push 7
ret
""", setup="push 3")
        self.assert_complete(31)

    def test_indirect_nested_calls_and_first_invocation(self):
        self.assemble("""subject:
call helper
ret
helper:
push 7
ret
""", calls="push subject\ncall\npop\ncall subject")
        self.assert_complete(4)

    def test_fp_save_restore(self):
        self.assemble("""subject:
nop
call helper
nop
push 7
ret
helper:
ret
""")
        # The ISA supports CPUSH/ CPOP FP, but uas has no mnemonics for them.
        start = next(int(line.split()[0], 16)
                     for line in (self.work / "app.map").read_text().splitlines()
                     if len(line.split()) == 2 and line.split()[1] == "subject")
        image = self.work / "app.pmem.hex"
        words = image.read_text().split()
        words[start], words[start + 2] = "1c803", "1c802"
        image.write_text("\n".join(words) + "\n")
        self.assert_complete(6)

    def test_software_interrupt(self):
        self.assemble("""subject:
int
push 7
ret
handler:
nop
iret
""", setup="push handler\npop isr")
        self.assert_complete(5)

    def test_hardware_interrupt_suppresses_instruction(self):
        self.assemble("subject:\npush 1\nst 2\n" + "nop\n" * 100 + """push 7
ret
handler:
push 0
st 2
iret
""", setup="push handler\npop isr\npush 2\nst 4")
        # 104 subject instructions + 3 handler instructions. IRQ entry occupies
        # four cycles but replaces an instruction which must not be counted.
        self.assert_complete(107, clock_cycles=432)
        self.assert_complete(3, function="handler")

    def test_profile_at_reset_entry(self):
        self.assemble("subject:\npush 7\nret")
        # start has no caller and never returns: include its very first fetch.
        result, stats = self.simulate("start", extra=("+max-cycles=4",))
        self.assertEqual(result.returncode, 2)
        self.assertEqual(stats["profile_status"], "incomplete")
        self.assertEqual(stats["instructions"], "1")
        self.assertEqual(stats["clock_cycles"], "5")

    def test_not_entered(self):
        self.assemble("subject:\npush 7\nret", calls="push 7")
        result, stats = self.simulate()
        self.assertEqual(result.returncode, 0)
        self.assertEqual(stats["profile_status"], "not_entered")
        self.assertEqual(stats["instructions"], "0")
        self.assertEqual(stats["clock_cycles"], "0")

    def test_incomplete_on_timeout(self):
        self.assemble("subject:\njmp subject")
        result, stats = self.simulate(extra=("+max-cycles=40",))
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "timeout\n")
        self.assertEqual(stats["profile_status"], "incomplete")
        self.assertGreater(int(stats["instructions"]), 0)

    def test_incomplete_on_uart_exit(self):
        self.assemble("subject:\npush 7\nst 6\nwait:\njmp wait")
        result, stats = self.simulate()
        self.assertEqual(result.returncode, 0)
        self.assertEqual(stats["profile_status"], "incomplete")

    def test_map_errors(self):
        self.assemble("subject:\npush 7\nret")
        for contents in (None, "section .data\n0000 subject\n",
                         "section .text\n0003 subject\n0003 subject\n",
                         "section .text\n4000 subject\n",
                         "section .text\nxyz subject\n"):
            with self.subTest(contents=contents):
                path = self.work / "app.map"
                if contents is None:
                    path.unlink()
                else:
                    path.write_text(contents)
                result, stats = self.simulate()
                self.assertEqual(result.returncode, 1)
                self.assertIn("error:", result.stderr)
                self.assertEqual(stats, {})

    def test_profile_requires_map(self):
        result = subprocess.run(
            [SIM, "+pmem=unused", "+profile=subject"], text=True, capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("requires +map", result.stderr)

    def test_empty_function(self):
        result = subprocess.run([SIM, "+pmem=unused", "+profile="],
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("empty +profile", result.stderr)

    def test_fibonacci(self):
        self.run_tool([CC, "-o", "app.s", ROOT / "cpu/verilator/examples/fib.c"])
        self.run_tool([AS, "--pmem", "app.pmem.hex", "--dmem", "app.dmem.hex",
                       "--map", "app.map", "app.s"])
        result, outer = self.simulate("buntan_main")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "08\n")
        result, inner = self.simulate("fib")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "08\n")
        for stats in (outer, inner):
            self.assertEqual(stats["profile_status"], "complete")
            self.assertEqual(int(stats["clock_cycles"]), 4 * int(stats["instructions"]))
        self.assertGreater(int(outer["instructions"]), int(inner["instructions"]))
        self.assertEqual(self.simulate("fib")[1], inner)


class ProfileScriptTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="buntan profile ")
        self.addCleanup(self.temp.cleanup)
        self.work = pathlib.Path(self.temp.name)
        self.source = self.work / "test app.c"
        self.header = self.work / "value.h"
        self.source.write_text('#include "value.h"\nint buntan_main() { return value(); }\n')
        self.header.write_text("int value() { return 7; }\n")

    def profile(self, *arguments):
        return subprocess.run([ROOT / "ucc/profile", self.source, *arguments], cwd=self.work,
                              text=True, capture_output=True, timeout=30)

    def outputs(self):
        return [self.work / ".profile" / (self.source.stem + suffix)
                for suffix in (".s", ".pmem.hex", ".dmem.hex", ".map", ".d")]

    def snapshot(self):
        return [path.stat().st_mtime_ns for path in self.outputs()]

    def assert_success(self, result, output="07\n", function="buntan_main"):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, output)
        self.assertIn(f"profile_function: {function}\nprofile_status:   complete\n", result.stderr)

    def test_default_and_cached_build_with_function_change(self):
        result = self.profile()
        self.assert_success(result)
        self.assertEqual(result.stderr, "profile_function: buntan_main\nprofile_status:   complete\n"
                         "instructions:     4\nclock_cycles:     16\nhardware_time:    592.593 ns (clock=27MHz)\n")
        previous = self.snapshot()
        result = self.profile("value")
        self.assert_success(result, function="value")
        self.assertEqual(self.snapshot(), previous)
        self.assertFalse(self.source.with_suffix(".profile.json").exists())

    def test_verbose_shows_commands_and_output(self):
        result = self.profile("-v")
        self.assert_success(result)
        self.assertIn("profile: make ", result.stderr)
        self.assertIn("size of .text:", result.stderr)
        self.assertIn("profile: " + str(SIM), result.stderr)
        self.assertIn("+max-cycles=10000000", result.stderr)
        self.assertIn("+progress", result.stderr)

    def test_progress_lines_with_redirected_stderr(self):
        runner = runpy.run_path(str(ROOT / "ucc/profile"))["run_with_progress"]
        script = ("import sys; print('progress: 123 cycles', file=sys.stderr, flush=True); "
                  "print('profile_status:   complete', file=sys.stderr); print('07')")
        for verbose in (False, True):
            with self.subTest(verbose=verbose):
                stderr = io.StringIO()
                with contextlib.redirect_stderr(stderr):
                    result = runner([sys.executable, "-c", script], verbose, self.work)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(stderr.getvalue().splitlines().count("progress: 123 cycles"), 1)
                self.assertNotIn("\x1b", stderr.getvalue())
                self.assertNotIn("\r", stderr.getvalue())
                if not verbose:
                    self.assertEqual(result.stdout, "07\n")
                    self.assertEqual(stderr.getvalue(), "progress: 123 cycles\n")
                    self.assertNotIn("progress:", result.stderr)

    def test_cycle_limit(self):
        result = self.profile("--max-cycles", "1")
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("timeout: exceeded 1 cycles", result.stderr)
        self.assert_success(self.profile("--max-cycles", "1000"))

    def test_invalid_cycle_limit(self):
        for limit in ("0", "-1", "18446744073709551616", "abc"):
            with self.subTest(limit=limit):
                result = self.profile("--max-cycles", limit)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn("usage:", result.stderr)
                self.assertNotIn("profile: make", result.stderr)

    def test_source_and_header_changes_rebuild(self):
        self.assert_success(self.profile())
        previous = self.snapshot()
        self.header.write_text("int value() { return 8; }\n")
        self.assert_success(self.profile(), output="08\n")
        self.assertNotEqual(self.snapshot()[0], previous[0])
        previous = self.snapshot()
        self.source.write_text("int buntan_main() { return 9; }\n")
        self.assert_success(self.profile(), output="09\n")
        self.assertNotEqual(self.snapshot()[0], previous[0])

    def test_missing_image_reassembles_without_compiling(self):
        self.assert_success(self.profile())
        previous = self.snapshot()
        self.outputs()[3].unlink()
        self.assert_success(self.profile())
        self.assertEqual(self.snapshot()[0], previous[0])
        self.assertNotEqual(self.snapshot()[1:4], previous[1:4])

    def test_missing_dependencies_are_regenerated(self):
        self.assert_success(self.profile())
        self.outputs()[-1].unlink()
        self.header.write_text("int value() { return 8; }\n")
        self.assert_success(self.profile(), output="08\n")
        self.assertTrue(self.outputs()[-1].is_file())

    def test_updated_assembly_reassembles_without_compiling(self):
        self.assert_success(self.profile())
        assembly = self.outputs()[0]
        assembly.write_text(assembly.read_text() + "\n")
        previous = self.snapshot()
        self.assert_success(self.profile())
        self.assertEqual(self.snapshot()[0], previous[0])
        self.assertNotEqual(self.snapshot()[1:4], previous[1:4])

    def test_compile_failure_preserves_images_and_does_not_simulate(self):
        self.assert_success(self.profile())
        previous = self.snapshot()
        self.source.write_text("#unsupported\n")
        result = self.profile()
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("profile_function:", result.stderr)
        self.assertIn("profile: make ", result.stderr)
        self.assertIn("#unsupported", result.stderr)
        self.assertEqual([path.stat().st_mtime_ns for path in self.outputs()[1:4]], previous[1:4])
        self.source.write_text("int buntan_main() { return 7; }\n")
        self.assert_success(self.profile())

    def test_bad_arguments_and_simulator_error(self):
        result = subprocess.run([ROOT / "ucc/profile"], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("usage:", result.stderr)
        result = self.profile("unknown_function")
        self.assertEqual(result.returncode, 1)
        self.assertIn("profile function must appear exactly once", result.stderr)


if __name__ == "__main__":
    unittest.main()
