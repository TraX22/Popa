# Popa — Especificación de diseño

Fecha: 2026-10-08 (revisión 2: C++ con Win32)
Estado: pendiente de revisión

## 1. Objetivo

Popa es un lanzador de aplicaciones para Windows 11. Al presionar un atajo
global de teclado abre, en la posición del cursor, un menú emergente con los
programas que el usuario cargó a mano, organizados en submenús.

Motivos del proyecto:

- Código libre, en un repositorio público de GitHub.
- Sin métricas ni telemetría de ningún tipo.
- Aspecto Windows 11.
- Ejecutable chico: **menos de 5 MB, idealmente cerca de 1 MB**.
- El usuario final no instala nada: un solo .exe.
- Código legible, que sirva para aprender.

## 2. Principios

- **Todo manual.** La app nunca escanea ni detecta programas instalados. Solo
  muestra lo que el usuario cargó.
- **Privacidad.** La única conexión de red es la consulta de actualizaciones a
  GitHub Releases. No se envía ningún dato del usuario ni de uso.
- **Silencio sin internet.** La falta de conexión nunca produce un mensaje de
  error en la consulta automática.
- **Portable.** Un solo .exe; la configuración vive junto a él.
- **Solo lo que trae Windows.** Toda la funcionalidad usa APIs incluidas en
  Windows. La única librería externa es un archivo de cabecera para JSON.

## 3. Base técnica

| Tema | Decisión |
|---|---|
| Lenguaje | C++20 |
| Interfaz | API de Windows (Win32) pura, sin MFC, Qt ni WinUI |
| Compilador | MSVC, Visual Studio 2026, carga "Desarrollo para el escritorio con C++" |
| Plataforma | x64, Windows 11 (debería funcionar en Windows 10, sin garantía) |
| Runtime de C++ | Enlazado estático (`/MT`), para no exigir el "Visual C++ Redistributable" |
| Tamaño | Release con optimización de tamaño (`/O1`), sin información de depuración en el .exe |
| JSON | `nlohmann/json`, un único archivo de cabecera copiado en `third_party/` (licencia MIT) |
| Pruebas | Framework de pruebas nativo de Microsoft (CppUnitTest), incluido en la carga de C++ |
| Licencia | MIT |

El manifiesto de la aplicación declara:

- Controles comunes versión 6 (aspecto moderno de botones, árbol, etc.).
- DPI por monitor versión 2 (nitidez en pantallas con escalado).
- Compatibilidad con Windows 10/11.

## 4. Estructura de la solución

```
Popa.slnx
src/
  PopaCore/        Biblioteca estática: toda la lógica sin interfaz
    Model.h/.cpp           Árbol de ítems y opciones
    ConfigStore.h/.cpp     Lectura y escritura de menu.json y settings.json
    CommandLine.h/.cpp     Separar ejecutable y argumentos, variables de entorno
    IconRef.h/.cpp         Interpretar "archivo:índice"
    MenuImporter.h/.cpp    Importar un menú desde una configuración XML
    Version.h/.cpp         Comparación de versiones
    HotkeyText.h/.cpp      Conversión de texto del atajo ("Win+Z") a teclas
    DroppedItem.h/.cpp     Reglas para los archivos soltados en el editor
    UpdateCheck.h/.cpp     Interpretar la respuesta de GitHub
    Localization.h/.cpp    Textos de la interfaz en inglés y español
    Text.h/.cpp            Conversión UTF-8 / UTF-16
  Popa/            Aplicación Win32
    main.cpp               Punto de entrada, instancia única
    App.h/.cpp             Ventana invisible, bucle de mensajes y estado de la app
    Tray.h/.cpp            Icono de bandeja y su menú
    Hotkey.h/.cpp          Hook de teclado
    PopupMenu.h/.cpp       Menú emergente dibujado a medida
    Theme.h/.cpp           Colores claro/oscuro y detección del tema
    IconCache.h/.cpp       Extracción y caché de iconos
    ShortcutResolver.h/.cpp  Lectura de .lnk
    Launcher.h/.cpp        Ejecutar comandos
    EditorWindow.h/.cpp
    SettingsDialog.h/.cpp
    IconPickerDialog.h/.cpp
    XmlFile.h/.cpp         Lectura de XML con XmlLite
    FileDialog.h/.cpp      Diálogo estándar "Abrir"
    UiUtil.h               Fuente y escala por DPI compartidas por las ventanas
    Startup.h/.cpp         Iniciar con Windows
    Updater.h/.cpp         Descarga y reemplazo del .exe
    Popa.rc, resource.h, Popa.ico, Popa.manifest
tests/
  PopaTests/       Pruebas de PopaCore
third_party/
  nlohmann/json.hpp
tools/             make_icon.py (dibuja Popa.ico)
docs/
build.ps1          Compila y prueba (MSBuild y vstest, ubicados con vswhere)
publish.ps1        Prepara dist\Popa.exe y, con -Publish, crea el release
README.md, README.es.md, LICENSE
```

