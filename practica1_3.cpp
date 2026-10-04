/*
    PRACTICA 1.3: REDUCE EN MPI CON OPENMP

    INTEGRANTES (primer apellido, orden alfabetico):
    Hermosillo Prado Carlos
    Montes de Oca del Risco Rafael Alejandro
    Ramirez Andrade Uriel Ismael Guadalupe

    COMO ESTA ORGANIZADO ESTE ARCHIVO:
      1. Funciones de apoyo para imprimir y guardar en el log
      2. Clase OperacionesArreglos (llenado, operaciones, reducciones, comunicacion)
      3. main(): menu de 12 opciones

    Las 3 versiones de comunicacion que pide la practica:
      Version 1: MPI_Send / MPI_Recv        -> opciones 2 a 5, version 2
      Version 2: MPI_Scatter / MPI_Gather   -> opciones 2 a 5, version 3
      Version 3: MPI_Reduce / MPI_Allreduce -> opciones 8 a 11
*/

#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <thread>
#include <chrono>

using namespace std;

// ===========================================================================
// 1. FUNCIONES DE APOYO
// ===========================================================================

ofstream archivoLog;      // archivo log de ESTE nodo
string nombrePC;          // nombre del equipo
int miRank = 0;           // numero de proceso MPI de este nodo

const char* LINEA_EQUIPO =
"EQUIPO: Hermosillo Prado Carlos | Montes de Oca del Risco Rafael Alejandro | Ramirez Andrade Uriel Ismael Guadalupe";

// Escribe SIEMPRE en el log y, si pantalla es true, tambien en la consola
void imprimir(const string& texto, bool pantalla = true) {
    archivoLog << texto << endl;
    if (pantalla) cout << texto << endl;
}

// Convierte un numero a texto: enteros sin decimales, los demas con 2 decimales
string num(double v) {
    char buffer[64];
    if (fabs(v) < 1e15 && v == floor(v)) sprintf(buffer, "%lld", (long long)v);
    else sprintf(buffer, "%.2f", v);
    return string(buffer);
}

// Convierte un tiempo en segundos a texto con 6 decimales
string tiempoTexto(double t) {
    char buffer[64];
    sprintf(buffer, "%.6f", t);
    return string(buffer);
}

// Arma el mensaje con el formato de la practica:
// [Equipo] [Proceso MPI] [Hilo OpenMP] [Posicion] [Operacion] [Valor]
// (si hilo es -1 no se muestra el hilo; si posicion o valor estan vacios, tampoco)
string armar(int hilo, const string& posicion, const string& operacion, const string& valor) {
    string s = "[Equipo: " + nombrePC + "] [Proceso MPI: " + to_string(miRank) + "]";
    if (hilo >= 0) s += " [Hilo OpenMP: " + to_string(hilo) + "]";
    if (posicion != "") s += " [Posicion: " + posicion + "]";
    s += " [Operacion: " + operacion + "]";
    if (valor != "") s += " [Valor: " + valor + "]";
    return s;
}

// Imprime desde un hilo de OpenMP (critical = un hilo a la vez, para no mezclar lineas)
void imprimirHilo(long long posicion, const string& operacion, const string& valor) {
    string s = armar(omp_get_thread_num(), to_string(posicion), operacion, valor);
#pragma omp critical
    {
        imprimir(s, true);
    }
}

// ===========================================================================
// 2. CLASE OperacionesArreglos
// ===========================================================================
class OperacionesArreglos {
public:
    // Seccion LOCAL de cada proceso (memoria dinamica con punteros)
    double* A;
    double* B;
    double* C;

    // Arreglos GLOBALES: solo existen en el maestro (nodo 0)
    double* globalA;
    double* globalB;
    double* globalC;

    int N;            // tamano total del arreglo
    int local_n;      // cuantos elementos le tocan a este proceso
    int inicio;       // posicion global donde empieza mi seccion
    int size;         // total de procesos MPI

