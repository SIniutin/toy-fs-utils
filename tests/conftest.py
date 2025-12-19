from __future__ import annotations

import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence, Optional, Mapping

import pytest


@dataclass(frozen=True)
class CmdResult:
    args: list[str]
    returncode: int
    stdout: str
    stderr: str

    def dump(self) -> str:
        return (
            f"\ncmd: {' '.join(self.args)}\n"
            f"returncode: {self.returncode}\n"
            f"stdout:\n{self.stdout}\n"
            f"stderr:\n{self.stderr}\n"
        )


@pytest.fixture
def bin_dir() -> Path:
    return Path("/app/build/bin")


@pytest.fixture
def tool(bin_dir: Path):
    def _tool(name: str) -> Path:
        return bin_dir / name
    return _tool

@pytest.fixture
def test_home(tmp_path: Path) -> Path:
    home = tmp_path / "home"
    home.mkdir(parents=True, exist_ok=True)
    return home

@pytest.fixture
def run_cmd():
    def _run(
        cmd: Sequence[str | Path],
        cwd: Optional[Path] = None,
        env: Optional[Mapping[str, str]] = None,
        input_text: Optional[str] = None,
    ) -> CmdResult:
        argv = [str(x) for x in cmd]
        base_env = os.environ.copy()
        if env:
            base_env.update({k: str(v) for k, v in env.items()})

        res = subprocess.run(
            argv,
            cwd=str(cwd) if cwd is not None else None,
            env=base_env,
            input=input_text,
            capture_output=True,
            text=True,
        )
        return CmdResult(argv, res.returncode, res.stdout, res.stderr)

    return _run