`PopaCore` no incluye nada de ventanas, así que se puede probar sola. `Popa`
usa `PopaCore`; nunca al revés.

## 5. Componentes

### 5.1 Arranque e instancia única

- Un mutex con nombre (`Local\Popa`) impide dos instancias. Si ya hay una
  abierta, la segunda le pide abrir el editor y se cierra.
- Una ventana invisible recibe los mensajes del icono de bandeja, del atajo y
  del actualizador.

### 5.2 Bandeja del sistema (`Tray`)

Icono junto al reloj (`Shell_NotifyIconW`). Clic derecho muestra:

- Abrir editor
- Opciones
- Buscar actualizaciones
- Atajos: activar / desactivar
- Salir

Clic izquierdo abre el menú emergente. Doble clic abre el editor.

Si el Explorador de Windows se reinicia, el icono se vuelve a crear
(mensaje `TaskbarCreated`).

### 5.3 Atajo global (`Hotkey`)

- Atajo por defecto: **Win+Z**. Configurable en Opciones.
- Hook de teclado de bajo nivel (`SetWindowsHookExW` con `WH_KEYBOARD_LL`),
  porque Windows 11 reserva Win+Z y `RegisterHotKey` lo rechaza.
- Al detectar el atajo, la pulsación se consume y se abre el menú.
- Para que no se abra el menú Inicio al soltar Win, se envía una tecla neutra
  antes de soltarla (técnica habitual en este tipo de programas).
- Se puede apagar y encender desde la bandeja.
- Riesgo conocido: algunos antivirus desconfían de los hooks de teclado en
  programas sin firma digital. Se documenta en el README.

### 5.4 Menú emergente (`PopupMenu`)

Se usa el **menú nativo de Windows** (`CreatePopupMenu` + `TrackPopupMenuEx`)
con **ítems dibujados a medida** (`MF_OWNERDRAW`):

- Windows 11 pone solo las esquinas redondeadas, la sombra y la animación.
- Los submenús en cascada, la navegación con teclado, el cierre al hacer clic
  afuera y el reacomodo en pantalla los resuelve Windows.
- Popa dibuja cada fila: icono a 16, 24, 32 o 48 px, nombre, resaltado
  redondeado al pasar el mouse, y separadores como línea fina.
- El dibujo se hace con GDI y los iconos con `DrawIconEx`, respetando el DPI
  del monitor.
- El menú se abre en el cursor. Si es muy alto, Windows lo desplaza con
  flechas de scroll.
- Al elegir un ítem se ejecuta su comando.

Tema claro/oscuro (`Theme`):

- Se detecta con el valor `AppsUseLightTheme` del registro y se actualiza al
  cambiar el tema de Windows (`WM_SETTINGCHANGE`).
- El fondo oscuro del menú usa `SetPreferredAppMode` de `uxtheme.dll`, función
  no documentada pero estable desde Windows 10 1903 y usada por muchos
  programas. Si no existe, el menú queda claro.
- La opción "Tema" permite forzar claro u oscuro.

### 5.5 Editor (`EditorWindow`)

Ventana redimensionable. Arriba, un árbol (`TreeView`) con los ítems y sus
iconos. Abajo, el detalle del ítem seleccionado.

Barra de botones:

- Agregar ítem
- Agregar submenú
- Agregar separador
- Subir / bajar
- Eliminar (pide confirmación si el submenú tiene hijos)
- Importar configuración…

Panel de detalle:

- Nombre
- Línea de comando, editable a mano, con botón "Examinar…"
- Icono: vista previa, "Cambiar…" y "Restablecer"
- Botón "Probar"

Arrastrar dentro del árbol reordena o mueve un ítem a otro submenú
(`TVN_BEGINDRAG`). Los cambios se guardan solos en `menu.json` y el menú
emergente los refleja en la próxima apertura.

