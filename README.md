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

### V1.0P hard-suppression test candidate

Branch: `test/v1.0p-hard-suppress`

V1.0P keeps the cumulative V1.0O feature set and hardens the three still-unresolved paths from in-game testing: the remaining main-menu ghost row is physically removed from the DOM, center-dot discovery now searches the global screen center including unnamed SVG/pseudo primitives, and Multi Launch suppression now changes both `HasAimTarget` and `ReticuleHidden` inside the updater before Coherent notification.

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

V1.0N's native `CrosshairData + 0xAC` experiment was rejected after the field was confirmed to be `m_fMinReticuleSize`, part of reticle sizing/scatter rather than an independent center-dot visibility switch. V1.0O's HUD-local search did not hide the visible dot in game. V1.0P therefore searches semantic dot selectors, small centered descendants, `document.elementsFromPoint()` at the screen center, centered SVG primitives and pseudo-elements. The default `HideDelayMs=0` requests an always-hidden center dot, while the global Show HUD hotkey temporarily restores it.

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

**Release rule:** `HideNewGame=0` and `HideMissionSelect=0` are the public release defaults. This V1.0P test package temporarily uses `1/1` only so both paths can be validated immediately. Native action guards still make `New Game` and `Mission Select` no-ops while enabled. `Mission Select` also keeps the native `MenuOptions.m_bHasMissionSaves=false` suppression. V1.0O reduced the menu from two ghost positions to one; V1.0P physically removes the matched UI row instead of only hiding it, targeting that final ghost position.

### Multi Launch object-attached indicators

```ini
[MultiLaunch]
HideObjectIndicators=1
HideDelayMs=0
```

The exact current-build model layout is audited from the registration functions themselves. The Launch block is at HUD+0x168. The three object-attached MultiLaunchIndicator objects are at +0x88/+0x120/+0x1B8; within each, m_bHasAimTarget is +0x40, m_bIsReticuleHidden is +0x41, and m_iValue is +0x44.

V1.0L and V1.0M proved that writing these fields after the HUD update is too late. V1.0N moved `ReticuleHidden` suppression before notification but still did not hide the visible indicators in game. V1.0P patches the updater's own state generation so a suppressed slot publishes both `HasAimTarget=false` and `ReticuleHidden=true` before Coherent notification. With `HideDelayMs=0`, suppression is unconditional outside the F1 Show HUD override. The log records the post-update values for each slot, which lets the next test distinguish a bad field mapping from a different visual model.

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
- validate V1.0P dual pre-notification Multi Launch suppression;
- validate V1.0P global center-dot suppression;
- validate V1.0P physical main-menu row removal and confirm the last ghost slot is gone;
- validate the hardened Expedition forced-modifier behavior;
- add configurable behavior for additional HUD elements where safe;
- optional in-game configuration overlay later;
- public V1.0 release after the configurable branch is validated.

## Credits

- Remedy Entertainment for Control.
- reg2k for the Control Plugin Loader and prior Control modding research.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
