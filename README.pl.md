# FatFs R0.16

Fork oparty na [archiwum FatFs R0.16 autora ChaN](https://elm-chan.org/fsw/ff/arc/ff16.zip).
Poprawia dwie podatności:

- Montowanie odrzuca geometrię woluminu FAT, przy której rozmiar obszaru
  systemowego się przepełnia (CVE-2026-6682).
- Gdy `f_lseek()` rozszerza plik otwarty do zapisu, wypełnia lukę zerami,
  więc plik nie odsłania danych pozostawionych w ponownie przydzielonych
  klastrach (CVE-2026-6686). Każdy nowy sektor jest wtedy zapisywany, więc
  duże rozszerzenie trwa dłużej.

Testy regresji uruchomisz poleceniem `python3 tests/run_tests.py`.
Wymagają GCC i Clang; korzystają z dysków w pamięci, bez sprzętu.

[English](README.md).
