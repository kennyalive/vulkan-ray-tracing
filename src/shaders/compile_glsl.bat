@echo off

set spirv_dir=.\..\..\data\spirv

if not exist %spirv_dir% (
    mkdir %spirv_dir%
)

set glslang=glslc.exe

for %%f in (glsl\*.rgen) do (
    %glslang% -o %spirv_dir%\%%~nf.rgen.spv %%f
)

for %%f in (glsl\*.rmiss) do (
    %glslang% -O -o %spirv_dir%\%%~nf.miss.spv %%f
)

for %%f in (glsl\*.rchit) do (
    %glslang% -O -o %spirv_dir%\%%~nf.chit.spv %%f
)

for %%f in (glsl\*.rahit) do (
    %glslang% -O -o %spirv_dir%\%%~nf.ahit.spv %%f
)
