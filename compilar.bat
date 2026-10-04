@echo off
REM ============================================================
REM  Practica 1.3: Reduce en MPI con OpenMP - Compilacion
REM  Ejecutar desde "x64 Native/Cross Tools Command Prompt for VS 2022"
REM  Requiere MS-MPI SDK instalado en la ruta por defecto.
REM ============================================================

set MPI_INC=C:\Program Files (x86)\Microsoft SDKs\MPI\Include
set MPI_LIB=C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64

cl /EHsc /O2 /openmp /I"%MPI_INC%" practica1_3.cpp /link /LIBPATH:"%MPI_LIB%" msmpi.lib /out:practica1_3.exe

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] La compilacion fallo.
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] practica1_3.exe generado.
REM Para usar reduction(max:)/reduction(min:) de OpenMP 3.1, cambiar /openmp por /openmp:llvm
