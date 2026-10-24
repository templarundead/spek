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

#include "spek-config.h"

#include <memory>
#include <utility>

#include <wx/string.h>

#include "spek-platform.h"

#include "spek-preferences.h"

SpekPreferences& SpekPreferences::get()
{
    static SpekPreferences instance;
    return instance;
}

void SpekPreferences::init()
{
    this->locale = std::make_unique<wxLocale>();

    int lang = wxLANGUAGE_DEFAULT;
    wxString code = this->get_language();
    if (spek_platform_can_change_language() && !code.IsEmpty()) {
        const wxLanguageInfo *info = wxLocale::FindLanguageInfo(code);
        if (info) {
            lang = info->Language;
        }
    }
    this->locale->Init(lang);
    this->locale->AddCatalog(SPEK_GETTEXT_PACKAGE);
}

SpekPreferences::SpekPreferences() :
    config(std::make_unique<wxFileConfig>(
        wxEmptyString,
        wxEmptyString,
        spek_platform_config_path("spek"),
        wxEmptyString,
        wxCONFIG_USE_LOCAL_FILE,
        wxConvUTF8
    ))
{
}

SpekPreferences::~SpekPreferences() = default;

bool SpekPreferences::get_show_toolbar()
{
    bool result = true;
    this->config->Read("/general/show_toolbar", &result);
    return result;
}

void SpekPreferences::set_show_toolbar(bool value)
{
    this->config->Write("/general/show_toolbar", value);
    this->config->Flush();
}

wxString SpekPreferences::get_language()
{
    wxString result("");
    this->config->Read("/general/language", &result);
    return result;
}

void SpekPreferences::set_language(const wxString& value)
{
    this->config->Write("/general/language", value);
    this->config->Flush();
}
