# FatFs R0.16

Based on [ChaN's FatFs R0.16 archive](https://elm-chan.org/fsw/ff/arc/ff16.zip).
This fork fixes two vulnerabilities:

- Mounting rejects FAT volume geometry that overflows the system area size
  (CVE-2026-6682).
- When `f_lseek()` extends a file opened for writing, it fills the gap with
  zeros, so the file never exposes data left in reused clusters
  (CVE-2026-6686). Every new sector is written, so a large extension takes
  longer.

Run the host regression tests with `python3 tests/run_tests.py`.
They require GCC and Clang and use in-memory disks, without hardware.

[Polish](README.pl.md).
