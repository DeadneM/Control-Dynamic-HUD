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

The concrete View vtable used by this `coherentuigt.dll` is assigned at RVA `0x281028`.

Verified entries:

- slot 13 (`+0x68`) -> RVA `0xD9A90`, the scripted-animation service used by modern Control UI mods as a Coherent-thread execution point.
- slot 61 (`+0x1E8`) -> RVA `0xDBD00`, `Coherent::UIGT::View::ExecuteScript(const char*, const char*)`.

The function body at `0xDBD00` confirms the x64 argument layout:
- RCX = View `this`
- RDX = script C string
- R8 = frame selector

Although the absolute RVA is correct for this exact DLL, Control Dynamic HUD should call the method through `View->vtable[61]` and verify that the resolved pointer belongs to the known Coherent module/build.

## ui::Page::getView

The current export:

```text
?getView@Page@ui@@QEAAPEAVView@UIGT@Coherent@@XZ
RVA 0x2980
```

reads the page index at `Page+0x18` and returns the corresponding native View pointer from the current UI system's page table.

## V0.5 / V0.5A crash explanation

Both failed builds successfully reached a valid HUD object, valid `ui::Page::getView()`, valid native View pointer and non-null internal View page.

They then called `ExecuteScript` while hooked from slot 27, which on the current ABI is the page update path, not `onReadyForBindings`.

The JavaScript body is ruled out because V0.5A used only a trivial assignment and crashed at the same native call boundary.

## Corrected implementation rule

Future builds must:

1. start from the V0.4 native factory/lifecycle base;
2. hook HUD page **slot 28**, not 27;
3. call the original slot 28 callback first;
4. obtain the View through exported `ui::Page::getView()`;
5. resolve `ExecuteScript` from `View->vtable[61]`;
6. validate the Coherent build/pointer;
7. restore the temporary ready hook **before** executing custom script, eliminating re-entrant use of the hook;
8. fail open on any mismatch.

This replaces the obsolete 2020 slot-27 assumption and is the current canonical ABI model for the project.
