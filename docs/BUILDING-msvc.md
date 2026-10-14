# Building and testing with MSVC (Windows)

Verified 2026-10-14 with Visual Studio 2022 17.8 (MSVC 14.38), from a `vcvars32.bat` shell
in the repo root. The repo ships no `.vcxproj`; this single command is equivalent to the
README's "VS2019 default settings" build:

```bat
cl /nologo /O2 /EHsc /MT /W3 /DBUILD_DLL /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
   /Fo:build\obj\ /LD /Fe:build\OpenSMACX.dll src\*.cpp ^
   /link user32.lib gdi32.lib winmm.lib /IMPLIB:build\OpenSMACX.lib
```

Clean at /W3. `/MT` statically links the CRT so the DLL needs no VC redistributable.
The DLL exports every `DLLEXPORT` symbol (623 as of v0.2.3), a superset of the 462 names
`ImportAdder.exe` adds to the exe; that is expected.

## Patch and run

`tools/opensmac_patcher.py` needs `pefile` (a venv is fine). Patch the pristine GOG
`terranx.exe` (3,084,288 bytes, sha256 01901cbf…), never an exe that already has PRACX or
another in-place patch applied:

```bat
python tools\opensmac_patcher.py -e <path\to\pristine\terranx.exe> -d build\OpenSMACX.dll -o <test folder>
```

Use a test folder that is a full copy of the game directory, so `terranx_opensmacx.exe`
finds its data files. Expected output: "Functions redirected: 462". The exe should boot to
the Alien Crossfire main menu with `OpenSMACX.dll` loaded.