    bool datosListos;  // ya se llenaron A y B
    bool aleatorio;    // el ultimo llenado fue aleatorio
    bool semilla;      // ya se llamo a srand

    OperacionesArreglos() {
        A = B = C = nullptr;
        globalA = globalB = globalC = nullptr;
        N = local_n = inicio = 0;
        size = 1;
        datosListos = false;
        aleatorio = false;
        semilla = false;
    }

    ~OperacionesArreglos() { liberar(); }

    void liberar() {
        delete[] A; A = nullptr;
        delete[] B; B = nullptr;
        delete[] C; C = nullptr;
        delete[] globalA; globalA = nullptr;
        delete[] globalB; globalB = nullptr;
        delete[] globalC; globalC = nullptr;
    }

    string rango() {
        return to_string(inicio) + "-" + to_string(inicio + local_n - 1);
    }

    // Cada nodo inicia su semilla con el tiempo + su rank (una sola vez)
    void iniciarSemilla() {
        if (!semilla) {
            srand((unsigned int)time(NULL) + miRank * 1000);
            semilla = true;
        }
    }

    // -----------------------------------------------------------------------
    // Opcion 1: crear arreglos (N debe ser multiplo del numero de procesos)
    // -----------------------------------------------------------------------
    void crearArregloMPI(int n, int totalProcesos, bool detalle) {
        liberar();
        N = n;
        size = totalProcesos;
        local_n = N / size;            // cada proceso recibe la misma cantidad
        inicio = miRank * local_n;

        A = new double[local_n];
        B = new double[local_n];
        C = new double[local_n];
        datosListos = false;

        imprimir(armar(-1, rango(), "Crear Arreglos", to_string(local_n) + " elementos"),
            detalle || miRank == 0);
    }

    // -----------------------------------------------------------------------
    // Opcion 6: llenar secuencial (A = 1,2,3...  B = 2,4,6...)
    // -----------------------------------------------------------------------
    void llenarSecuencial(bool detalle) {
        aleatorio = false;
#pragma omp parallel for
        for (int i = 0; i < local_n; i++) {
            long long g = inicio + i;                 // posicion global
            A[i] = (double)(g + 1);
            B[i] = (double)(g + 1) * 2.0;
            if (detalle) imprimirHilo(g, "Llenado Secuencial", "A=" + num(A[i]) + ", B=" + num(B[i]));
        }
        datosListos = true;
    }

    // -----------------------------------------------------------------------
    // Opcion 7: llenar aleatorio (1 a 1,000,000)
    // -----------------------------------------------------------------------
    void llenarAleatorio(bool detalle) {
        iniciarSemilla();
        aleatorio = true;
        unsigned long long base = (unsigned long long)rand() * 32768ULL + rand();

#pragma omp parallel
        {
            // rand() no sirve bien con varios hilos, asi que cada hilo
            // usa su propia variable x, calculada a partir de la semilla del nodo
            unsigned long long x = base + 7919ULL * omp_get_thread_num() + 1;
#pragma omp for
            for (int i = 0; i < local_n; i++) {
                long long g = inicio + i;
                x = x * 6364136223846793005ULL + 1442695040888963407ULL;
                A[i] = (double)((x >> 33) % 1000000 + 1);
                x = x * 6364136223846793005ULL + 1442695040888963407ULL;
                B[i] = (double)((x >> 33) % 1000000 + 1);
                if (detalle) imprimirHilo(g, "Llenado Aleatorio", "A=" + num(A[i]) + ", B=" + num(B[i]));
            }
        }
        datosListos = true;
    }

    // -----------------------------------------------------------------------
    // Opciones 2 a 5: operaciones elemento a elemento
    // -----------------------------------------------------------------------
    void sumar(bool detalle) {
#pragma omp parallel for
        for (int i = 0; i < local_n; i++) {
            C[i] = A[i] + B[i];
            if (detalle) imprimirHilo(inicio + i, "Suma", "C=" + num(C[i]));
        }
    }

