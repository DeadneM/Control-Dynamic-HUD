$ErrorActionPreference = "Stop"

$clang = Get-Command clang-cl -ErrorAction SilentlyContinue
$lld   = Get-Command lld-link -ErrorAction SilentlyContinue

if (-not $clang -or -not $lld) {
    throw "clang-cl and lld-link are required (LLVM/llvm-mingw)."
}

New-Item -ItemType Directory -Force -Path build-out | Out-Null

& clang-cl /nologo /c /O2 /GS- /GR- /EHs-c- /Zl /DWIN32 /D_WINDOWS /Fo:build-out/ControlDynamicHUD.obj src/ControlDynamicHUD.cpp
if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $LASTEXITCODE" }
& lld-link /dll /entry:DllMain /subsystem:windows /machine:x64 /nodefaultlib /dynamicbase /nxcompat /out:build-out/ControlDynamicHUD.dll build-out/ControlDynamicHUD.obj
if ($LASTEXITCODE -ne 0) { throw "Link failed: $LASTEXITCODE" }

Write-Host "Built build-out/ControlDynamicHUD.dll"
