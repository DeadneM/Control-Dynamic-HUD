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

### V1.0R selection-highlight + stacked-dot test candidate

Branch: `test/v1.0r-selection-highlight`

V1.0R keeps the cumulative HUD feature set but changes the three paths still under test after the V1.0Q in-game results. Main-menu rows are collapsed together with their wrapper geometry instead of leaving a reserved blank slot. Multi Launch no longer relies on `MultiLaunchIndicator` fields or `InterfaceOptions.m_bTargetIndicatorEnabled`; V1.0R suppresses the native `launch_start_selection_highlight` and `launch_change_selection_highlight` event dispatches while leaving the stop event intact for cleanup. Center-dot handling now marks every small square candidate stacked at the crosshair center instead of stopping after the first candidate.

Currently configurable:

- Health bar
- Mission log
- Crosshair
- Expedition forced-modifier panel
- Ground Slam targeting circle
- Multi Launch held-object input prompts (experimental)
- global **Show HUD** hotkey

Every timed HUD element has its own configurable hide delay. CSS-managed elements can also expose a fade duration; native model-level suppressions use their own game model timing path instead of a synthetic CSS fade.

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

### Crosshair center dot

```ini
[CrosshairDot]
Enabled=1
HideDelayMs=0
```

V1.0N's native `CrosshairData + 0xAC` experiment was rejected after the field was confirmed to be `m_fMinReticuleSize`, part of reticle sizing/scatter rather than an independent center-dot visibility switch. V1.0Q proved that the first center candidate found by the Coherent search belonged to the Launch/Multi Launch ability presentation: that dot disappeared, while the normal center crosshair dot remained. V1.0R therefore collects all small, roughly square elements stacked at the center, including semantic elements, SVG primitives and pseudo-elements, while rejecting elongated shapes so the crosshair arms are not intentionally targeted. The default `HideDelayMs=0` requests immediate suppression; F1 temporarily restores the marks.

### Expedition forced-modifier panel

```ini
[Expedition]
Enabled=1
HideDelayMs=2000
FadeDurationMs=300
```

### Launch target reticle

```ini
[Launch]
HideTargetReticle=1
HideDelayMs=0
```

This hides the Launch targeting reticle that remains attached to enemies/targets.

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

**Release rule:** `HideNewGame=0` and `HideMissionSelect=0` are the public release defaults. This V1.0R test package temporarily uses `1/1` only for validation. Native action guards still make `New Game` and `Mission Select` no-ops while enabled, and Mission Select keeps the native `MenuOptions.m_bHasMissionSaves=false` suppression. V1.0Q hid the remaining entry but left a blank reserved slot. V1.0R identifies the complete row wrapper and collapses its display, height, margins and padding so the menu layout can close the gap.

### Multi Launch object-attached indicators

```ini
[MultiLaunch]
HideObjectIndicators=1
HideDelayMs=0
```

The exact current-build `MultiLaunchIndicator` model layout was audited, but V1.0P and V1.0Q conclusively showed that it is not the visual path for the dynamic icons seen on selected objects: all three slots published `HasAimTarget=false` and `ReticuleHidden=true` and the icons remained visible. V1.0Q also forced the native `InterfaceOptions.m_bTargetIndicatorEnabled` flag off, with no effect on those object icons.

V1.0R instead targets the native selection-highlight event pipeline discovered in the executable. The game emits `launch_start_selection_highlight`, two `launch_change_selection_highlight` paths and `launch_stop_selection_highlight`. When the Multi Launch hide rule is active, V1.0R suppresses only the start/change event dispatches and deliberately leaves the stop event intact so an existing highlight can still be cleaned up. F1 temporarily restores the vanilla event dispatches.

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
- Launch target reticle validated in V1.0L;
- V1.0M post-update Multi Launch suppression and DOM center-dot search rejected;
- validate V1.0R Launch selection-highlight event suppression against Multi Launch object icons;
- validate V1.0R stacked center-dot candidate suppression while preserving the crosshair arms;
- validate V1.0R collapsed main-menu wrapper and confirm the blank slot is gone;
- validate the hardened Expedition forced-modifier behavior;
- add configurable behavior for additional HUD elements where safe;
- optional in-game configuration overlay later;
- public V1.0 release after the configurable branch is validated.

## Credits

- Remedy Entertainment for Control.
- reg2k for the Control Plugin Loader and prior Control modding research.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
