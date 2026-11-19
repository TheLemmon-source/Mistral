# Sesiones de Ser

Un archivo completo por sesion de trabajo: `sesiones/<YYYY-MM-DD-HHMM>/Ser.cpp`
(fecha y hora en UTC). Cada sesion vive en su rama `vibe/sesion-<fecha-hhmm>`;
las ramas viejas se pueden borrar sin perder la vigente: el camino del archivo
es la marca de tiempo, nunca se pisa un contenido.

- Compilar (Windows/MinGW64): `g++ -O2 -std=c++11 sesiones/<carpeta>/Ser.cpp -o ser.exe -luser32 -lgdi32`
- Bateria headless (Linux o MinGW sin ventana): `g++ -O2 -std=c++11 sesiones/<carpeta>/Ser.cpp -o ser && ./ser`

El `src/Ser.cpp` de la rama de trabajo acumulativa (`vibe/meta-ui-filas-creadoras-06e08b`,
PR #1) sigue siendo la linea principal de codigo; estas copias son cortes
inmutables por sesion.