    void restar(bool detalle) {
#pragma omp parallel for
        for (int i = 0; i < local_n; i++) {
            C[i] = A[i] - B[i];
            if (detalle) imprimirHilo(inicio + i, "Resta", "C=" + num(C[i]));
        }
    }

    void multiplicar(bool detalle) {
#pragma omp parallel for
        for (int i = 0; i < local_n; i++) {
            C[i] = A[i] * B[i];
            if (detalle) imprimirHilo(inicio + i, "Multiplicacion", "C=" + num(C[i]));
        }
    }

    void cuadrado(bool detalle) {
#pragma omp parallel for
        for (int i = 0; i < local_n; i++) {
            C[i] = A[i] * A[i];
            if (detalle) imprimirHilo(inicio + i, "Cuadrado", "C=" + num(C[i]));
        }
    }

    // -----------------------------------------------------------------------
    // Opciones 8 a 11: reducciones en 2 niveles
    //   Nivel 1: OpenMP (dentro del nodo)
    //   Nivel 2: MPI_Reduce o MPI_Allreduce (entre nodos)
    // -----------------------------------------------------------------------
    double sumatoria(bool detalle, bool allreduce) {
        double suma_local = 0.0;

#pragma omp parallel for reduction(+:suma_local)
        for (int i = 0; i < local_n; i++) {
            suma_local += A[i];
            if (detalle) imprimirHilo(inicio + i, "Sumatoria Local (suma A[i])", num(A[i]));
        }

        if (detalle) imprimir(armar(omp_get_thread_num(), rango(), "Sumatoria Local del nodo", num(suma_local)), true);

        double suma_global = 0.0;
        if (allreduce) MPI_Allreduce(&suma_local, &suma_global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        else           MPI_Reduce(&suma_local, &suma_global, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        return suma_global;
    }

    double maximo(bool detalle, bool allreduce) {
        double max_local = -1e300;

#if defined(_OPENMP) && _OPENMP >= 201107
        // Compiladores nuevos: se usa reduction(max:)
#pragma omp parallel for reduction(max:max_local)
        for (int i = 0; i < local_n; i++) {
            if (A[i] > max_local) max_local = A[i];
            if (detalle) imprimirHilo(inicio + i, "Maximo Local (compara A[i])", num(A[i]));
        }
#else
        // Visual Studio (OpenMP 2.0) no tiene reduction(max:): cada hilo busca su maximo
        // y luego se combinan con critical
#pragma omp parallel
        {
            double max_hilo = -1e300;
#pragma omp for
            for (int i = 0; i < local_n; i++) {
                if (A[i] > max_hilo) max_hilo = A[i];
                if (detalle) imprimirHilo(inicio + i, "Maximo Local (compara A[i])", num(A[i]));
            }
#pragma omp critical
            {
                if (max_hilo > max_local) max_local = max_hilo;
            }
        }
#endif

        if (detalle) imprimir(armar(omp_get_thread_num(), rango(), "Maximo Local del nodo", num(max_local)), true);

        double max_global = 0.0;
        if (allreduce) MPI_Allreduce(&max_local, &max_global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        else           MPI_Reduce(&max_local, &max_global, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        return max_global;
    }

    double minimo(bool detalle, bool allreduce) {
        double min_local = 1e300;

#if defined(_OPENMP) && _OPENMP >= 201107
#pragma omp parallel for reduction(min:min_local)
        for (int i = 0; i < local_n; i++) {
            if (A[i] < min_local) min_local = A[i];
            if (detalle) imprimirHilo(inicio + i, "Minimo Local (compara A[i])", num(A[i]));
        }
#else
#pragma omp parallel
        {
            double min_hilo = 1e300;
#pragma omp for
            for (int i = 0; i < local_n; i++) {
                if (A[i] < min_hilo) min_hilo = A[i];
                if (detalle) imprimirHilo(inicio + i, "Minimo Local (compara A[i])", num(A[i]));
            }
#pragma omp critical
            {
                if (min_hilo < min_local) min_local = min_hilo;
            }
        }
#endif

        if (detalle) imprimir(armar(omp_get_thread_num(), rango(), "Minimo Local del nodo", num(min_local)), true);

        double min_global = 0.0;
        if (allreduce) MPI_Allreduce(&min_local, &min_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        else           MPI_Reduce(&min_local, &min_global, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
        return min_global;
    }

    // -----------------------------------------------------------------------
    // Esquema maestro-esclavo (versiones 1 y 2 de comunicacion)
    // El maestro crea los arreglos globales, los reparte, y despues recoge C.
    // -----------------------------------------------------------------------

    // Solo el maestro: genera A y B completos
    void generarGlobal() {
        if (globalA == nullptr) {
            globalA = new double[N];
            globalB = new double[N];
            globalC = new double[N];
        }
        if (aleatorio) {
            iniciarSemilla();
            unsigned long long x = (unsigned long long)rand() * 32768ULL + rand() + 1;
            for (int i = 0; i < N; i++) {
                x = x * 6364136223846793005ULL + 1442695040888963407ULL;
                globalA[i] = (double)((x >> 33) % 1000000 + 1);
                x = x * 6364136223846793005ULL + 1442695040888963407ULL;
                globalB[i] = (double)((x >> 33) % 1000000 + 1);
            }
        }
        else {
#pragma omp parallel for
            for (int i = 0; i < N; i++) {
                globalA[i] = (double)i + 1.0;
                globalB[i] = ((double)i + 1.0) * 2.0;
            }
        }
        imprimir(armar(-1, "0-" + to_string(N - 1), "Maestro genera A y B globales",
            aleatorio ? "aleatorio" : "secuencial"), true);
    }

    // VERSION 1: MPI_Send / MPI_Recv ---------------------------------------
    double distribuirSendRecv(bool detalle) {
        MPI_Barrier(MPI_COMM_WORLD);
        double t0 = MPI_Wtime();

        if (miRank == 0) {
            for (int i = 0; i < local_n; i++) { A[i] = globalA[i]; B[i] = globalB[i]; }  // su propio bloque
            for (int r = 1; r < size; r++) {                                              // bloques de los demas
                MPI_Send(&globalA[r * local_n], local_n, MPI_DOUBLE, r, 1, MPI_COMM_WORLD);
                MPI_Send(&globalB[r * local_n], local_n, MPI_DOUBLE, r, 2, MPI_COMM_WORLD);
                imprimir(armar(-1, to_string(r * local_n) + "-" + to_string((r + 1) * local_n - 1),
                    "MPI_Send bloque A y B hacia nodo " + to_string(r), to_string(local_n) + " elementos"), true);
            }
        }
        else {
            MPI_Recv(A, local_n, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(B, local_n, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            imprimir(armar(-1, rango(), "MPI_Recv bloque A y B desde nodo 0", to_string(local_n) + " elementos"), detalle);
        }

        MPI_Barrier(MPI_COMM_WORLD);
        datosListos = true;
        return MPI_Wtime() - t0;
    }

    double recopilarSendRecv(bool detalle) {
        MPI_Barrier(MPI_COMM_WORLD);
        double t0 = MPI_Wtime();

        if (miRank == 0) {
            for (int i = 0; i < local_n; i++) globalC[i] = C[i];
            for (int r = 1; r < size; r++) {
                MPI_Recv(&globalC[r * local_n], local_n, MPI_DOUBLE, r, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                imprimir(armar(-1, to_string(r * local_n) + "-" + to_string((r + 1) * local_n - 1),
                    "MPI_Recv parcial de C desde nodo " + to_string(r), to_string(local_n) + " elementos"), true);
            }
        }
        else {
            MPI_Send(C, local_n, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);
            imprimir(armar(-1, rango(), "MPI_Send parcial de C hacia nodo 0", to_string(local_n) + " elementos"), detalle);
        }

        MPI_Barrier(MPI_COMM_WORLD);
        return MPI_Wtime() - t0;
    }

    // VERSION 2: MPI_Scatter / MPI_Gather ----------------------------------
    double distribuirScatter(bool detalle) {
        MPI_Barrier(MPI_COMM_WORLD);
        double t0 = MPI_Wtime();

        MPI_Scatter(globalA, local_n, MPI_DOUBLE, A, local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Scatter(globalB, local_n, MPI_DOUBLE, B, local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
        datosListos = true;
        imprimir(armar(-1, rango(), "MPI_Scatter recibe bloque A y B", to_string(local_n) + " elementos"),
            detalle || miRank == 0);
        return MPI_Wtime() - t0;
    }

    double recopilarGather(bool detalle) {
        MPI_Barrier(MPI_COMM_WORLD);
        double t0 = MPI_Wtime();

        MPI_Gather(C, local_n, MPI_DOUBLE, globalC, local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
        imprimir(armar(-1, rango(), "MPI_Gather envia parcial de C", to_string(local_n) + " elementos"),
            detalle || miRank == 0);
        return MPI_Wtime() - t0;
    }

    // Solo el maestro: muestra el resultado C completo que recibio
    void mostrarGlobalC(bool detalle) {
        double suma = 0.0;
        for (int i = 0; i < N; i++) {
            suma += globalC[i];
            if (detalle) imprimir(armar(0, to_string(i), "C global recibido en maestro", num(globalC[i])), true);
        }
        imprimir(armar(-1, "0-" + to_string(N - 1), "Suma de verificacion de C global", num(suma)), true);
    }
};

// ===========================================================================
// 3. PROGRAMA PRINCIPAL
// ===========================================================================

string nombreOperacion(int opcion) {
    if (opcion == 2) return "Suma";
    if (opcion == 3) return "Resta";
    if (opcion == 4) return "Multiplicacion";
    if (opcion == 5) return "Cuadrado";
    if (opcion == 6) return "Llenar Secuencial";
    if (opcion == 7) return "Llenar Aleatorio";
    if (opcion == 8) return "Sumatoria";
    if (opcion == 9) return "Promedio";
    if (opcion == 10) return "Maximo";
    return "Minimo";
}

// Muestra un texto sin salto de linea y lo guarda en el log (para las preguntas)
void preguntar(const string& texto) {
    archivoLog << texto << endl;
    cout << texto << flush;
}

int main(int argc, char* argv[]) {
    // ---- Iniciar MPI ----
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int size;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    int largo = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &miRank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(hostname, &largo);
    nombrePC = string(hostname);

    // ---- Cada nodo crea su archivo log local ----
    string nombreArchivo = "log_equipo_" + nombrePC + "_nodo_" + to_string(miRank) + ".txt";
    archivoLog.open(nombreArchivo.c_str());

    // ---- PRIMERA LINEA: nombres del equipo ----
    if (miRank == 0) {
        imprimir(LINEA_EQUIPO);
        imprimir("==============================================");
        imprimir("PRACTICA 1.3: REDUCE EN MPI CON OPENMP");
        imprimir("Procesos MPI: " + to_string(size) + " | Hilos OpenMP por proceso: " + to_string(omp_get_max_threads()));
        imprimir("==============================================");
    }
    MPI_Barrier(MPI_COMM_WORLD);

    OperacionesArreglos op;
    int N = 40;
    bool detalle = true;       // true cuando N <= 40 (muestra todo elemento por elemento)

    op.crearArregloMPI(N, size, true);
    MPI_Barrier(MPI_COMM_WORLD);

    int opcion = 0;

    while (true) {
        int version = 1;       // version de comunicacion (opciones 2 a 5)
        int allreduce = 0;     // 0 = MPI_Reduce, 1 = MPI_Allreduce (opciones 8 a 11)

        // ---- El maestro muestra el menu y lee la opcion ----
        if (miRank == 0) {
            imprimir("\n==========================================\n"
                "        MENU DE OPERACIONES MPI + OPENMP\n"
                "==========================================\n"
                "1. Crear arreglos\n"
                "2. Sumar arreglos\n"
                "3. Restar arreglos\n"
                "4. Multiplicar arreglos\n"
                "5. Calcular el cuadrado de un arreglo\n"
                "6. Llenar secuencial\n"
                "7. Llenar aleatorio\n"
                "8. Sumatoria\n"
                "9. Promedio\n"
                "10. Maximo\n"
                "11. Minimo\n"
                "12. Salir");
            preguntar("Seleccione una opcion: ");
            if (!(cin >> opcion)) opcion = 12;    // si falla la lectura, salir

            if (opcion >= 2 && opcion <= 5) {
                imprimir("\nVersion de comunicacion:\n"
                    "  1) Local (usa A y B locales ya llenados)\n"
                    "  2) Punto a punto (MPI_Send / MPI_Recv)\n"
                    "  3) Colectiva (MPI_Scatter / MPI_Gather)");
                preguntar("Seleccione (1-3): ");
                cin >> version;
                if (version < 1 || version > 3) version = 1;
            }
            if (opcion >= 8 && opcion <= 11) {
                imprimir("\nFuncion de reduccion:\n"
                    "  1) MPI_Reduce (resultado solo en el nodo 0)\n"
                    "  2) MPI_Allreduce (resultado en todos los nodos)");
                preguntar("Seleccione (1 o 2): ");
                int v = 1;
                cin >> v;
                allreduce = (v == 2) ? 1 : 0;
            }
        }

        // ---- El maestro avisa a todos los nodos con MPI_Bcast ----
        int datos[3] = { opcion, version, allreduce };
        MPI_Bcast(datos, 3, MPI_INT, 0, MPI_COMM_WORLD);
        opcion = datos[0];
        version = datos[1];
        allreduce = datos[2];

        if (opcion == 12) break;

        // ---- Opcion 1: crear arreglos ----
        if (opcion == 1) {
            if (miRank == 0) {
                imprimir("\n1) N = 40 (muestra todo)\n2) N = 4,000,000 (solo tiempos)\n3) Otro tamano");
                preguntar("Seleccione (1-3): ");
                int s = 1;
                cin >> s;
                if (s == 2) N = 4000000;
                else if (s == 3) {
                    preguntar("Ingrese N: ");
                    cin >> N;
                }
                else N = 40;

                // N debe ser multiplo del numero de procesos para repartirlo parejo
                if (N < size) N = size;
                if (N % size != 0) {
                    N = N - (N % size);
                    imprimir("N ajustado a " + to_string(N) + " (multiplo de " + to_string(size) + " procesos)");
                }
            }
            MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
            detalle = (N <= 40);
            op.crearArregloMPI(N, size, detalle);
            MPI_Barrier(MPI_COMM_WORLD);
            continue;
        }

        if (opcion < 2 || opcion > 11) {
            if (miRank == 0) imprimir("Opcion invalida.");
            continue;
        }

        // ---- Avisar si faltan datos ----
        bool necesitaDatos = (opcion >= 8) || (opcion <= 5 && version == 1);
        if (necesitaDatos && !op.datosListos) {
            if (miRank == 0) imprimir("Primero llene los arreglos con la opcion 6 o 7 (o use version de comunicacion 2 o 3).");
            continue;
        }

        // ---- Ejecutar la operacion midiendo el tiempo ----
        double tDistribuir = 0.0, tRecopilar = 0.0;

        MPI_Barrier(MPI_COMM_WORLD);
        double t0 = MPI_Wtime();

        if (opcion >= 2 && opcion <= 5) {
            if (version != 1) {
                if (miRank == 0) op.generarGlobal();
                if (version == 2) tDistribuir = op.distribuirSendRecv(detalle);
                else              tDistribuir = op.distribuirScatter(detalle);
            }

            if (opcion == 2) op.sumar(detalle);
            if (opcion == 3) op.restar(detalle);
            if (opcion == 4) op.multiplicar(detalle);
            if (opcion == 5) op.cuadrado(detalle);

            if (version != 1) {
                if (version == 2) tRecopilar = op.recopilarSendRecv(detalle);
                else              tRecopilar = op.recopilarGather(detalle);
                if (miRank == 0) op.mostrarGlobalC(detalle);
            }
        }
        else if (opcion == 6) {
            op.llenarSecuencial(detalle);
        }
        else if (opcion == 7) {
            op.llenarAleatorio(detalle);
        }
        else {
            // Opciones 8 a 11: reducciones
            double resultado = 0.0;
            string nombre = "";
            if (opcion == 8) { resultado = op.sumatoria(detalle, allreduce == 1); nombre = "Sumatoria Global"; }
            if (opcion == 9) { resultado = op.sumatoria(detalle, allreduce == 1) / op.N; nombre = "Promedio Global"; }
            if (opcion == 10) { resultado = op.maximo(detalle, allreduce == 1); nombre = "Maximo Global"; }
            if (opcion == 11) { resultado = op.minimo(detalle, allreduce == 1); nombre = "Minimo Global"; }

            // MPI_Reduce: solo el nodo 0 tiene el resultado. MPI_Allreduce: todos lo tienen.
            if (allreduce == 1)
                imprimir(armar(-1, "", nombre + " (MPI_Allreduce)", num(resultado)), true);
            else if (miRank == 0)
                imprimir(armar(-1, "", nombre + " (MPI_Reduce, solo nodo 0)", num(resultado)), true);
        }

        double tLocal = MPI_Wtime() - t0;
        MPI_Barrier(MPI_COMM_WORLD);
        double tGlobal = MPI_Wtime() - t0;

        // Cada nodo guarda su tiempo local en su propio log
        imprimir(armar(-1, "", "Tiempo local de " + nombreOperacion(opcion), tiempoTexto(tLocal) + " s"), false);

        // El maestro muestra el tiempo global en pantalla
        if (miRank == 0) {
            string detalleOp = nombreOperacion(opcion);
            if (opcion <= 5 && version == 2) detalleOp += " [Send/Recv]";
            if (opcion <= 5 && version == 3) detalleOp += " [Scatter/Gather]";
            if (opcion >= 8) detalleOp += (allreduce == 1) ? " [MPI_Allreduce]" : " [MPI_Reduce]";

            imprimir("[TIEMPO GLOBAL] " + detalleOp + " | N=" + to_string(op.N) +
                " | MPI_Wtime: " + tiempoTexto(tGlobal) + " s");
            if (opcion <= 5 && version != 1) {
                imprimir("[TIEMPO COMUNICACION] Distribucion: " + tiempoTexto(tDistribuir) +
                    " s | Recopilacion: " + tiempoTexto(tRecopilar) +
                    " s | Total: " + tiempoTexto(tDistribuir + tRecopilar) + " s");
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);
        // Pausa corta para que la salida de los otros nodos llegue antes del menu
        if (detalle && miRank == 0) this_thread::sleep_for(chrono::milliseconds(300));
    }

    // ---- Cierre ----
    imprimir(armar(-1, "", "Finalizando nodo", "Equipo " + nombrePC), detalle || miRank == 0);
    MPI_Barrier(MPI_COMM_WORLD);

    if (miRank == 0) {
        this_thread::sleep_for(chrono::milliseconds(300));
        imprimir("==============================================");
        imprimir("COMUNICACION HIBRIDA MPI + OPENMP FINALIZADA");
        imprimir("==============================================");
        imprimir(LINEA_EQUIPO);      // ULTIMA LINEA: nombres del equipo
    }

    archivoLog.close();   // cerrar el log ANTES de MPI_Finalize
    MPI_Finalize();
    return 0;
}