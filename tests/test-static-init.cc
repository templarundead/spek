// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Regression test for wxWidgets static initialization on Unix.
//
// This deliberately runs before main() and before wxEntry(): wxGTK 3.3.3
// could dereference wxConvFileName and use wxStandardPaths before their
// dynamic initializers had run while reading /etc/os-release.

#include <wx/string.h>
#include <wx/utils.h>

namespace
{
const wxString os_description = wxGetOsDescription();
}

int main()
{
    // Do not require a display or initialize wx just to test this path.
    return os_description.empty() ? 1 : 0;
}