El editor usa el tema claro de Windows; con tema oscuro solo se oscurece la
barra de título (`DwmSetWindowAttribute`). Un editor completamente oscuro
queda fuera de esta versión, porque los controles estándar de Win32 no lo
soportan bien.

### 5.6 Arrastrar y soltar desde Windows

El árbol acepta archivos soltados (`DragAcceptFiles` / `WM_DROPFILES`). Se crea
un ítem por archivo:

| Se suelta | Nombre | Comando | Icono |
|---|---|---|---|
| Acceso directo `.lnk` | Nombre del acceso | Destino del acceso, con sus argumentos | El del acceso; si no tiene, el del destino |
| Ejecutable `.exe` | Nombre del archivo sin extensión | Ruta del .exe | El del .exe |
| Carpeta | Nombre de la carpeta | Ruta de la carpeta | Icono de carpeta |
| Otro archivo | Nombre del archivo | Ruta del archivo | El asociado al tipo |

El ítem se inserta en la posición donde se soltó; sobre un submenú queda
adentro.

`ShortcutResolver` lee los `.lnk` con `IShellLinkW` e `IPersistFile` (COM).

### 5.7 Iconos (`IconRef`, `IconCache`, `IconPickerDialog`)

- Un icono se identifica por **archivo + índice**:
  `shell32.dll:13` es el icono número 13 de ese archivo.
- Sin icono propio, se usa el del archivo del comando (`SHGetFileInfoW`
  para carpetas y documentos, `PrivateExtractIconsW` para .exe/.dll/.ico al
  tamaño exacto).
- Se admiten `.ico`, `.exe`, `.dll` y `.png` (este último con WIC, incluido en
  Windows).
- Las rutas admiten variables de entorno (`%SystemRoot%`) y nombres sin
  carpeta (`pifmgr.dll`), que se buscan en `System32`.
- `IconPickerDialog` muestra todos los iconos de un archivo en una grilla para
  elegir uno.
- Los iconos se cargan una vez y quedan en memoria.
- Si no se puede obtener, se usa un icono genérico.

### 5.8 Lanzador (`CommandLine`, `Launcher`)

- Separa ejecutable y argumentos respetando comillas.
- Expande variables de entorno (`ExpandEnvironmentStringsW`).
- Ejecuta con `ShellExecuteExW`, que también abre carpetas, documentos,
  accesos directos y direcciones web.
- Carpeta de trabajo: la del ejecutable.
- Si falla (archivo inexistente), muestra un aviso breve con el motivo.

### 5.9 Importar configuración (`MenuImporter`)

Lee una configuración de menú en XML (UTF-16 o UTF-8) con **XmlLite**, el
lector XML incluido en Windows. Reglas, tomadas de un archivo real:

| En el XML | En Popa |
|---|---|
| `<item>` con `<item>` hijos | Submenú |
| `<item issep="1">` | Separador (el texto decorativo del nombre se descarta) |
| `<item>` sin hijos | Ítem; `<command>` pasa a ser su comando |
| `<command>command</command>` en un submenú | Se ignora: es un valor de relleno |
| `icon="ruta:N"` | Icono con archivo `ruta` e índice `N` |
| Comillas alrededor de la ruta del icono | Se quitan |
| Ítem sin atributo `icon` | Icono automático según el comando |
| `ID`, `lastID`, `<settings>`, `<wins>` | No se importan |

Para que `PopaCore` no dependa de XmlLite y se pueda probar, el importador
recibe los nodos ya leídos a través de una interfaz mínima; la lectura con
XmlLite vive en `Popa`. Si ya hay ítems, se pregunta si reemplazarlos o
agregarlos al final. El archivo original nunca se modifica.

### 5.10 Opciones (`SettingsDialog`, `Startup`)

- Atajo del menú
- Tamaño de iconos: 16, 24, 32 o 48
- Tema: sistema, claro u oscuro
- Idioma: igual que Windows, inglés o español
- Iniciar con Windows
- Buscar actualizaciones al iniciar

"Iniciar con Windows" escribe o borra una entrada en
`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`. No requiere permisos de
administrador.

### 5.11 Actualizador (`UpdateCheck`, `Updater`)

- Consulta `https://api.github.com/repos/TraX22/Popa/releases/latest` con
  **WinHTTP**, incluido en Windows.
- Compara la etiqueta publicada (por ejemplo `v1.2.0`) con la versión del .exe,
  que se lee de su recurso de versión.
