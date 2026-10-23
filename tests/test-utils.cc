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

#include <string>

#include "spek-utils.h"

#include "test.h"

static void test_vercmp()
{
    test<int>("1.2.3 == 1.2.3", 0, spek_vercmp("1.2.3", "1.2.3"));
    test("1.2.3 > 1.2.2", 1, spek_vercmp("1.2.3", "1.2.2"));
    test("1.2.2 < 1.2.3", -1, spek_vercmp("1.2.2", "1.2.3"));
    test("1.2.3 > 1", 1, spek_vercmp("1.2.3", "1"));
    test("1.2.3 > 1.", 1, spek_vercmp("1.2.3", "1."));
    test("1.2.3 > 1.2", 1, spek_vercmp("1.2.3", "1.2"));
    test("1.2.3 > 1.2.", 1, spek_vercmp("1.2.3", "1.2."));
    test("1.15.3 > 1.2", 1, spek_vercmp("1.15.3", "1.2"));
    test("2 > 1.2.2", 1, spek_vercmp("2", "1.2.2"));
    test("1.2.3 > ''", 1, spek_vercmp("1.2.3", ""));
    test("'' == ''", 0, spek_vercmp("", ""));
    test("123 == 123", 0, spek_vercmp("123", "123"));
    test("0.2.3 < 1", -1, spek_vercmp("0.2.3", "1"));
    test("0.9.8 < 0.10.1", -1, spek_vercmp("0.9.8", "0.10.1"));
    test("1.200 < 2.20", -1, spek_vercmp("1.200", "2.20"));
    test("1.0.0 < 2.0.0", -1, spek_vercmp("1.0.0", "2.0.0"));
    test("1.0.0 < 1.0.1", -1, spek_vercmp("1.0.0", "1.0.1"));
}

static void test_format()
{
    test<std::string>("no arguments", "hello", spek_format("%s", "hello"));
    test<std::string>("integer", "42 Hz", spek_format("%d Hz", 42));
    test<std::string>("two integers", "1 / 2", spek_format("%d / %d", 1, 2));
    test<std::string>("string", "a: b", spek_format("%s: %s", "a", "b"));
    test<std::string>("double", "0.5", spek_format("%.1f", 0.5));
}

static void test_extensions()
{
    test("extensions", true, spek_audio_extensions().size() > 10);
    test("flac", true, spek_has_audio_extension("song.flac"));
    test("upper case", true, spek_has_audio_extension("SONG.FLAC"));
    test("path", true, spek_has_audio_extension("/home/user/song.mp3"));
    test("dotted directory", false, spek_has_audio_extension("/home/some.dir/song"));
    test("unknown", false, spek_has_audio_extension("song.xyz"));
    test("no extension", false, spek_has_audio_extension("song"));
    test("empty", false, spek_has_audio_extension(""));
}

void test_utils()
{
    run("vercmp", test_vercmp);
    run("format", test_format);
    run("extensions", test_extensions);
}
