# WTC — v5.25 Full Language Public

Windows x64 C++20 / DirectX 11 overlay. English is the initial menu language; Spanish is selectable. Original project and third-party attribution are retained in the license files.

## This release

- **Air-only static-entity filter:** optional, enabled initially. An aircraft/drone must remain within a 3 cm position envelope for 2 seconds across at least four fresh position samples. Duplicate render frames do not count. Movement restores visibility immediately; snapshot gaps, scene/identity-epoch changes and disabled/invalid frame transitions reset history. The same history gates aircraft ESP, A2A/G2A target selection and classified aircraft radar contacts.
- **Bounded overhead:** fixed 4,096-entry, four-way cache (under 512 KiB), bounded probes, no per-update allocations or additional DMA reads for this filter. Capacity collisions evict history conservatively rather than hiding an unobserved contact.
- **Important limitation:** this is a stationary-position heuristic, not proof that an entity is fake. Parked aircraft or exceptionally stable helicopters may also be hidden. Disable the option when needed. No unverified terrain-height or alive-state offsets have been invented. Unknown radar categories fail open.
- **Radar:** checked read success, finite geometry, immediate invalid-map rejection, local/team exclusion and bounded classification history. No previous map view is drawn after an invalid/closed visibility gate. Actual game map-latch semantics, modes and alignment still require live testing.
- **INVUL:** all combat ground-vehicle subtypes with validated ground classification are eligible, including SPAA and future subtype names. Dummies, structures, aircraft and ships remain excluded. A validated true flag is required; a timer never determines vulnerability.
- **Language:** English/Spanish selector. Language and static-air-filter preferences use the existing explicit manual profile save. Switching a setting never writes configuration automatically. Application-owned runtime states, errors, diagnostics, tooltips, configuration dialogs, clipboard reports, console messages and color-editor controls now follow the selected language as well as the static menu. Real vehicle/weapon names, versions, paths, operating-system/device names and numeric values are deliberately preserved as data.
- Existing ballistic solvers and the shared A2A/G2A circle/cross/connector style are retained. This release does not apply an arbitrary correction to ballistic lead.

## Source-only state and publication

The current working copy contains source code, tests, project files and the five DMA development dependencies listed below. Generated executables, runtime DLLs, object/linker files, SDK ZIP packages and temporary dependency backups were removed after the successful build. No prebuilt executable is included; building locally creates new output files.

This GitHub update explicitly tracks the five DMA development files supplied in the v5.25 English ZIP, so a fresh clone includes them. The [.gitignore](<.gitignore>) still excludes build output, runtime data, local IDE state and any additional untracked vendor files; ignore rules do not exclude dependencies that have already been explicitly added to Git. Runtime DLLs remain separate. Third-party licensing and redistribution requirements still apply; inclusion is not a legal compatibility certification.

The publication excludes local Visual Studio user settings and contains no runtime executables, DLLs, build output or IDE cache. A targeted scan found no strong credential/private-key patterns; this is not an exhaustive security or privacy audit. Before future publications, review the actual staged files for private settings, credentials and logs; ignore rules do not remove files that were already tracked.

## Build

1. Install Visual Studio with Desktop development with C++, MSVC **v145**, and the Windows SDK. The verified target is **Release | x64**; Win32 and Debug configurations were not verified.
2. Keep all five development files in [WTC/lib/dma](<WTC/lib/dma/>). They are already present in this working copy:

   | Dependency | Required development files |
   | --- | --- |
   | MemProcFS | [vmmdll.h](<WTC/lib/dma/vmmdll.h>), [vmm.lib](<WTC/lib/dma/vmm.lib>) |
   | LeechCore | [leechcore.h](<WTC/lib/dma/leechcore.h>), [leechcore.lib](<WTC/lib/dma/leechcore.lib>) |
   | FTDI D3XX | [FTD3XX.lib](<WTC/lib/dma/FTD3XX.lib>) |

   All three import libraries were checked as x64. A 32-bit library is not interchangeable with an x64 library. See the [DMA dependency notes](<WTC/lib/dma/README.md>) for versions, providers and licensing.
