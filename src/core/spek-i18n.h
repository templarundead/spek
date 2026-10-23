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

#pragma once

#include <string>
#include <string_view>

// The core library is GUI- and toolkit-agnostic, so it cannot call into
// gettext (or into wxWidgets' own translation machinery) directly.
// Instead the application registers a translator at start-up and the
// core code uses spek_tr() / spek_tr_n() below.

using SpekTranslateFn = std::string (*)(const char *msgid);
using SpekTranslatePluralFn = std::string (*)(
    const char *msgid, const char *msgid_plural, unsigned long n);

// Installs the translator used by spek_tr() and spek_tr_n().
// Passing nullptr resets either function to the default (no translation).
void spek_set_translator(SpekTranslateFn tr, SpekTranslatePluralFn tr_n);

// Translates `msgid`, returns the original string when no translator is set.
std::string spek_tr(std::string_view msgid);

// Translates `singular` / `plural` depending on `n`.
std::string spek_tr_n(std::string_view singular, std::string_view plural, unsigned long n);

#if SPEK_HAVE_INTL
// Binds the gettext domain and uses libintl for the translation of the
// core strings. Only used by the command line interface, the GUI routes
// the translations through wxWidgets instead.
void spek_i18n_init_gettext();
#endif
