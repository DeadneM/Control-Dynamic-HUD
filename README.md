# Control Dynamic HUD

A modern dynamic-HUD plugin for **Control** (PC), rebuilt as a native x64 plugin for the existing Control Plugin Loader.

## Current status

### Canonical stable base: V0.9

V0.9 fixed the core lifecycle problems:

- current Control HUD factory identified safely by signature;
- current `ui::Page::onReadyForBindings` ABI corrected to **vtable slot 28**;
- public Coherent `UIGTView` wrapper identified correctly;
- `ExecuteScript` resolved from the public View's **vtable slot 61**;
- persistent slot-28 lifecycle hook survives pause/menu page recreation;
- no legacy per-frame reinjection storm.

### V1.0J configurable test branch

Branch: `dev/v1.0-configurable-hud`

V1.0J adds configuration through `plugins/ControlDynamicHUD.ini`.

Currently configurable:

- Health bar
- Mission log
- Crosshair
- Expedition forced-modifier panel
- Ground Slam targeting circle
- Multi Launch held-object input prompts (experimental)
- global **Show HUD** hotkey

Every HUD element that the mod currently hides has its own configurable hide delay and fade duration.

The default Show HUD key is **F1**. The hotkey is detected natively from the HUD update path, so it does not depend on Coherent receiving function-key keyboard events. Pressing it forces every HUD element managed by the mod visible for a configurable duration, then each element resumes its normal independent timer.

The crosshair is now driven by Control's real native `onPlayerAimChanged(bool)` event plus combat state, rather than treating generic `PLAYER_MODE_ACTION` as aiming. This prevents normal jumps from reviving the crosshair while preserving it for actual aiming and combat.

The Ground Slam targeting circle can be suppressed natively by redirecting the current-build `slam_target_show` event to `slam_target_hide`. The patch is signature-based and fail-open if the path is already modified or unsupported.

## Installation

Copy these files into:

```text
Control\plugins\
```

Files:

```text
ControlDynamicHUD.dll
ControlDynamicHUD.ini
```

The plugin writes:

```text
plugins\ControlDynamicHUD.log
```

DX11 and DX12 share the same plugin binary.

## Configuration

### General

```ini
[General]
Enabled=1
ShowDiagnostics=0
```

### Global Show HUD hotkey

```ini
[Hotkeys]
ShowHUDKey=F1
ShowHUDDurationMs=5000
```

### Health

```ini
[Health]
Enabled=1
ShowDuringCombat=1
ShowHealthThresholdPercent=100
HideDelayMs=2000
FadeDurationMs=300
VisibleOpacityPercent=80
```

### Mission Log

```ini
[MissionLog]
Enabled=1
InitialHideDelayMs=2000
AfterMapCloseHideDelayMs=3000
MissionUpdateVisibleMs=7000
FadeDurationMs=300
ShowInMap=1
```

### Crosshair

```ini
[Crosshair]
Enabled=1
HideDelayMs=1000
FadeDurationMs=300
```

### Expedition forced-modifier panel

```ini
[Expedition]
Enabled=1
HideDelayMs=2000
FadeDurationMs=300
```

### Ground Slam target circle

```ini
[GroundSlam]
HideTargetCircle=1
```

### Main menu cleanup

```ini
[MainMenu]
HideNewGame=0
HideMissionSelect=0
```

Both options are **off by default**. When either option is enabled, the plugin hooks the separate `menu.ui` page. V1.0J hard-disables the matching entry, removes it from the DOM, removes keyboard focus, blocks pointer input, and installs capture-phase guards so the hidden entry cannot still be activated.

### Multi Launch held-object input glyphs

```ini
[MultiLaunch]
HideInputPrompts=1
HideDelayMs=0
FadeDurationMs=150
```

V1.0F/V1.0G DOM targeting and the V1.0H JavaScript model-write path were rejected. V1.0I moves Multi Launch suppression fully native: the HUD's Launch block is read directly, the three MultiLaunchIndicator visibility bytes are used as the active-state signal, and the native InteractionMarkerData vector is modified after the game's HUD update. The exact audited layout is: HUD+0x168 Launch block; Multi Launch indicators at +0x88/+0x120/+0x1B8 with m_bHighlightVisible at +0x60; HUD+0x160 InteractionMarkersUIData; data vector at +0x10; InteractionMarkerData size 0x18; m_fButtonOpacity at +0x10.

All timing values are in milliseconds.

## Technical notes

The current game/UI ABI is documented in:

```text
docs/CURRENT_UI_ABI_AUDIT.md
```

The important current-build findings are:

- HUD page slot 27 is an update path and must not be used for injection.
- HUD page slot 28 is the current `onReadyForBindings` callback.
- `ui::Page::getView()` returns the public Coherent UIGTView wrapper.
- For the audited Coherent build, public View slot 61 resolves to the correct queued `ExecuteScript` wrapper.
- The lifecycle hook stays active so the script is re-injected after pause/menu page recreation.

## Roadmap

- V1.0D real-aim crosshair behavior validated;
- validate V1.0I fully-native Multi Launch button-opacity override;
- validate V1.0J safe main-menu removal with no remaining clickable/focusable hitbox;
- validate the hardened Expedition forced-modifier behavior;
- add configurable behavior for additional HUD elements where safe;
- optional in-game configuration overlay later;
- public V1.0 release after the configurable branch is validated.

## Credits

- Remedy Entertainment for Control.
- reg2k for the Control Plugin Loader and prior Control modding research.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
