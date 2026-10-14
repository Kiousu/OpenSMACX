# Building OpenSMACX with mingw-w64 (Linux / cross-compile)

Verified 2026-10-14 on Debian 13 aarch64 with `g++-mingw-w64-i686` 14.2 (also works with the
MinGW-w64 i686 toolchain on Windows). Flags mirror `opensmacx.cbp`.

```sh
mkdir -p build
i686-w64-mingw32-g++ -std=c++11 -O2 -DBUILD_DLL \
  -fno-delete-null-pointer-checks -fno-strict-aliasing \
  -Wno-comment -Wno-parentheses -Wno-unknown-pragmas \
  -Itools/mingw-shim -shared -static \
  -o build/OpenSMACX.dll src/*.cpp -lgdi32 -luser32 -lwinmm
```

`tools/mingw-shim/SDKDDKVer.h` is an empty stand-in for the MSVC-only header included by
`src/stdafx.h`.

## Caveat: the mingw DLL cannot be used with `tools/opensmac_patcher.py` as-is

`ImportAdder.exe` adds imports for the 463 MSVC-mangled names (`?tech_name@@YAHPAD@Z`, ...)
baked into it, in a fixed order, and the patcher addresses import slots by position in that
list. A GCC build exports Itanium-mangled names, so the loader cannot resolve those imports.
Use MSVC for a DLL that is actually loaded by the game, or export the MSVC names via a
generated `.def` alias file. The mingw build is still useful as a fast compile check.

## Redirect manifest

`tools/redirected-functions.tsv` lists the 462 functions currently redirected by the patcher:
file offset in `terranx.exe`, virtual address (image base 0x400000; `.text` raw offset equals
its RVA, so VA = file offset + 0x400000), and the MSVC-mangled name. Generated from
`opensmac_patcher.py`.
