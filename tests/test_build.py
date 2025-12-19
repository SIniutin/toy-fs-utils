from pathlib import Path

def test_binaries_exist(bin_dir: Path):
    tools = ["diskusage", "fswatch", 
             "rm_trash", "untrash", "list_trash",
             "backup", "upback", "backup_list"]
    for t in tools:
        assert (bin_dir / t).exists(), f"missing binary: {t} in {bin_dir}"