- **Automático:** una vez, unos segundos después de iniciar, en un hilo aparte
  para no trabar nada. Si falla por cualquier motivo, no se muestra nada.
- **Manual:** "Buscar actualizaciones" en la bandeja. Se informa el resultado:
  hay versión nueva, ya está al día, o no se pudo conectar.
- Si hay versión nueva, un aviso muestra el número y las notas, con
  "Actualizar" y "Ahora no". Con "Ahora no" se vuelve a avisar en el próximo
  inicio.
- Al actualizar: descarga `Popa.exe` del release a un archivo temporal junto
  al actual, renombra el .exe en ejecución a `Popa.old.exe` (Windows permite
  renombrar un .exe abierto), mueve el nuevo a su lugar y reinicia. Al iniciar,
  la app borra el `.old.exe`.
- Solo se aceptan descargas HTTPS desde `github.com` y sus dominios de
  descarga de releases.
- `menu.json` y `settings.json` nunca se modifican en una actualización.

La petición solo lleva el encabezado `User-Agent: Popa/<versión>`, que GitHub
exige. No se envía nada más.

## 6. Datos

Ambos archivos están junto al .exe, en UTF-8 y con sangría para poder
editarlos a mano.

`menu.json`

```json
{
  "version": 1,
  "items": [
    { "type": "item", "name": "Discord",
      "command": "C:\\Users\\yo\\AppData\\Local\\Discord\\Update.exe --processStart Discord.exe",
      "icon": null },
    { "type": "separator" },
    { "type": "menu", "name": "Internet",
      "icon": { "path": "shell32.dll", "index": 13 }, "items": [] },
    { "type": "menu", "name": "Adobe",
      "icon": { "path": "icons\\adobe.ico", "index": 0 },
      "items": [
        { "type": "item", "name": "Photoshop",
          "command": "\"C:\\Program Files\\Adobe\\Photoshop\\Photoshop.exe\"",
          "icon": null }
      ] }
  ]
}
```

- `icon` en `null` significa "usar el icono del comando".
- Las rutas de icono pueden ser absolutas o relativas a la carpeta del .exe.

`settings.json`

```json
{
  "hotkey": "Win+Z",
  "hotkeysEnabled": true,
  "iconSize": 32,
  "theme": "system",
  "language": "system",
  "runAtStartup": true,
  "checkUpdatesAtStartup": true
}
```

Si la carpeta del .exe no permite escritura, la configuración se guarda en
`%APPDATA%\Popa`.

## 7. Manejo de errores

| Situación | Comportamiento |
|---|---|
| El programa de un ítem ya no existe | El ítem se marca en el editor; al hacer clic en el menú se avisa |
| `menu.json` dañado | Se guarda una copia como `menu.json.bak`, se avisa y se arranca con menú vacío |
| `settings.json` dañado o ausente | Se usan los valores por defecto |
| No se puede instalar el hook de teclado | Aviso en la bandeja; el menú sigue disponible con clic en el icono |
| Sin internet en la consulta automática | Nada |
| Falla la descarga de una actualización | Aviso; la versión actual queda intacta |

## 8. Pruebas

Pruebas automáticas (CppUnitTest) sobre `PopaCore`:

- `ConfigStore`: guardar y cargar, archivo dañado, archivo ausente.
- `CommandLine`: separación de ejecutable y argumentos, comillas, variables.
- `IconRef`: `"archivo:índice"`, comillas, variables, rutas relativas.
- `MenuImporter`: conversión de un ejemplo que cubre submenús,
  separadores, iconos con índice y rutas entre comillas.
- `Version` y `UpdateCheck`: comparación de versiones y lectura de una
  respuesta de GitHub de ejemplo.
- Conversión de texto del atajo ("Win+Z") a teclas.

La interfaz (menú, editor, arrastrar y soltar, atajo, bandeja) se verifica a
mano con una lista de comprobación por etapa.

Criterio de tamaño: el `Popa.exe` de Release debe pesar menos de 5 MB; se
espera alrededor de 1 MB.

## 9. Fuera de alcance en esta versión

Clips, memos, ventanas ("Wins"), recientes, gestos de mouse, menús de carpeta,
menú en columnas, modo solo iconos, atajos por ítem, editor en tema oscuro
completo, instalador y firma digital del ejecutable.

## 10. Repositorio

Público, en `https://github.com/TraX22/Popa`. El usuario y el nombre del
repositorio figuran como constantes en un único lugar del código.
