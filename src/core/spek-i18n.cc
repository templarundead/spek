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

#if SPEK_HAVE_INTL
#include <libintl.h>
#include <locale.h>
#endif

#include "spek-i18n.h"

namespace
{

std::string default_tr(const char *msgid)
{
    return std::string(msgid);
}

std::string default_tr_n(const char *msgid, const char *msgid_plural, unsigned long n)
{
    return std::string(n == 1 ? msgid : msgid_plural);
}

SpekTranslateFn g_tr = &default_tr;
SpekTranslatePluralFn g_tr_n = &default_tr_n;

} // namespace

void spek_set_translator(SpekTranslateFn tr, SpekTranslatePluralFn tr_n)
{
    g_tr = tr ? tr : &default_tr;
    g_tr_n = tr_n ? tr_n : &default_tr_n;
}

std::string spek_tr(std::string_view msgid)
{
    // gettext expects NUL-terminated strings, all msgids are literals.
    return g_tr(std::string(msgid).c_str());
}

std::string spek_tr_n(std::string_view singular, std::string_view plural, unsigned long n)
{
    return g_tr_n(std::string(singular).c_str(), std::string(plural).c_str(), n);
}

#if SPEK_HAVE_INTL
namespace
{

std::string gettext_tr(const char *msgid)
{
    return std::string(dgettext(SPEK_GETTEXT_PACKAGE, msgid));
}

std::string gettext_tr_n(const char *msgid, const char *msgid_plural, unsigned long n)
{
    return std::string(dngettext(SPEK_GETTEXT_PACKAGE, msgid, msgid_plural, n));
}

} // namespace

void spek_i18n_init_gettext()
{
    setlocale(LC_ALL, "");
    bindtextdomain(SPEK_GETTEXT_PACKAGE, SPEK_LOCALE_DIR);
    bind_textdomain_codeset(SPEK_GETTEXT_PACKAGE, "UTF-8");
    textdomain(SPEK_GETTEXT_PACKAGE);
    spek_set_translator(&gettext_tr, &gettext_tr_n);
}
#endif
