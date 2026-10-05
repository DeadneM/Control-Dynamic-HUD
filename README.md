# Control Dynamic HUD

A modern, performance-focused dynamic HUD mod for **Control** (PC), rebuilt from scratch as a native plugin for the existing Control Plugin Loader.

The project exists because the original DynaHUD/UIFramework stack can repeatedly reinject HUD JavaScript on current versions of the game, causing severe performance degradation. Control Dynamic HUD aims to preserve the useful dynamic-HUD behavior without the reinjection storm or long-lived listener/observer buildup.

## Current status

### V0.2 HUD Lifecycle Probe

V0.2 still makes **no visual HUD changes**. It identifies the native HUD factory by a signature shared by both DX11 and DX12, installs a small fail-open trampoline hook, calls the original game function, and logs each real factory invocation plus the returned HUD object pointer.

Offline analysis of the supplied executables resolved the signature exactly once in both renderers:

- DX12 factory: RVA `0x3960E0`
- DX12 HUD constructor: `0x1405F2570`
- DX11 factory: RVA `0x3960E0`
- DX11 HUD constructor: `0x1405F2510`

These absolute addresses are **not hard-coded** by the plugin. They are documented results; V0.2 finds the factory from its byte signature and resolves the constructor from the relative call.

### V0.1 results

V0.1 validated the native plugin foundation under the user's normal DX12 path:

- `Control_DX12.exe`: detected
- `CoherentUIGT.dll`: loaded
- `uiresources\\p7\\hud.ui`: one match
- `m_bIsHudVisible`: one match
- direct exported `Coherent::UIGT::Page::getView`: not available

## Installation

1. Install the existing Control Plugin Loader.
2. Disable/remove the old `plugins\\UIFramework.dll` while testing Control Dynamic HUD.
3. Copy `ControlDynamicHUD.dll` and `ControlDynamicHUD.ini` into the game's `plugins` folder.
4. Start Control normally in DX11 or DX12.

The plugin writes:

```text
plugins\\ControlDynamicHUD.log
```

For V0.2, play briefly, open/close the map and pause menu several times, then send the log. We want to learn whether the native HUD factory runs once per real HUD object or behaves like the noisy legacy `Menu ready` path.

## Roadmap

- **V0.1**: DX11/DX12 native probe, no hooks or patches
- **V0.2**: native HUD lifecycle factory probe
- **V0.3**: inject a minimal bridge once per validated HUD object
- **V0.4**: dynamic health bar
- later: crosshair, ammo, mission/objective display, opacity and timing controls

The long-term target is one DLL for both DX11 and DX12, signature-based where possible, fail-open on unsupported builds, and with no permanent per-frame polling.

## Safety model

V0.2 writes only the small entry detour required for its diagnostic factory hook. If the signature is missing or not unique, the plugin logs the condition and leaves the game untouched.

It does not alter HUD state, gameplay values, save data, graphics state, or resource files.

## Credits

- Remedy Entertainment for Control.
- reg2k for the Control Plugin Loader and prior Control modding work that established the `plugins\\*.dll` workflow.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
