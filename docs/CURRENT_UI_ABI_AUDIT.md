# Current UI ABI audit (Control current build)

This note records the reverse-engineering findings from the user's exact current game DLLs and DX11/DX12 executables.

## Audited files

- `coherentuigt.dll`
  - SHA-256: `442ac737a5068b9608e6b65dc35f3f1cefbcb56032384d5a9cfb0efe3ee19d9f`
  - TimeDateStamp: `0x5E8F6E9A`
  - SizeOfImage: `0x329000`
  - CheckSum: `0x323A4A`
- `ui_rmdwin10_f.dll`
  - SHA-256: `a073b8a61afed07f6df54177e381250871a12c31b276c8dc1679044b65b9ce75`
  - TimeDateStamp: `0x67E06A9F`
  - SizeOfImage: `0x158000`
  - CheckSum: `0x145D23`

## Root cause of the legacy UIFramework reinjection storm

The old 2020 plugins hook `UIPage::vtable[27]` and treat it as the ready-for-bindings callback.

That ABI assumption is no longer correct for the current `ui_rmdwin10_f.dll`.

Current `ui::System::update()` directly calls:

```text
Page vtable + 0xD8 = slot 27
```

for updating pages. This is a repeatedly executed update path.

Current `ui::Page::onReadyForBindings()` ends by dispatching:

```text
Page vtable + 0xE0 = slot 28
```

Therefore the old hook lands on the per-update callback instead of the actual ready callback. This explains the thousands of repeated "Menu ready" / JavaScript load entries seen in the legacy UIFramework logs.

It also explains why Control Dynamic HUD V0.4 appeared stable despite still using slot 27: V0.4 restored the hook after the first invocation, so it escaped the repeated update path before it could accumulate work.

## Current HUD vtables

The current HUD constructor assigns these primary vtables:

- DX12: RVA `0xE5E5E8`
  - slot 27: RVA `0x5F5BE0`
  - slot 28: RVA `0x5F4520`
- DX11: RVA `0xE5E608`
  - slot 27: RVA `0x5F5B80`
  - slot 28: RVA `0x5F44C0`

The DX11/DX12 function bodies differ only by their expected small renderer-build displacement.

## Coherent View ABI

The current Coherent DLL contains **two distinct View-related virtual interfaces** that must not be confused.

### Public UIGTView wrapper returned by ui::Page::getView()

The runtime View returned by the exported current-game `ui::Page::getView()` uses a vtable at:

```text
CoherentUIGT + 0x270780
```

This is not inferred from one runtime sample only. Two constructors in `coherentuigt.dll` explicitly assign that vtable.

For this public wrapper:

```text
slot 61 (+0x1E8) -> CoherentUIGT + 0x82870
```

The function at `+0x82870` has the expected public ExecuteScript signature:

- RCX = wrapper View `this`
- RDX = script C string
- R8  = frame selector

Its implementation allocates a command object, copies both strings into that command, and enqueues it through the wrapper's command queue. This matches the historical reg2k pattern:

```cpp
uiView->ExecuteScript(script);
```

Therefore **the correct ExecuteScript target for the View returned by `ui::Page::getView()` is `+0x82870` on this exact Coherent build**.

### Internal View implementation

A separate internal vtable exists at:

```text
CoherentUIGT + 0x281028
```

For that internal object:

```text
slot 13 -> CoherentUIGT + 0xD9A90
slot 61 -> CoherentUIGT + 0xDBD00
```

The function at `+0xDBD00` is therefore valid for the **internal implementation object**, but it is not directly callable with the public wrapper returned by `ui::Page::getView()`.

This distinction is the root cause of the V0.5/V0.5A crashes.

## ui::Page::getView

The current export:

```text
?getView@Page@ui@@QEAAPEAVView@UIGT@Coherent@@XZ
RVA 0x2980
```

reads the page index at `Page+0x18` and returns the pointer stored in the current UI system's page table. Runtime verification showed this returned object's vtable slot 61 resolving to `CoherentUIGT + 0x82870`, exactly matching the public-wrapper vtable assigned by the constructors above.

## V0.5 / V0.5A crash explanation

Both failed builds reached:

- a valid HUD page;
- a valid `ui::Page::getView()` result;
- a non-null internal View page pointer.

They then called `CoherentUIGT + 0xDBD00` directly using the **public wrapper pointer as RCX**. That address belongs to the separate internal View implementation, so the call used the wrong object layout and crashed.

V0.5A proved that the JavaScript body was not responsible because a trivial assignment crashed at the same native call boundary.

V0.5R then used fail-open validation and did not crash. Its runtime log resolved:

```text
View->vtable[61] = CoherentUIGT + 0x82870
```

which matches the statically reconstructed public wrapper ABI.

## Corrected implementation rule

Future builds must:

1. start from the V0.4 native factory/lifecycle base;
2. hook HUD page **slot 28**, not 27;
3. call the original slot 28 callback first;
4. obtain the public wrapper View through exported `ui::Page::getView()`;
5. verify the wrapper vtable is the expected current-build public vtable at `CoherentUIGT + 0x270780`;
6. resolve `ExecuteScript` through `View->vtable[61]` and verify it equals `CoherentUIGT + 0x82870`;
7. never call the internal `+0xDBD00` implementation with the public wrapper pointer;
8. restore the temporary ready hook **before** executing custom script, eliminating re-entrant use of the hook;
9. fail open on any mismatch.

This replaces the obsolete 2020 slot-27 assumption and is the current canonical ABI model for the project.
