#include "PopaCore/Localization.h"

#include <iterator>

namespace popa {

namespace {

struct TextRow {
    TextId id; // repeated here so a test can check the rows are in enum order
    const wchar_t* english;
    const wchar_t* spanish;
};

// Rows must follow the order of the TextId enum: Tr() indexes the table by
// the numeric value of the id. The static_assert below catches a missing
// or extra row at compile time; the tests catch rows in the wrong order.
constexpr TextRow kTexts[] = {
    {TextId::AppName, L"Popa", L"Popa"},

    {TextId::TrayOpenEditor, L"Open editor", L"Abrir editor"},
    {TextId::TrayOptions, L"Options", L"Opciones"},
    {TextId::TrayCheckUpdates, L"Check for updates", L"Buscar actualizaciones"},
    {TextId::TrayEnableHotkeys, L"Enable hotkeys", L"Activar atajos"},
    {TextId::TrayDisableHotkeys, L"Disable hotkeys", L"Desactivar atajos"},
    {TextId::TrayExit, L"Exit", L"Salir"},

    {TextId::MenuCorruptTitle, L"The menu file is damaged", L"El archivo del menú está dañado"},
    {TextId::MenuCorruptBody,
     L"Popa started with an empty menu. A copy of the damaged file was saved as {0}.",
     L"Popa arrancó con el menú vacío. Se guardó una copia del archivo dañado en {0}."},
    {TextId::MenuUnreadableTitle, L"The menu file could not be read",
     L"No se pudo leer el archivo del menú"},
    {TextId::MenuUnreadableBody,
     L"{0} may be open in another program. Changes will not be saved until Popa is restarted.",
     L"Puede que {0} esté abierto en otro programa. Los cambios no se guardarán hasta reiniciar Popa."},

    {TextId::MenuEmpty, L"(empty)", L"(vacío)"},
    {TextId::LaunchFailedBody, L"Could not open “{0}”.\n\n{1}\n\n{2}",
     L"No se pudo abrir «{0}».\n\n{1}\n\n{2}"},

    {TextId::HotkeyFailedTitle, L"The hotkey could not be set up", L"No se pudo activar el atajo"},
    {TextId::HotkeyFailedBody, L"{0} will not open the menu. Click the tray icon instead.",
     L"{0} no abrirá el menú. Podés abrirlo con un clic en este icono."},

    {TextId::EditorTitle, L"Popa — Menu editor", L"Popa — Editor del menú"},
    {TextId::EditorAddItem, L"Add item", L"Agregar ítem"},
    {TextId::EditorAddMenu, L"Add submenu", L"Agregar submenú"},
    {TextId::EditorAddSeparator, L"Add separator", L"Agregar separador"},
    {TextId::EditorMoveUp, L"Move up", L"Subir"},
    {TextId::EditorMoveDown, L"Move down", L"Bajar"},
    {TextId::EditorDelete, L"Delete", L"Eliminar"},
    {TextId::EditorImport, L"Import configuration…", L"Importar configuración…"},
    {TextId::EditorName, L"Name:", L"Nombre:"},
    {TextId::EditorCommand, L"Command line:", L"Línea de comando:"},
    {TextId::EditorBrowse, L"Browse…", L"Examinar…"},
    {TextId::EditorIcon, L"Icon:", L"Icono:"},
    {TextId::EditorChangeIcon, L"Change…", L"Cambiar…"},
    {TextId::EditorResetIcon, L"Reset", L"Restablecer"},
    {TextId::EditorTest, L"Test", L"Probar"},
    {TextId::EditorNewItem, L"New item", L"Nuevo ítem"},
    {TextId::EditorNewMenu, L"New submenu", L"Nuevo submenú"},
    {TextId::EditorNoName, L"(no name)", L"(sin nombre)"},
    {TextId::EditorDeleteConfirm, L"Delete “{0}” and the {1} entries inside it?",
     L"¿Eliminar «{0}» y los {1} elementos que contiene?"},
    {TextId::EditorTargetMissing, L"The target of this command was not found.",
     L"No se encontró el destino de este comando."},
    {TextId::EditorSaveFailedTitle, L"Changes could not be saved", L"No se pudieron guardar los cambios"},
    {TextId::EditorSaveFailedBody, L"Popa could not write {0}.", L"Popa no pudo escribir {0}."},

    {TextId::ImportFilter, L"XML configuration", L"Configuración XML"},
    {TextId::ImportFailed, L"{0} could not be read as a menu configuration.",
     L"No se pudo leer {0} como configuración de menú."},
    {TextId::ImportQuestion, L"The menu already has entries. What should be done with the imported ones?",
     L"El menú ya tiene elementos. ¿Qué hacer con los importados?"},
    {TextId::ImportReplace, L"Replace the current menu", L"Reemplazar el menú actual"},
    {TextId::ImportAppend, L"Add them at the end", L"Agregarlos al final"},
    {TextId::ImportDone, L"{0} entries were imported. The original file was not modified.",
     L"Se importaron {0} elementos. El archivo original no se modificó."},

    {TextId::SettingsTitle, L"Popa options", L"Opciones de Popa"},
    {TextId::SettingsHotkey, L"Menu hotkey:", L"Atajo del menú:"},
    {TextId::SettingsHotkeyInvalid, L"This combination cannot be used: choose Win, Ctrl or Alt, or an F key.",
     L"Esa combinación no se puede usar: elegí Win, Ctrl o Alt, o una tecla F."},
    {TextId::SettingsIconSize, L"Icon size:", L"Tamaño de los iconos:"},
    {TextId::SettingsPixels, L"{0} px", L"{0} px"},
    {TextId::SettingsTheme, L"Theme:", L"Tema:"},
    {TextId::SettingsLanguage, L"Language:", L"Idioma:"},
    {TextId::LanguageSystem, L"Same as Windows", L"Igual que Windows"},
    {TextId::ThemeSystem, L"Same as Windows", L"Igual que Windows"},
    {TextId::ThemeLight, L"Light", L"Claro"},
    {TextId::ThemeDark, L"Dark", L"Oscuro"},
    {TextId::SettingsRunAtStartup, L"Start with Windows", L"Iniciar con Windows"},
    {TextId::SettingsCheckUpdates, L"Check for updates at startup", L"Buscar actualizaciones al iniciar"},
    {TextId::StartupFailedTitle, L"Start with Windows", L"Iniciar con Windows"},
    {TextId::StartupFailedBody, L"The Windows startup setting could not be changed.",
     L"No se pudo cambiar el inicio con Windows."},

    {TextId::UpdatesTitle, L"Popa updates", L"Actualizaciones de Popa"},
    {TextId::UpdateAvailableTitle, L"Popa {0} is available", L"Popa {0} está disponible"},
    {TextId::UpdateAvailableBody, L"You have version {0}.", L"Tenés la versión {0}."},
    {TextId::UpdateShowNotes, L"Show what's new", L"Ver novedades"},
    {TextId::UpdateHideNotes, L"Hide what's new", L"Ocultar novedades"},
    {TextId::UpdateNow, L"Update", L"Actualizar"},
    {TextId::UpdateLater, L"Not now", L"Ahora no"},
    {TextId::UpdateUpToDate, L"Popa is up to date (version {0}).", L"Popa está al día (versión {0})."},
    {TextId::UpdateNoReleases, L"No version of Popa has been published yet.",
     L"Todavía no hay versiones publicadas de Popa."},
    {TextId::UpdateCheckFailed,
     L"Could not check for updates. Check the internet connection and try again later.",
     L"No se pudo consultar si hay actualizaciones. Revisá la conexión a internet y probá más tarde."},
    {TextId::UpdateDownloading, L"Downloading Popa {0}…", L"Descargando Popa {0}…"},
    {TextId::UpdateDownloadFailed, L"The update could not be downloaded: {0}.\n\nPopa stays at the current version.",
     L"No se pudo descargar la actualización: {0}.\n\nPopa sigue en la versión actual."},
    {TextId::UpdateErrorConnection, L"there was no connection to GitHub", L"no hubo conexión con GitHub"},
    {TextId::UpdateErrorAddress, L"the download address is not on GitHub", L"la dirección de descarga no es de GitHub"},
    {TextId::UpdateErrorHttp, L"GitHub answered with code {0}", L"GitHub respondió con el código {0}"},
    {TextId::UpdateErrorInvalid, L"the downloaded file is not a valid Popa.exe",
     L"el archivo descargado no es un Popa.exe válido"},
    {TextId::UpdateErrorHash, L"the file does not match the published SHA-256 fingerprint",
     L"el archivo no coincide con la huella SHA-256 publicada"},
    {TextId::UpdateErrorWrite, L"it could not be saved as {0}", L"no se pudo guardar como {0}"},
    {TextId::UpdateInstallFailed, L"Popa.exe could not be replaced: {0}\n\nPopa stays at the current version.",
     L"No se pudo reemplazar Popa.exe: {0}\n\nPopa sigue en la versión actual."},
    {TextId::UpdateRestartFailed, L"Popa was updated. Open it again to use the new version.",
     L"Popa se actualizó. Abrilo de nuevo para usar la versión nueva."},

    {TextId::ProgramsFilter, L"Programs and shortcuts", L"Programas y accesos directos"},
    {TextId::IconFilesFilter, L"Icons", L"Iconos"},
    {TextId::AllFilesFilter, L"All files", L"Todos los archivos"},

    {TextId::IconPickerTitle, L"Choose icon", L"Elegir icono"},
    {TextId::IconPickerFile, L"File:", L"Archivo:"},
    {TextId::IconPickerNoIcons, L"This file has no icons.", L"Este archivo no tiene iconos."},

    {TextId::CommonOk, L"OK", L"Aceptar"},
    {TextId::CommonCancel, L"Cancel", L"Cancelar"},

    {TextId::NotYetAvailable, L"This feature is not available yet.",
     L"Esta función todavía no está disponible."},
};

static_assert(std::size(kTexts) == static_cast<size_t>(TextId::Count),
              "kTexts needs exactly one row per TextId");

Language g_language = Language::English;

} // namespace

Language LanguageFromLangId(unsigned short langId)
{
    // A LANGID packs two numbers: the low 10 bits are the primary language
    // (Spanish = 0x0A) and the upper 6 bits the region (Spain, Mexico,
    // Argentina...). Only the primary language matters here. This is what
    // the PRIMARYLANGID macro of <windows.h> does.
    constexpr unsigned short kPrimaryMask = 0x3FF;
    constexpr unsigned short kLangSpanish = 0x0A;
    return (langId & kPrimaryMask) == kLangSpanish ? Language::Spanish : Language::English;
}

Language ResolveLanguage(LanguageSetting setting, unsigned short systemLangId)
{
    switch (setting) {
    case LanguageSetting::English: return Language::English;
    case LanguageSetting::Spanish: return Language::Spanish;
    case LanguageSetting::System:
    default: return LanguageFromLangId(systemLangId);
    }
}

void SetLanguage(Language language)
{
    g_language = language;
}

Language CurrentLanguage()
{
    return g_language;
}

const wchar_t* Tr(TextId id, Language language)
{
    const auto index = static_cast<size_t>(id);
    if (index >= std::size(kTexts))
        return L"";
    const TextRow& row = kTexts[index];
    return language == Language::Spanish ? row.spanish : row.english;
}

const wchar_t* Tr(TextId id)
{
    return Tr(id, g_language);
}

std::wstring Format(std::wstring_view pattern, std::initializer_list<std::wstring_view> arguments)
{
    std::wstring result;
    result.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size(); ++i) {
        // "{N}" with a single digit N in range becomes argument N.
        if (pattern[i] == L'{' && i + 2 < pattern.size() && pattern[i + 2] == L'}' &&
            pattern[i + 1] >= L'0' && pattern[i + 1] <= L'9') {
            const auto n = static_cast<size_t>(pattern[i + 1] - L'0');
            if (n < arguments.size()) {
                result += *(arguments.begin() + n);
                i += 2;
                continue;
            }
        }
        result += pattern[i];
    }
    return result;
}

} // namespace popa
