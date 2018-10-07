@echo off

set spirv_dir=.\..\..\data\spirv

if not exist %spirv_dir% (
    mkdir %spirv_dir%
)

set glslang=glslangValidator.exe

for %%f in (glsl\*.rgen) do (
    %glslang% -V -o %spirv_dir%\%%~nf.rgen.spv %%f
)
