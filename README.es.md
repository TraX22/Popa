# Popa

**Un lanzador de aplicaciones chico y privado para Windows 11.** Apretás
**Win+Z** y se abre, donde está el puntero del mouse, un menú con tus
programas.

[Read in English](README.md)

- **Un solo archivo, sin instalación.** `Popa.exe` pesa unos 0,6 MB y no
  necesita ningún runtime. La configuración vive a su lado.
- **Solo lo que cargás vos.** Popa nunca escanea el disco ni detecta
  programas instalados.
- **Privado.** Sin telemetría ni estadísticas. La única conexión que hace es
  la búsqueda de actualizaciones que se explica más abajo.
- **Aspecto Windows 11.** Menú nativo con esquinas redondeadas, iconos de
  16 a 48 px, tema claro y oscuro, nítido en pantallas con escalado.
- **Código abierto** (MIT), escrito en C++ con la API de Windows pura. El
  código está comentado para que sirva para aprender C++ y Win32.

## Descarga

Bajá `Popa.exe` de la [última versión](https://github.com/TraX22/Popa/releases/latest),
ponelo en la carpeta que quieras y ejecutalo. Popa muestra un icono junto
al reloj.

La primera vez, Windows SmartScreen puede advertir que el editor es
desconocido, porque el programa no tiene firma digital. Elegí **Más
información → Ejecutar de todas formas**. Cada versión publica la huella
SHA-256 de `Popa.exe` por si querés verificar la descarga.

## Uso

| Acción | Resultado |
|---|---|
| **Win+Z** (configurable) | Abre el menú en el puntero del mouse |
| Clic en el icono de la bandeja | Abre el menú |
| Doble clic en el icono | Abre el editor |
| Clic derecho en el icono | Editor, Opciones, Buscar actualizaciones, activar/desactivar el atajo, Salir |

En el menú podés usar el mouse o el teclado: flechas, Enter, Esc y la
primera letra de un elemento para saltar a él.

### El editor

- **Agregar ítem / Agregar submenú / Agregar separador**, **Subir / Bajar**,
  **Eliminar** (también con la tecla Supr).
- Arrastrá elementos dentro del árbol para reordenarlos o meterlos en un
  submenú.
- Arrastrá archivos, carpetas o accesos directos desde el Explorador o el
  escritorio para agregarlos.
- Para cada elemento: nombre, línea de comando (con **Examinar…**), icono
  (**Cambiar…** elige cualquier icono de un `.exe`, `.dll`, `.ico` o
  `.png`; **Restablecer** vuelve al del programa) y **Probar**.
- Los elementos cuyo programa no se encuentra aparecen en rojo.
- Los cambios se guardan solos.

### Importar una configuración

En el editor, **Importar configuración…** y elegí una configuración de menú
en XML (un archivo con elementos `<menu>` e `<item>`). Se importan
submenús, separadores, comandos e iconos; el archivo original solo se lee,
nunca se modifica.

### Opciones

Atajo, tamaño de iconos (16, 24, 32 o 48 px), tema (igual que Windows,
claro u oscuro), idioma (igual que Windows, inglés o español), iniciar con
Windows y buscar actualizaciones al iniciar.

## Archivos de configuración

Popa guarda dos archivos UTF-8 junto a `Popa.exe`, con sangría para poder
editarlos a mano: `menu.json` (el menú) y `settings.json` (las opciones).
Si esa carpeta no permite escritura (por ejemplo dentro de `Program Files`),
van a `%APPDATA%\Popa`. El formato está en el [README en inglés](README.md#configuration-files).

Si `menu.json` se daña, Popa guarda una copia como `menu.json.bak` y
arranca con el menú vacío.

## Privacidad y red

Popa se conecta **solamente** a
`https://api.github.com/repos/TraX22/Popa/releases/latest`, para saber si
hay una versión nueva: una vez, unos segundos después de iniciar (se puede
desactivar en Opciones), y cuando elegís **Buscar actualizaciones**. La
petición solo lleva el encabezado `User-Agent: Popa/<versión>`. Sin
internet, la consulta automática no muestra nada.

Si aceptás una actualización, Popa descarga `Popa.exe` del release, solo
por HTTPS y desde direcciones de GitHub, comprueba que sea un programa
válido y que coincida con la huella SHA-256 que publica GitHub, y se
reemplaza a sí mismo. Tus `menu.json` y `settings.json` no se tocan.

## Limitaciones conocidas

- Algunos **antivirus** desconfían del hook de teclado de bajo nivel que
  Popa usa para Win+Z, porque el programa no tiene firma digital. El hook
  hace falta porque Windows 11 reserva Win+Z y no deja registrarlo de la
  forma normal.
- El atajo no funciona mientras haya en primer plano un programa
  **ejecutado como administrador**: Windows no deja que los programas
  normales vean el teclado de los elevados. Usá el icono de la bandeja.
- El editor usa el tema claro; con el modo oscuro solo se oscurece su barra
  de título.

## Compilar

Requisitos: **Visual Studio 2026** con la carga "Desarrollo para el
escritorio con C++" (toolset MSVC v145, Windows SDK).

```powershell
.\build.ps1                         # Debug y Release, con todas las pruebas
.\build.ps1 -Configuration Release  # solo Release
.\publish.ps1                       # Release lista para publicar en dist\
```

O abrí `Popa.slnx` en Visual Studio. La estructura del proyecto está en el
[README en inglés](README.md#building).

## Licencia

[MIT](LICENSE). nlohmann/json también es MIT
([third_party/nlohmann/LICENSE.MIT](third_party/nlohmann/LICENSE.MIT)).
