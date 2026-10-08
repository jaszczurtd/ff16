#!/usr/bin/env python3
"""Compile and run regression cases with writable and read-only FatFs."""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
# Test source and whether it needs the writable configuration.
TESTS = (("test_mount.c", False), ("test_lseek.c", True))


def main():
    with tempfile.TemporaryDirectory(prefix="ff16 tests ") as directory:
        work = Path(directory)
        for readonly in (0, 1):
            config = (ROOT / "source" / "ffconf.h").read_text()
            config = re.sub(r"(#define FF_FS_READONLY\s+)0", rf"\g<1>{readonly}", config)
            (work / "ffconf.h").write_text(config)
            for compiler, flags in (
                ("gcc", ["-O2"]),
                ("clang", ["-O1", "-g", "-fsanitize=address,undefined",
                           "-fno-omit-frame-pointer"]),
            ):
                if not shutil.which(compiler):
                    raise SystemExit(f"Required compiler missing: {compiler}")
                for source, writable in TESTS:
                    if writable and readonly:
                        continue
                    binary = work / f"{Path(source).stem} binary"
                    subprocess.run([
                        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                        "-include", str(work / "ffconf.h"), "-I", str(ROOT / "source"),
                        str(ROOT / "source" / "ff.c"), str(ROOT / "tests" / source),
                        "-o", str(binary),
                    ], check=True)
                    print(f"{source}, {compiler}, FF_FS_READONLY={readonly}", flush=True)
                    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
