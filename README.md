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

### V1.0A configurable test branch

Branch: `dev/v1.0-configurable-hud`

V1.0A adds configuration through `plugins/ControlDynamicHUD.ini`.

Currently configurable:

- Health bar
- Mission log
- Crosshair
- Expedition modifiers
- global **Show HUD** hotkey

Every HUD element that the mod currently hides has its own configurable hide delay and fade duration.

The default Show HUD key is **F1**. Pressing it forces every HUD element managed by the mod visible for a configurable duration, then each element resumes its normal independent timer.

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

### Expedition modifiers

```ini
[Expedition]
Enabled=1
HideDelayMs=2000
FadeDurationMs=300
```

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

- validate V1.0A INI settings and F1 Show HUD;
- finish Expedition behavior;
- add configurable behavior for additional HUD elements where safe;
- optional in-game configuration overlay later;
- public V1.0 release after the configurable branch is validated.

## Credits

- Remedy Entertainment for Control.
- reg2k for the Control Plugin Loader and prior Control modding research.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
