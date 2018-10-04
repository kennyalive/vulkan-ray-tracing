@echo off

set spirv_dir=.\..\..\data\spirv

if not exist %spirv_dir% (
    mkdir %spirv_dir%
)

set glslang=.\..\..\third_party\glslang\glslangValidator.exe

for %%f in (*.rgen) do (
    %glslang% -V -o %spirv_dir%\%%~nf.rgenb %%f
)
