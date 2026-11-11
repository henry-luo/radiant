import subprocess
import sys
import tempfile
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class LayoutCastLintTest(unittest.TestCase):
    def check_source(self, source, selector="^no-int-cast-radiant$"):
        (ROOT / "temp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="lint-contract-", dir=ROOT / "temp") as directory:
            root = Path(directory)
            (root / "radiant").mkdir()
            (root / "radiant/layout_block.cpp").write_text(source)
            return subprocess.run([sys.executable, str(ROOT / "scripts/lint.py"),
                                   "--root", str(root), "--rule", selector],
                                  capture_output=True, text=True)

    def test_rejects_unmarked_geometry_cast(self):
        result = self.check_source("int size(float width) { return (int)width; }\n")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("radiant/layout_block.cpp:1:", result.stdout)

    def test_accepts_documented_integer_offset(self):
        result = self.check_source("int count(long bytes) { return (int)bytes; } // INT_CAST_OK: byte count.\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 cast(s) checked, 0 unmarked", result.stdout)

    def test_requires_reason(self):
        result = self.check_source("int count(long bytes) { return (int)bytes; } // INT_CAST_OK:\n")
        self.assertEqual(result.returncode, 1, result.stderr)

    def test_requires_marker_on_cast_line(self):
        result = self.check_source("int count(long bytes) {\n// INT_CAST_OK: byte count.\nreturn (int)bytes;\n}\n")
        self.assertEqual(result.returncode, 1, result.stderr)

    def test_rejects_empty_rule_selection(self):
        result = self.check_source("", "^missing-rule$")
        self.assertEqual(result.returncode, 2)
        self.assertIn("no policy rules match", result.stderr)


if __name__ == "__main__":
    unittest.main()
