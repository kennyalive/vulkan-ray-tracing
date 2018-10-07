@echo off

set spirv_dir=.\..\..\data\spirv

if not exist %spirv_dir% (
    mkdir %spirv_dir%
)

set dxc=.\..\..\third_party\dxc\dxc.exe

for %%f in (hlsl\*.vert) do (
    %dxc% -spirv -T vs_6_0 -Zpr -Fo %spirv_dir%\%%~nf.vert.spv %%f
)

for %%f in (hlsl\*.frag) do (
    %dxc% -spirv -T ps_6_0 -Zpr -Fo %spirv_dir%\%%~nf.frag.spv %%f
)

for %%f in (hlsl\*.comp) do (
    %dxc% -spirv -T cs_6_0 -Zpr -Fo %spirv_dir%\%%~nf.comp.spv %%f
)
