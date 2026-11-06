# DMA development dependencies

This working copy retains the five files needed for the verified Release x64 build. Generated executables, runtime DLLs and downloaded SDK archives were removed after compilation. The runtime DLLs are intentionally not included.

## Building

Keep these five files together in this folder:

| Development files | Provider | Verified local information |
| --- | --- | --- |
| [vmmdll.h](<vmmdll.h>), [vmm.lib](<vmm.lib>) | [MemProcFS](https://github.com/ufrisk/MemProcFS) | Header version 5.18.10; x64 import library |
| [leechcore.h](<leechcore.h>), [leechcore.lib](<leechcore.lib>) | [LeechCore](https://github.com/ufrisk/LeechCore) | Header version 2.23; x64 import library |
| [FTD3XX.lib](<FTD3XX.lib>) | [FTDI D3XX](https://ftdichip.com/drivers/d3xx-drivers/) | x64 DLL import library from FTD3XXLibrary 1.3.0.10 |

The original project referenced older headers (MemProcFS 5.8 and LeechCore 2.9.1). The current copies are the versions listed above. The exact MemProcFS and LeechCore import-library release versions have not been independently established; the libraries were checked for x64 architecture and linked successfully, not validated against a runtime DLL set.

In [memory.cpp](<../../src/memory/memory.cpp>), only three declarations were adjusted from `LPSTR` to `LPCSTR`: the two initialization argument arrays and the file-list callback name parameter. This matches the supplied header signatures without changing arguments or control flow. No SDK header modifications or functionality removal were needed.

The final source was compiled in an isolated **Release | x64** build on **2026-11-05**, with **0 errors and 979 warnings**. Warnings, including C4828 source-encoding warnings under `/utf-8`, remain unresolved. After synchronizing only duplicated offset values and numeric test expectations with the supplied table, **all 58 offline suites passed in one complete run**. No test assertions or coverage were removed. Runtime behavior was not tested. See the [main build instructions and validation notes](<../../../README.md#build>).

Do not replace x64 import libraries with Win32/x86 versions. If C1083 reports a missing header, restore the actual header in this folder. DLLs, renamed DLLs and empty placeholder files cannot substitute for development headers or import libraries.

## Running after a local build

The generated executable is `WTC/build/x64/Release/DMA-SDK.exe`; it is not present in the cleaned source tree. Move it to a runtime folder containing compatible x64 `vmm.dll`, `leechcore.dll` and `FTD3XX.dll`. These runtime files are not needed for compilation or linking, and the build does not download or copy them.

Use mutually compatible headers, import libraries and runtime DLLs. MemProcFS and the selected device/features may require additional provider plugins and support files. Consult the chosen official distributions; successful compilation does not establish complete runtime compatibility. Do not obtain loose DLLs from unofficial download sites.

## GitHub publication and licenses

This GitHub update explicitly tracks the five development files supplied in the user-provided ZIP, so clones include them. The [.gitignore](<../../../.gitignore>) still excludes additional untracked vendor files and runtime/build output; it does not prevent already-tracked dependencies from being included in commits. The original vendor licenses, notices and redistribution requirements must be reviewed independently; their inclusion does not certify permission or combined-program license compatibility.

The root project license does not grant rights to these dependencies. The original project identifies MemProcFS as AGPL-3.0. Review the exact licenses and combined-program distribution requirements for the versions of MemProcFS, LeechCore and FTDI you use. Excluding vendor files alone does not resolve linked-binary distribution obligations. This package is not a legal compatibility certification.
