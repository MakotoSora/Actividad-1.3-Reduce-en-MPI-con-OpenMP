# Práctica 1.3: Reduce en MPI con OpenMP

Programación Paralela · Ingeniería en Ciencias Computacionales · Universidad de Guadalajara · Equipo 11

**Integrantes** (por primer apellido, orden alfabético)

1. Hermosillo Prado Carlos
2. Montes de Oca del Risco Rafael Alejandro
3. Ramírez Andrade Uriel Ismael Guadalupe

## Descripción

Operaciones aritméticas elemento a elemento y reducciones colectivas sobre arreglos dinámicos distribuidos con **MPI** (MS-MPI) y procesados con **OpenMP** dentro de cada nodo. Cada nodo guarda todo lo que imprime en su propio archivo de log.

- Clase `OperacionesArreglos` con arreglos dinámicos por punteros (`new[]`), sin `std::vector`.
- Menú interactivo de 12 opciones difundido con `MPI_Bcast`.
- Tres versiones de comunicación:
  - **Versión 1:** `MPI_Send` / `MPI_Recv` (opciones 2–5, versión 2).
  - **Versión 2:** `MPI_Scatter` / `MPI_Gather` (opciones 2–5, versión 3).
  - **Versión 3:** `MPI_Reduce` / `MPI_Allreduce` (opciones 8–11).
- Reducción en dos niveles: OpenMP `reduction` dentro del nodo y MPI entre nodos.
- Tiempos medidos con `MPI_Wtime`.
- Log por nodo: `log_equipo_[NOMBRE_PC]_nodo_[RANK].txt`, cerrado antes de `MPI_Finalize()`.

## Archivos

| Archivo | Contenido |
| --- | --- |
| `practica1_3.cpp` | Código fuente completo |
| `compilar.bat` | Compilación con `cl` + MS-MPI |
| `ejecutar_local.bat` | 5 procesos en una computadora |
| `ejecutar_red.bat` | 5 procesos en 3 computadoras por IP |
| `salida programa 1.3.txt` | Salida completa de la ejecución distribuida |
| `log_equipo_CarlosHP_nodo_0..4.txt` | Logs de la ejecución local |
| `log_equipo_DESKTOP-*_nodo_*.txt` | Logs de los trabajadores en la ejecución distribuida |

## Compilación

Desde el *x64 Native/Cross Tools Command Prompt for VS 2022*, con el SDK de MS-MPI instalado:

```
compilar.bat
```

## Ejecución

```
ejecutar_local.bat     :: mpiexec -n 5 practica1_3.exe
ejecutar_red.bat       :: mpiexec -hosts 3 192.168.137.1 1 192.168.137.165 2 192.168.137.153 2 ...
```

Secuencia sugerida: `1` crear arreglos (40 o 4,000,000) → `6` o `7` llenar → `2–5` operaciones → `8–11` reducciones → `12` salir.

## Menú

1. Crear arreglos
2. Sumar arreglos
3. Restar arreglos
4. Multiplicar arreglos
5. Calcular el cuadrado de un arreglo
6. Llenar secuencial
7. Llenar aleatorio
8. Sumatoria
9. Promedio
10. Máximo
11. Mínimo
12. Salir
