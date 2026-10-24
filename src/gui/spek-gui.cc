// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <cstdlib>
#include <memory>
#include <string>

extern "C" {
#include <libavutil/log.h>
}

#include <wx/cmdline.h>
#include <wx/log.h>
#include <wx/socket.h>
#include <wx/translation.h>
#include <wx/wx.h>

#include "spek-artwork.h"
#include "spek-config.h"
#include "spek-i18n.h"
#include "spek-platform.h"
#include "spek-preferences.h"

#include "spek-spectrogram.h"
#include "spek-window.h"

#include "spek-gui.h"

namespace
{

class SpekApp : public wxApp
{
public:
    bool OnInit() override;
    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;
    int OnRun() override;

#ifdef OS_OSX
    void MacOpenFiles(const wxArrayString& files) override;
#endif

private:
    SpekWindow *window = nullptr;
    wxString path;
    bool quit = false;
};

// The core library knows nothing about wxWidgets, so it asks the application
// to translate its strings. Route the requests to wx, which loads the
// catalogue set up by SpekPreferences.
std::string wx_translate(const char *msgid)
{
    return std::string(wxGetTranslation(wxString::FromUTF8(msgid)).utf8_str());
}

std::string wx_translate_plural(const char *msgid, const char *msgid_plural, unsigned long n)
{
    return std::string(wxGetTranslation(
        wxString::FromUTF8(msgid),
        wxString::FromUTF8(msgid_plural),
        static_cast<unsigned>(n)
    ).utf8_str());
}

} // namespace

// Spek defines its own main(), so we only implement the application class.
wxIMPLEMENT_APP_NO_MAIN(SpekApp);

bool SpekApp::OnInit()
{
    if (!wxApp::OnInit()) {
        return false;
    }
    if (this->quit) {
        return true;
    }

    // FFmpeg is very chatty about the formats it has to guess, only report
    // the real errors.
    av_log_set_level(AV_LOG_ERROR);

#if defined(__WXMSW__) && wxCHECK_VERSION(3, 3, 0)
    // wxWidgets 3.3 can draw the window decorations in dark mode, on older
    // versions this is done per window, see SpekWindow's constructor.
    MSWEnableDarkMode(wxApp::DarkMode_Auto);
#endif

    wxInitAllImageHandlers();
    wxSocketBase::Initialize();

    spek_set_translator(&wx_translate, &wx_translate_plural);

    spek_artwork_init();
    spek_platform_init();
    SpekPreferences::get().init();

    this->window = new SpekWindow(this->path);
    this->window->Show(true);
    SetTopWindow(this->window);

    return true;
}

void SpekApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    static const wxCmdLineEntryDesc desc[] = {
        {
            wxCMD_LINE_SWITCH,
            "h",
            "help",
            "Show this help message",
            wxCMD_LINE_VAL_NONE,
            wxCMD_LINE_OPTION_HELP,
        },
        {
            wxCMD_LINE_SWITCH,
            "V",
            "version",
            "Display the version and exit",
            wxCMD_LINE_VAL_NONE,
            wxCMD_LINE_PARAM_OPTIONAL,
        },
        {
            wxCMD_LINE_PARAM,
            nullptr,
            nullptr,
            "FILE",
            wxCMD_LINE_VAL_STRING,
            wxCMD_LINE_PARAM_OPTIONAL,
        },
        wxCMD_LINE_DESC_END,
    };

    // wxApp::OnInit() parses the command line before returning. Supply our
    // options here instead of parsing it a second time with the default wx
    // options first, which rejects a perfectly valid audio file argument.
    parser.SetDesc(desc);
}

bool SpekApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    if (parser.Found("version")) {
        // TRANSLATORS: the %s is the package version.
        wxPrintf(_("Spek version %s"), wxString::FromUTF8(SPEK_VERSION).c_str());
        wxPrintf("\n");
        this->quit = true;
        return true;
    }
    if (parser.GetParamCount()) {
        this->path = parser.GetParam();
    }

    return true;
}

int SpekApp::OnRun()
{
    if (this->quit) {
        return 0;
    }

    return wxApp::OnRun();
}

#ifdef OS_OSX
void SpekApp::MacOpenFiles(const wxArrayString& files)
{
    if (files.GetCount() == 1 && this->window != nullptr) {
        this->window->open(files[0]);
    }
}
#endif

int spek_run_gui(int argc, char **argv)
{
#if defined(OS_UNIX) && SPEK_STATIC_LINK
    // Spek draws through wxDC and does not create an OpenGL canvas. Prevent
    // GTK's startup GLX probe from dlopen()ing the host's Mesa/LLVM stack into
    // a fully static process, which can crash in LLVM's initialisers.
    setenv("GDK_GL", "disable", 1);
#endif
    return wxEntry(argc, argv);
}
