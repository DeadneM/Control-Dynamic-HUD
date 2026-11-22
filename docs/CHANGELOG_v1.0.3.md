Changes since v1.0.2:

- Added optional automatic Continue at startup, using the game's normal Continue action when a save is available.
- Added an independent option to skip the initial "Press any key" screen. Enable both options to go directly from launch to your saved game.
- Added configurable delays for both startup options (0-10000 ms). All four Startup settings default to 0, preserving manual startup.
- Startup actions run only once per game session. Returning to the menu remains manual; profile checks and consent dialogs remain under the game's control. Menu input cancels automatic Continue.
- Release ZIPs now contain only plugins/ControlDynamicHUD.dll, preserving your existing INI when updating. The optional INI is available as a separate download. Without it, built-in defaults are used.

The validated dynamic HUD, F1 Show HUD, crosshair dot, Launch/MultiLaunch controls, menu fixes and save-protection options are preserved. HideNewGame, HideMissionSelect and ShowDiagnostics remain disabled by default.

Startup flow tested and approved by the project owner. One x64 plugin supports DX11 and DX12; the Control Plugin Loader is required.