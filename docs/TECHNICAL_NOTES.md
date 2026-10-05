# Technical Notes

## Legacy DynaHUD/UIFramework diagnosis

The original DynaHUD package uses `UIFramework.dll` to inject CSS/JavaScript into `uiresources\\p7\\hud.ui`.

On the tested current Control build (`Game version: 2060881`), diagnostics showed the same HUD page pointer receiving thousands of repeated `Menu ready` callbacks. Each callback caused UIFramework to reload DynaHUD resources. Captured runs showed roughly 2,500 to more than 3,000 repeated JS/CSS injections for one HUD page instance.

Stopping the repeated injection removes the severe lag, but the old DynaHUD logic also depended on reinjection to reacquire recreated HUD DOM nodes. That made UIFramework 1.0 a poor foundation for a modern repair.

Control Dynamic HUD therefore starts over as a dedicated native plugin.

## Plugin loading

reg2k's Control Plugin Loader enumerates `plugins\\*.dll` and calls `LoadLibraryA` for each plugin when the process entry point is reached. No proprietary registration API is required.

## V0.1 findings

The first probe performed no hooks or memory writes and confirmed:

```text
Executable: Control_DX12.exe
CoherentUIGT.dll: loaded
hud.ui string matches: 1
m_bIsHudVisible matches: 1
Coherent Page::getView export: NOT FOUND
Probe complete.
```

The unique `hud.ui` string was then used as the starting point for static analysis of both supplied executables.

## V0.2 reverse-engineering result

The unique `uiresources\\p7\\hud.ui` reference is inside a constructor used by a small object factory.

DX12:

```text
Factory RVA:       0x3960E0
Constructor:       0x1405F2570
hud.ui LEA:        0x1405F25A2
```

DX11:

```text
Factory RVA:       0x3960E0
Constructor:       0x1405F2510
hud.ui LEA:        0x1405F2542
```

The factory block is structurally identical between DX11 and DX12. It allocates `0x310` bytes, passes the allocation to the HUD-specific constructor, and returns the resulting object.

V0.2 recognizes this factory through a wildcard byte signature shared by both executables. The plugin verifies there is exactly one match before modifying memory.

### V0.2 hook

The hook replaces the first 19 bytes of the factory with an absolute x64 jump. Those 19 bytes end on an instruction boundary and contain no RIP-relative instructions. A trampoline copies those bytes, jumps back to `factory + 19`, and therefore preserves the original function.

The diagnostic hook logs:

```text
HUD factory signature matches: 1
HUD constructor target: ...
HUD factory hook installed.
HUD factory call #N ...
HUD object returned: ...
```

No HUD state is changed.

### Why this point is better than legacy Menu ready

The legacy `Menu ready` callback fired thousands of times for one page pointer. The V0.2 factory sits on the native allocation/construction path for the HUD object itself. The test goal is to establish whether this path corresponds to genuine object creation and remains quiet through ordinary pause/map transitions.

## Next decision

If V0.2 shows only a small number of factory calls tied to genuine HUD creation, V0.3 can attach a minimal one-time bridge to the returned object. If the factory is unexpectedly noisy, analysis will move one level deeper to the constructor-owned state or its page creation call rather than adding throttling.
