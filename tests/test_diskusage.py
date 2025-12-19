from __future__ import annotations

import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


# Helpers (parsing / asserts)
_HEADER_RE = re.compile(
    r"^=== Disk Report \|\s*(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\s*===$",
    re.MULTILINE,
)
_FILES_RE = re.compile(r"^Files:\s*(\d+)\s*$", re.MULTILINE)
_TOTAL_RE = re.compile(r"^Total:\s*(.+)\s*$", re.MULTILINE)


@dataclass(frozen=True)
class DiskusageReport:
    ts: datetime
    files: int
    total_str: str

def run_diskusage(run_cmd, tool, target: Path) -> str:
    """Run diskusage and return stdout (assert return code == 0)."""
    diskusage = tool("diskusage")
    res = run_cmd([diskusage, target])
    assert res.returncode == 0, res.dump()
    return res.stdout


def parse_report(stdout: str) -> DiskusageReport:
    """Parse minimal stable fields from stdout."""
    m = _HEADER_RE.search(stdout)
    assert m, f"header/timestamp not found in output:\n{stdout}"
    ts = datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S")

    m = _FILES_RE.search(stdout)
    assert m, f"Files: line not found in output:\n{stdout}"
    files = int(m.group(1))

    m = _TOTAL_RE.search(stdout)
    assert m, f"Total: line not found in output:\n{stdout}"
    total_str = m.group(1).strip()

    return DiskusageReport(ts=ts, files=files, total_str=total_str)


def assert_time_close_to_now(actual: datetime, tolerance_sec: int = 10):
    """Assert timestamp is close to now (tolerant to CI/docker delays)."""
    now = datetime.now()
    delta = abs((now - actual).total_seconds())
    assert delta <= tolerance_sec, (
        f"time delta too large: {delta:.2f}s\n"
        f"actual: {actual}\n"
        f"now: {now}"
    )

# Tests
def test_diskusage_format_has_required_sections(tmp_path: Path, tool, run_cmd):
    out = run_diskusage(run_cmd, tool, tmp_path)

    for marker in (
        "=== Disk Report |",
        "Files:",
        "Top-10:",
        "Logs:",
        "Temp:",
        "Hidden:",
        "Total:",
    ):
        assert marker in out, out


def test_diskusage_time_is_recent(tmp_path: Path, tool, run_cmd):
    out = run_diskusage(run_cmd, tool, tmp_path)
    report = parse_report(out)

    assert_time_close_to_now(report.ts, tolerance_sec=15)


def test_diskusage_empty_dir_counts_zero(tmp_path: Path, tool, run_cmd):
    out = run_diskusage(run_cmd, tool, tmp_path)
    report = parse_report(out)

    assert report.files == 0, out
    assert report.total_str == "0 B", out


def test_diskusage_counts_files_and_nonzero_total(tmp_path: Path, tool, run_cmd):
    (tmp_path / "a.txt").write_text("hello", encoding="utf-8")
    (tmp_path / "b.bin").write_bytes(b"\x00" * 10)

    out = run_diskusage(run_cmd, tool, tmp_path)
    report = parse_report(out)

    assert report.files >= 2, out

    assert report.total_str != "0 B", out


def test_diskusage_recursive_nonzero_total(tmp_path: Path, tool, run_cmd):
    sub = tmp_path / "sub"
    sub.mkdir()
    (sub / "x.txt").write_text("1234567890", encoding="utf-8")

    out = run_diskusage(run_cmd, tool, tmp_path)
    report = parse_report(out)

    assert report.total_str != "0 B", out


def test_diskusage_errors_on_missing_path(tmp_path: Path, tool, run_cmd):
    diskusage = tool("diskusage")
    missing = tmp_path / "no_such_dir"

    res = run_cmd([diskusage, missing])

    assert res.returncode != 0, res.dump()
