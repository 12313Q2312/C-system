import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROGRAM = Path(sys.argv.pop(1)).resolve()


class RecordsTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.data = Path(directory.name) / "records.tsv"

    def run_cli(self, *args, code=0):
        result = subprocess.run(
            [str(PROGRAM), str(self.data), *args], capture_output=True, text=True,
            encoding="utf-8", timeout=10,
        )
        self.assertEqual(result.returncode, code, result.stderr)
        return result.stdout

    def test_record_lifecycle_across_processes(self):
        self.assertEqual(self.run_cli("list"), "")
        self.run_cli("add", "101", "Motor bracket")
        self.run_cli("add", "102", "Bearing seat")
        self.assertEqual(self.run_cli("get", "101"), "101\tMotor bracket\n")
        self.run_cli("update", "101", "Motor bracket v2")
        self.assertEqual(self.run_cli("list"), "101\tMotor bracket v2\n102\tBearing seat\n")
        self.run_cli("remove", "102")
        self.assertEqual(self.data.read_text(), "101\tMotor bracket v2\n")
        self.run_cli("get", "102", code=1)

    def test_invalid_input_leaves_data_intact(self):
        self.run_cli("add", "1", "Fixture")
        original = self.data.read_bytes()
        self.run_cli("add", "1", "Duplicate", code=1)
        self.run_cli("update", "2", "Unknown record", code=1)
        self.run_cli("remove", "2", code=1)
        self.run_cli("add", "-1", "Invalid ID", code=2)
        self.run_cli("add", "2", "  ", code=2)
        self.run_cli("add", "2", "Invalid\ttitle", code=2)
        self.run_cli("add", "2", "x" * 121, code=2)
        self.assertEqual(self.data.read_bytes(), original)

    def test_invalid_file_is_never_replaced(self):
        original = b"2\tFixture\n2\tDuplicate\n"
        self.data.write_bytes(original)
        self.run_cli("add", "3", "Bracket", code=1)
        self.assertEqual(self.data.read_bytes(), original)

    def test_writer_respects_an_existing_temporary_file(self):
        temporary = self.data.with_suffix(".tsv.tmp")
        temporary.write_bytes(b"other writer")
        self.run_cli("add", "1", "Fixture", code=1)
        self.assertEqual(temporary.read_bytes(), b"other writer")
        self.assertFalse(self.data.exists())


if __name__ == "__main__":
    unittest.main()