3. Open [sdk.sln](<WTC/sdk.sln>), select **Release | x64**, and build. Alternatively, from a Visual Studio Developer PowerShell at the repository root:

   ```powershell
   MSBuild.exe WTC/sdk.sln /p:Configuration=Release /p:Platform=x64 /m
   ```

   Expected generated output: `WTC/build/x64/Release/DMA-SDK.exe`. Release linking disables debug/CodeView metadata. This output was removed during source cleanup and will be regenerated by the next build.
4. Runtime DLLs are **not required to compile or link**, but are required to run. Move the generated executable to your existing runtime folder containing compatible x64 `vmm.dll`, `leechcore.dll` and `FTD3XX.dll`. The build does not fetch or copy these DLLs. Other provider files/plugins may be needed; execution was not tested.

### Build warnings and SDK compatibility

- The latest verified local Release x64 build on **2026-11-05** completed with **0 errors and 979 warnings**. The count applies to that build, not every future environment or incremental build.
- Warnings remain unresolved. They include **C4828** for source characters that are not valid under `/utf-8`; see, for example, [units.hpp](<WTC/src/game/classes/units.hpp>). No source encoding conversion or warning suppression was applied. A successful build does not mean the warnings are harmless or runtime behavior is verified.
- The supplied headers identify MemProcFS **5.18.10** and LeechCore **2.23**. Three declarations in [memory.cpp](<WTC/src/memory/memory.cpp>) were changed from `LPSTR` to `LPCSTR`: the two initialization argument arrays and the file-list callback name parameter. This matches the installed SDK signatures without changing arguments, control flow or application logic.
- If a fresh checkout reports **C1083**, check that both required headers exist in the dependency folder. Missing-library errors require the corresponding import libraries; runtime DLLs and empty placeholder files cannot replace them.

Offline regression tests can be run separately with Windows PowerShell. All 58 suites passed in the final publication check described below:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./WTC/tests/run.ps1
```

## Validation and limitations

**Publication verification (2026-11-05):** the final source compiled in an isolated Release x64 build with **0 errors and 979 warnings**, and **all 58 offline regression suites passed in one complete run**. The application was not executed, and runtime/game behavior or the external DLL set was not tested. Generated validation binaries are not published.

The first test attempt exposed expectations that still used the previous offsets. With user approval, only hexadecimal values were synchronized with the supplied table: three duplicated projectile-layout defaults and numeric fixtures/expectations in five test files. No algorithms, control flow, test assertions or test coverage were removed or disabled. The versioned historical free-API response fixture and the deliberately invalid cached RVA used by the startup fallback test were preserved. Other code from the supplied ZIP was left unchanged.

**Historical validation from the original release notes (not rerun for the SDK update):** the release includes dedicated tests for bilingual runtime catalog coverage, producer/console message inventories, complete printf/newline contracts, dynamic sentence templates, raw-data preservation, language switching, stable widget IDs and explicit manual profile persistence. The original notes reported a Release x64 clean rebuild with zero build errors and all 58 offline suites passing in one complete run. The runtime catalog test exercised 393 bilingual entries and 72 dynamic sentence templates, including preservation of real names/percent data, numeric formatting, cross-thread language selection and unchanged manual-save wiring. This does not replace live in-game or visual testing. The air filter uses 229,400 bytes of fixed storage. A host-only one-million-call stress test measured roughly 22–28 ns per policy call; that is not a live DMA/render-frame performance guarantee.

Offline tests cover numerical solvers, temporal filters, render-command contracts, language/ID/persistence rules and the radar implementation with mocked DMA/ImGui. They do not constitute a full simulated ESP/DMA/game integration test. Real in-game ghost discrimination, radar behavior and ballistic accuracy remain to be confirmed on live data. No replay recording, diagnostic export or visual multi-resolution tests are added.

## Licenses

See the original root LICENSE and WTC/lib/imgui/LICENSE.txt, including bundled stb notices. Root licensing does not replace the licenses of DMA SDK dependencies. Linked-binary redistribution may have additional obligations; review the applicable dependency licenses before publishing. Required third-party author attribution is not personal session data and has been preserved.
