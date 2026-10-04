@echo off
REM ============================================================
REM  Ejecucion distribuida: 3 computadoras, 5 procesos MPI
REM    192.168.137.1   CarlosHP          1 proceso  (rank 0, maestro)
REM    192.168.137.165 DESKTOP-NRB8997   2 procesos (ranks 1 y 2)
REM    192.168.137.153 DESKTOP-C9F9T9F   2 procesos (ranks 3 y 4)
REM  Requisitos: smpd activo en cada equipo (smpd -d) y el ejecutable
REM  en la misma ruta en las 3 computadoras.
REM ============================================================
mpiexec -hosts 3 192.168.137.1 1 192.168.137.165 2 192.168.137.153 2 C:\MPI\Proyecto_MPI\practica1_3.exe
