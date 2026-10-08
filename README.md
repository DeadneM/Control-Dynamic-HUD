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

### V1.0Z Page::bind-window navigation test candidate

Branch: `test/v1.0z-bindwindow`

V1.0Z is based on the validated V1.0Y navigation behavior and the stable V1.0U1 HUD behavior. The center-dot mask and split Multi Launch icon/outline paths remain frozen. V1.0Y proved that the Coherent `engine.on("OnNavigateUp"/"OnNavigateDown")` wrapper must exist before Control registers its real navigation handlers, but injecting JavaScript before the entire native `onReadyForBindings` removed the menu background image/video. V1.0Z moves the same wrapper into the **first native `ui::Page::bind` call inside `onReadyForBindings`**. This occurs after the page/base/model setup has run but before callback registration completes, aiming to preserve both the validated ghost-slot skip and the normal menu background lifecycle.

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

V1.0N's native `CrosshairData + 0xAC` experiment was rejected after the field was confirmed to be `m_fMinReticuleSize`. V1.0Q proved that the first center candidate belonged to the Launch/Multi Launch presentation, while the normal center dot remained. V1.0R's stacked-candidate scan still did not reach the normal dot. V1.0S therefore adds a CSS mask to `.awesome-crosshair` itself, punching a very small resolution-scaled transparent circle through the exact center of the complete component. This path does not need to know which child, pseudo-element, SVG or image owns the dot. F1 removes the mask temporarily with the rest of the managed HUD.

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

**Release rule:** `HideNewGame=0` and `HideMissionSelect=0` are the public release defaults. Test packages may temporarily use `1/1` for validation.

These options are not only cosmetic. Their purpose is to **protect a completed-game autosave**. After the story is finished, returning through Mission Select can move progression back to an earlier mission and subsequent autosaves can replace the completed-state save. Starting New Game can replace/delete the active completed-game save path entirely. The intended protected workflow is therefore: once the game is completed, continue playing only from the completed-state autosave.

`Mission Select` is already removed correctly at the native `MenuOptions.m_bHasMissionSaves` model layer. `New Game` has no equivalent visibility field in `MenuOptions`. V1.0Y validated the navigation fix but removed the menu background image/video because its wrapper was injected before the whole native ready callback. V1.0Z keeps the same wrapper logic but injects it from the first `ui::Page::bind` call inside the original ready function, after visual/base/model initialization and before callback registration finishes. If the bind-window hook cannot be installed, the mod falls back to V1.0Y timing so the save-protection navigation behavior fails safe rather than silently returning the ghost slot.

### Multi Launch icon and white outline

```ini
[MultiLaunch]
HideIcon=1
HideOutline=1
```

V1.0T proved that the engine-level approach used by **reg2k's NoHighlight v1.0** reaches the correct Launch highlight subsystem, but its broad conditional-branch skip suppresses both visible effects together.

V1.0U separates them at their native outputs:

- `HideIcon` forces the unique `LaunchIndicator.m_bHighlightVisible` publication to false before the HUD model is notified. This targets the object-attached Launch / Multi Launch icon without intentionally disabling the world outline.
- `HideOutline` suppresses only `HighlightComponentState.m_out_pRenderObject`, preventing the selected object's white render outline from being published while leaving the HUD icon path independent.

For compatibility, old INI files containing `HideObjectIndicators` are still accepted as the default value for both new options when `HideIcon` / `HideOutline` are absent.

The F1 Show HUD override restores both native publication paths. V1.0U additionally hooks the verified HighlightComponent refresh callback and, when a recently active component is available, re-runs it once after F1 restores the outline path so the white outline can be republished immediately instead of waiting for a target change.

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
- V1.0U1 HUD behavior validated in-game; keep center-dot and split Multi Launch paths frozen;
- V1.0S+ center-pixel mask validated for the normal crosshair dot; keep this path frozen;
- V1.0Y navigation ghost fix validated: Continue <-> Options no longer passes through an invisible New Game slot;
- validate V1.0Z first-`Page::bind` injection: keep Y navigation fix while restoring menu background image/video;
- validate the hardened Expedition forced-modifier behavior;
- add configurable behavior for additional HUD elements where safe;
- optional in-game configuration overlay later;
- public V1.0 release after the configurable branch is validated.

## Credits

- Remedy Entertainment for Control.
- **reg2k** for the Control Plugin Loader, DynaHUD, and **NoHighlight v1.0**. NoHighlight exposed the correct engine-level Launch highlight path. V1.0T reproduced its broad signature-scanned branch method; V1.0U uses that finding to split the HUD icon and world-outline outputs into independent controls.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
