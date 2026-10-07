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

### V1.0T reg2k NoHighlight-method test candidate

Branch: `test/v1.0t-reg2k-nohighlight`

V1.0T keeps the validated V1.0S center-dot mask unchanged. For Launch/Multi Launch object highlighting it abandons the unsuccessful model/event experiments and adopts the low-level method used by reg2k's NoHighlight v1.0: a unique Launch code signature is scanned at runtime and the conditional branch at signature +4 is changed from `0F 85` to `90 E9`, forcing the game's own skip path while preserving the original relative displacement. F1 temporarily restores the original bytes and the patch is re-applied when the Show HUD override expires. New Game remains the only unresolved main-menu entry; V1.0T additionally compacts lower menu siblings when the removed branch came from a fixed/absolute layout.

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

**Release rule:** `HideNewGame=0` and `HideMissionSelect=0` are the public release defaults. This V1.0S test package temporarily uses `1/1` only for validation. `Mission Select` is already removed correctly at the native `MenuOptions.m_bHasMissionSaves` model layer. `New Game` has no equivalent visibility field in `MenuOptions`, so V1.0S treats it as the sole remaining static Coherent entry: its matched branch is physically removed and empty wrapper nodes are pruned upward.

### Multi Launch object-attached indicators

```ini
[MultiLaunch]
HideObjectIndicators=1
HideDelayMs=0
```

The exact current-build `MultiLaunchIndicator` model layout was audited, but V1.0P and V1.0Q conclusively showed that it is not the visual path for the dynamic icons seen on selected objects: all three slots published `HasAimTarget=false` and `ReticuleHidden=true` and the icons remained visible. V1.0Q also forced the native `InterfaceOptions.m_bTargetIndicatorEnabled` flag off, with no effect on those object icons.

V1.0T supersedes the V1.0R/V1.0S selection-highlight event approach. The reference `NoHighlight.dll` supplied for comparison identifies itself as `NoHighlight v1.0 by reg2k`. Its Launch path scans the signature `83 7D 50 00 0F 85 ? ? ? ? ? ? ? 0F 84 ? ? ? ? 49 8B 85 ? ? ? ?`, applies an offset of +4, and changes the branch opcode bytes from `0F 85` to `90 E9`. The same signature is unique in the current Control executable. Control Dynamic HUD V1.0T reimplements that method with runtime signature validation, fail-open behavior, byte restoration for the global F1 Show HUD override, and no fixed executable address.

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
- validate V1.0T reg2k NoHighlight-style Launch branch suppression against Multi Launch object icons;
- V1.0S/V1.0T center-pixel mask validated for the normal crosshair dot; keep this path frozen;
- validate V1.0T New Game branch removal plus fixed-layout compaction;
- validate the hardened Expedition forced-modifier behavior;
- add configurable behavior for additional HUD elements where safe;
- optional in-game configuration overlay later;
- public V1.0 release after the configurable branch is validated.

## Credits

- Remedy Entertainment for Control.
- **reg2k** for the Control Plugin Loader, DynaHUD, and **NoHighlight v1.0**. The V1.0T Launch/Multi Launch highlight suppression method is based on NoHighlight's signature-scanned branch-patch approach and was reimplemented for the current executable.
- The original DynaHUD project for the dynamic-HUD concept that motivated this rewrite.

This is an unofficial community project and is not affiliated with Remedy Entertainment.
