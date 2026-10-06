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

### V1.0N configurable test branch

Branch: `dev/v1.0-configurable-hud`

V1.0N adds configuration through `plugins/ControlDynamicHUD.ini`.

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

V1.0M's DOM-based center-dot search was rejected. V1.0N hooks the native CrosshairData updater instead and temporarily forces the source minimum-reticle size to 0 before the model notification is sent. The default `HideDelayMs=0` requests an always-hidden center dot except during the global Show HUD override. This native mapping is still a test candidate until validated in game.

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

Both options are **off by default**. Native action guards still make `New Game` and `Mission Select` no-ops when their corresponding option is enabled. V1.0N additionally forces `MenuOptions.m_bHasMissionSaves=false`, so Mission Select is removed from the native menu model rather than merely hidden from the DOM. New Game has no equivalent visibility boolean in the audited MenuOptions model, so V1.0N keeps the visual removal and adds an experimental navigation-skip guard for the hidden row.

### Multi Launch object-attached indicators

```ini
[MultiLaunch]
HideObjectIndicators=1
HideDelayMs=0
```

The exact current-build model layout is audited from the registration functions themselves. The Launch block is at HUD+0x168. The three object-attached MultiLaunchIndicator objects are at +0x88/+0x120/+0x1B8; within each, m_bHasAimTarget is +0x40, m_bIsReticuleHidden is +0x41, and m_iValue is +0x44.

V1.0L and V1.0M proved that writing these fields after the HUD update is too late: the Multi Launch updater resets m_bIsReticuleHidden to false and publishes that state to Coherent first. V1.0N therefore hooks the real Multi Launch updater and changes its hidden-state publication before the model notification. Once the independent HideDelayMs expires, the updater publishes ReticuleHidden=true itself. F1 temporarily restores the vanilla publication path. This pre-notification implementation is the first timing-correct Multi Launch candidate and still needs in-game validation.

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
- validate V1.0N pre-notification Multi Launch suppression;
- validate V1.0N native CrosshairData center-dot suppression;
- validate V1.0N native Mission Select removal and experimental New Game navigation skip;
- validate the hardened Expedition forced-modifier behavior;
- add configurable behavior for additional HUD elements where safe;
- optional in-game configuration overlay later;
- public V1.0 release after the configurable branch is validated.

## Credits

- Remedy Entertainment for Control.
- reg2k for the Control Plugin Loader and prior Control modding research.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
