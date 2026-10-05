# Control Dynamic HUD

A modern, performance-focused dynamic HUD mod for **Control** (PC), rebuilt from scratch as a native plugin for the existing Control Plugin Loader.

The project exists because the original DynaHUD/UIFramework stack can repeatedly reinject HUD JavaScript on current versions of the game, causing severe performance degradation. Control Dynamic HUD aims to preserve the useful dynamic-HUD behavior without the reinjection storm or long-lived listener/observer buildup.

## Current status

**V0.1 Probe** is a diagnostic foundation. It does **not** change the HUD yet.

It verifies that a single native x64 plugin can run under both `Control_DX11.exe` and `Control_DX12.exe`, detects the loaded Coherent UI module, and locates stable HUD-related strings without installing hooks or writing to game memory.

Validated so far:

- DX12 plugin load: **working**
- `CoherentUIGT.dll`: **detected**
- `uiresources\\p7\\hud.ui`: **detected once**
- `m_bIsHudVisible`: **detected once**
- direct exported `Coherent::UIGT::Page::getView`: **not exported**, so V0.2 will resolve the HUD lifecycle through internal signatures instead

## Installation

1. Install the existing Control Plugin Loader.
2. Disable/remove the old `plugins\\UIFramework.dll` while testing Control Dynamic HUD.
3. Copy `ControlDynamicHUD.dll` and `ControlDynamicHUD.ini` into the game's `plugins` folder.
4. Start Control normally in DX11 or DX12.

V0.1 writes its diagnostic log to:

```text
plugins\\ControlDynamicHUD.log
```

## Roadmap

- **V0.1**: DX11/DX12 native probe, no hooks or patches
- **V0.2**: identify and hook the real `hud.ui` lifecycle once per true page instance
- **V0.3**: dynamic health bar
- later: crosshair, ammo, mission/objective display, opacity and timing controls

The long-term target is one DLL for both DX11 and DX12, signature-based where possible, fail-open on unsupported game builds, with no permanent per-frame polling.

## Project layout

```text
src/                    Native plugin source
config/                 Default INI
build/                   Local build helper scripts
.github/workflows/      Reproducible CI build
```

## V0.1 safety model

V0.1 performs no hooks, no patches, and no memory writes. It only reads loaded module metadata / executable memory for diagnostics and writes `ControlDynamicHUD.log`.

## Credits

- Remedy Entertainment for Control.
- reg2k for the Control Plugin Loader and prior Control modding work that established the `plugins\\*.dll` workflow.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
