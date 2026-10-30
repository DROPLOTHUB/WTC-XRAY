# Wallet Takes Control (WTC) — v5.16 A2A Movimiento Relativo

Proyecto C++ para Windows con interfaz Win32/Direct3D 11 y Dear ImGui. Esta version utiliza exclusivamente offsets incorporados en el codigo: no descarga actualizaciones, no solicita claves ni carga caches remotas.

Usalo unicamente en entornos autorizados y conforme a las leyes y condiciones de uso aplicables. El software se proporciona sin garantias; consulta [LICENSE](<LICENSE>).

## Requisitos y compilacion

- Windows x64.
- Visual Studio o Build Tools con Desarrollo para el escritorio con C++, MSVC v145 y Windows SDK 10.0.
- Dependencias DMA externas segun [la guia DMA](<WTC/lib/dma/README.md>). No se descargan automaticamente.

Abre [sdk.sln](<WTC/sdk.sln>) y compila Release | x64, o ejecuta desde una consola de desarrollador:

```powershell
msbuild WTC\sdk.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

La salida esta en `WTC/build/x64/Release/`. Instala las dependencias de ejecucion junto al ejecutable. Los binarios, bibliotecas externas y datos personales no deben subirse al repositorio.

## Offsets locales

Los valores estan en [offsets.hpp](<WTC/src/game/offsets.hpp>). Para actualizarlos, edita esa tabla y recompila. Recargar SDK reconecta usando los mismos valores compilados; no consulta archivos externos de offsets.

Al conectar se validan los punteros de juego, jugador local, balistica y camara. Si son incompatibles, la inicializacion falla y muestra un diagnostico. El valor `tactical_map_obj_offset` no aparece en la ultima tabla suministrada: conserva el valor anterior y requiere confirmacion antes de dar por validado el radar. La presencia de un offset en la tabla no acredita compatibilidad con la version actual del juego; las lecturas conservan sus comprobaciones de punteros, rangos y valores finitos.

## Uso y configuracion

- F1 abre/oculta el menu; F2 cambia el perfil.
- Los perfiles se cargan desde `%LOCALAPPDATA%\WTC\wtc_menu.cfg`, con alternativa junto al ejecutable. Los cambios solo se guardan al pulsar Guardar; no hay autoguardado de preferencias.
- Los nombres de misiles observados se registran automaticamente en `missiles/registry.txt` junto al ejecutable. No publicar registros ni perfiles personales.
- El renderizador utiliza Arial Bold de Windows; no se redistribuye la fuente.
- ESP, radar, X-Ray y deteccion experimental de proyectiles requieren datos compatibles y hardware DMA. Compilar no demuestra funcionamiento en partida.

## A2A Movimiento Relativo

La prediccion utiliza movimiento relativo, velocidad del avion al disparar y velocidad real de municion. El solver integra gravedad Y-up y resistencia cuadratica; no introduce una velocidad de boca ficticia. Las convenciones de masa, calibre y drag requieren validacion contra el juego.

La velocidad y aceleracion nativas se leen mediante la tabla local. Las muestras invalidas se descartan; si falta aceleracion se deriva de velocidades, y si falta velocidad nativa se usa el historial de posiciones. Los campos contiguos conservan lectura agrupada y recuperacion individual. El menu muestra el origen de las muestras y el residual numerico, que no es un error de impacto medido.

Turn Prediction (3D) esta activado por defecto en perfiles nuevos. Para comparar en partida, empezar con Extra Lead Time = 0 y mantener avion, municion y escena. El diagnostico por arma permite revisar lecturas fallidas sin inventar offsets ni correspondencias de torretas.

## Pruebas

El ejecutor [run.ps1](<WTC/tests/run.ps1>) compila y ejecuta las pruebas offline con MSVC:

```powershell
./WTC/tests/run.ps1
```

Cubren validacion de lecturas, movimiento y balistica sinteticos, snapshots, perfiles, menu y politicas de render. No requieren conectarse al juego. No certifican precision de impacto, compatibilidad de offsets ni funcionamiento con hardware real.

## Validacion de esta revision

Compilacion Release x64 correcta y 39 suites offline aprobadas. El ejecutable no importa funciones WinHTTP ni las funciones de credenciales y cifrado usadas por el antiguo actualizador. Persisten advertencias de compilacion en dependencias y fixtures; no se ha probado en partida ni con hardware DMA real.

## Publicacion

Publica los fuentes y las licencias, no salidas de compilacion, caches, perfiles ni copias de respaldo. La carpeta local `v5.16 A2A Movimiento Relativo1/` esta excluida por [gitignore](<.gitignore>); no la incluyas al crear un ZIP manualmente.

## Licencias

- Proyecto: [MIT, copyright 2026 Marazm](<LICENSE>).
- Dear ImGui: [licencia y atribuciones](<WTC/lib/imgui/LICENSE.txt>).
- MemProcFS, LeechCore y FTDI tienen condiciones propias; consulta [la guia DMA](<WTC/lib/dma/README.md>) antes de redistribuir dependencias o programas enlazados.
