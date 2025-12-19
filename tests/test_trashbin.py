from __future__ import annotations

from pathlib import Path


def _trash_dir(home: Path) -> Path:
    return home / ".trash"


def test_rm_trash_creates_symlink_in_trash(tmp_path: Path, test_home: Path, tool, run_cmd):
    work = tmp_path / "work"
    work.mkdir()

    f = work / "a.txt"
    f.write_text("hello", encoding="utf-8")
    assert f.exists()

    rm_trash = tool("rm_trash")
    res = run_cmd([rm_trash, f], env={"HOME": str(test_home)})
    assert res.returncode == 0, res.dump()

    assert not f.exists()

    tdir = _trash_dir(test_home)
    assert tdir.exists() and tdir.is_dir(), f"trash dir missing: {tdir}"

    link = tdir / f.name
    assert link.exists(), f"trash link missing: {link}"
    assert link.is_symlink(), f"expected symlink, got: {link}"

    assert link.resolve(strict=False) == f, f"symlink points to {link.resolve(strict=False)} not {f}"


def test_untrash_restores_file_after_confirm(tmp_path: Path, test_home: Path, tool, run_cmd):
    work = tmp_path / "work"
    work.mkdir()

    f = work / "b.txt"
    f.write_text("data", encoding="utf-8")
    assert f.exists()

    rm_trash = tool("rm_trash")
    res = run_cmd([rm_trash, f], env={"HOME": str(test_home)})
    assert res.returncode == 0, res.dump()
    assert not f.exists()

    link = _trash_dir(test_home) / f.name
    assert link.exists() and link.is_symlink(), f"missing trash symlink: {link}"

    untrash = tool("untrash")
    res2 = run_cmd([untrash, f.name], env={"HOME": str(test_home)}, input_text="y\n")
    assert res2.returncode == 0, res2.dump()

    assert f.exists()
    assert f.read_text(encoding="utf-8") == "data"

    assert not link.exists(), f"trash symlink still exists after untrash: {link}"


def test_untrash_decline_does_not_restore(tmp_path: Path, test_home: Path, tool, run_cmd):
    work = tmp_path / "work"
    work.mkdir()

    f = work / "c.txt"
    f.write_text("no", encoding="utf-8")

    rm_trash = tool("rm_trash")
    res = run_cmd([rm_trash, f], env={"HOME": str(test_home)})
    assert res.returncode == 0, res.dump()
    assert not f.exists()

    untrash = tool("untrash")
    res2 = run_cmd([untrash, f.name], env={"HOME": str(test_home)}, input_text="n\n")
    assert not f.exists(), "file restored even though confirmation was declined"


def test_rm_trash_missing_file_errors(tmp_path: Path, test_home: Path, tool, run_cmd):
    work = tmp_path / "work"
    work.mkdir()

    missing = work / "nope.txt"
    rm_trash = tool("rm_trash")
    res = run_cmd([rm_trash, missing], env={"HOME": str(test_home)})

    assert res.returncode != 0, res.dump()
