WHERE dxc
IF %ERRORLEVEL% NEQ 0 ECHO DirectX Shader Compiler was not found. Consider installing it for shader compilation.

dxc -spirv -T vs_6_6 -E VsMain Triangle.hlsl -Fo TriangleVS.cso
dxc -spirv -T ps_6_6 -E PsMain Triangle.hlsl -Fo TrianglePS.cso