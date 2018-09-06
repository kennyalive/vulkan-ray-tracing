@echo off

set spirv_dir=.\..\..\data\spirv

if not exist %spirv_dir% (
    mkdir %spirv_dir%
)

set dxc=.\..\..\third_party\dxc\dxc.exe

for %%f in (*.hlsl) do (
    %dxc% -spirv -T vs_6_0 -E main_vs -Fo %spirv_dir%\%%~nf.vb %%f
    %dxc% -spirv -T ps_6_0 -E main_fs -Fo %spirv_dir%\%%~nf.fb %%f
)
