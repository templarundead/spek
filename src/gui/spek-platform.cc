#include "spek-config.h"

#include <cstring>

#ifdef OS_WIN
// wxWidgets 3.3 uses Winsock2; don't let windows.h pull in Winsock 1 first.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <wx/filename.h>
#include <wx/settings.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include "spek-platform.h"

#ifdef OS_OSX
// Implemented in spek-platform-osx.mm, the AppKit API is Objective-C.
void spek_platform_init_osx();
#endif

void spek_platform_init()
{
#ifdef OS_OSX
    spek_platform_init_osx();
#endif
}

wxString spek_platform_config_path(const wxString& app_name)
{
#ifdef OS_WIN
    wxFileName file_name(wxStandardPaths::Get().GetUserConfigDir(), wxEmptyString);
#else
    wxFileName file_name;
    wxString xdg_config_home;
    if (wxGetEnv("XDG_CONFIG_HOME", &xdg_config_home) && !xdg_config_home.IsEmpty()) {
        file_name = wxFileName(xdg_config_home, wxEmptyString);
    } else {
        file_name = wxFileName(wxGetHomeDir(), wxEmptyString);
        file_name.AppendDir(".config");
    }
#endif
    file_name.AppendDir(app_name);
    file_name.Mkdir(0755, wxPATH_MKDIR_FULL);
    file_name.SetFullName("preferences");
    return file_name.GetFullPath();
}

bool spek_platform_can_change_language()
{
#ifdef OS_UNIX
    return false;
#else
    return true;
#endif
}

double spek_platform_font_scale()
{
#ifdef OS_OSX
    return 1.3;
#else
    return 1.0;
#endif
}

bool spek_platform_is_dark()
{
    return wxSystemSettings::GetAppearance().IsDark();
}

void spek_platform_enable_dark_mode(wxWindow *window)
{
#ifdef OS_WIN
    if (window == nullptr || !spek_platform_is_dark()) {
        return;
    }

    const HWND handle = static_cast<HWND>(window->GetHandle());
    if (handle == nullptr) {
        return;
    }

    // DWMWA_USE_IMMERSIVE_DARK_MODE is not documented by Microsoft and is
    // missing from older SDKs, so look the function up at run time. It is
    // available since Windows 10 1809 and only used there since Windows 11.
    using DwmSetWindowAttributeFunc = HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD);

    const HMODULE module = ::LoadLibraryW(L"dwmapi.dll");
    if (module == nullptr) {
        return;
    }

    auto set_attribute = reinterpret_cast<DwmSetWindowAttributeFunc>(
        ::GetProcAddress(module, "DwmSetWindowAttribute")
    );
    if (set_attribute != nullptr) {
        constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
        const BOOL value = TRUE;
        set_attribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    }

    ::FreeLibrary(module);
#else
    (void) window;
#endif
}
