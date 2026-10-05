# Technical Notes

## Why this rewrite exists

The original DynaHUD package uses `UIFramework.dll` to inject CSS/JavaScript into `uiresources\\p7\\hud.ui`.

On the tested current Control build (`Game version: 2060881`), diagnostics showed the same HUD page pointer receiving thousands of repeated `Menu ready` callbacks. Each callback caused UIFramework to reload DynaHUD resources. In one captured run the log contained roughly 2,500 repeated JavaScript loads for one HUD page instance. Earlier tests reached more than 3,000 CSS/JS injections.

This behavior explains the severe progressive frame-rate loss seen with the old stack.

Attempts to deduplicate injection inside the legacy framework proved that stopping the reinjection removes the lag, but also exposed how tightly the old DynaHUD lifecycle depended on repeated reinjection to reacquire recreated HUD DOM nodes.

Control Dynamic HUD therefore starts over as a dedicated native plugin rather than layering more compatibility work onto UIFramework 1.0.

## Plugin loading

The existing Control Plugin Loader enumerates `plugins\\*.dll` and calls `LoadLibraryA` for each plugin when the game's process entry point is reached. A plugin therefore needs no proprietary registration interface; ordinary DLL initialization is sufficient.

## V0.1 architecture

V0.1 is intentionally freestanding:

- x64 Windows DLL
- no CRT dependency
- no external runtime dependency
- no import table
- resolves required Win32 exports itself
- ASLR relocations enabled
- fail-open behavior
- no hooks
- no patches
- no game-memory writes

It logs:

- executable name and base
- `CoherentUIGT.dll` presence/base
- occurrences of `uiresources\\p7\\hud.ui`
- occurrences of `m_bIsHudVisible`
- whether a guessed decorated `Coherent::UIGT::Page::getView` symbol is exported

## First DX12 probe result

Observed with the user's normal renderer:

```text
Executable: Control_DX12.exe
CoherentUIGT.dll: loaded
hud.ui string matches: 1
m_bIsHudVisible matches: 1
Coherent Page::getView export: NOT FOUND
Probe complete.
```

This confirms DX12 as the currently tested primary target and confirms that V0.2 must locate the HUD lifecycle through internal signatures/call-site analysis rather than a direct exported Coherent method.

## V0.2 direction

The next stage should remain diagnostic-first:

1. find references to the unique `hud.ui` string in DX12 and DX11;
2. identify the code path that receives/creates the Coherent HUD page;
3. find a stable hook site shared by both executables where possible;
4. log real page creation/destruction or stable page identity;
5. only after that is validated, inject any HUD bridge.

No visual behavior should be added until the lifecycle hook is known to be stable and non-repeating.
