% SPEK(1) User's Guide | Version 0.8.5
%
% 2023-01-09

# NAME

Spek - Acoustic Spectrum Analyser

# SYNOPSIS

`spek` [*OPTION* *...*] \[*FILE*]

# DESCRIPTION

*Spek* generates a spectrogram for the input audio file.

# OPTIONS

`-h`, `--help`
:   Output the help message then quit.

`-V`, `--version`
:   Output version information then quit.

`-c`, `--cli`
:   Run without the graphical interface, see *BATCH PROCESSING* below.

`-i`, `--input` *PATH*
:   Audio file or directory to analyse.

`-o`, `--output` *PATH*
:   PNG file to write, or a directory when analysing more than one file.

`-w`, `--width` *N*
:   Width of the image in pixels, 1024 by default.

`-H`, `--height` *N*
:   Height of the image in pixels, one pixel per FFT band by default.

`--fft-bits` *N*
:   Size of the DFT window: 2^*N* samples, 11 (2048 samples) by default.

`--stream` *N*
:   Audio stream to analyse, zero-based.

`--channel` *N*
:   Audio channel to analyse, zero-based.

`--min-db` *N*
:   Lower limit of the dynamic range in dBFS, -120 by default.

`--max-db` *N*
:   Upper limit of the dynamic range in dBFS, 0 by default.

`--palette` *NAME*
:   Palette to use: `sox` (the default), `spectrum` or `mono`.

`--window` *NAME*
:   DFT window function: `hann` (the default), `hamming` or `blackman-harris`.

`-r`, `--recursive`
:   Recurse into the sub-directories of the input directory.

`-j`, `--jobs` *N*
:   Number of files to analyse in parallel.

`-q`, `--quiet`
:   Do not print the progress information.

# BATCH PROCESSING

Without `--cli` *Spek* opens the graphical interface. When `--cli` (or
`--input`) is given, *Spek* analyses the files and saves the spectrograms as
PNG images without opening a window:

    spek --cli --input song.flac --output song.png
    spek --cli --input '~/Music' --output spectrograms --recursive --jobs 4

When the input is a directory, every file with a known audio extension is
analysed and the images are written into the output directory (or next to the
input files when `--output` is omitted), keeping the name of the source file.

# KEYBINDINGS

## Notes

On OS X use the Command key instead of Ctrl.

## Menu

`Ctrl-O`
:   Open a new file.

`Ctrl-S`
:   Save the spectrogram as an image file.

`Ctrl-E`
:   Show the preferences dialog.

`F1`
:   Open online manual in the browser.

`Shift-F1`
:   Show the about dialog.

## Spectrogram

`c`, `C`
:   Change the audio channel.

`f`, `F`
:   Change the DFT window function.

`l`, `L`
:   Change the lower limit of the dynamic range in dBFS.

`p`, `P`
:   Change the palette.

`s`, `S`
:   Change the audio stream.

`u`, `U`
:   Change the upper limit of the dynamic range in dBFS.

`w`, `W`
:   Change the DFT window size.

# FILES

*~/.config/spek/preferences*
:   The configuration file for *Spek*, stored in a simple INI format.

# AUTHORS

Alexander Kojevnikov <alexander@kojevnikov.com>. Other contributors are listed
in the CREDITS.md file distributed with the source code.

# DISTRIBUTION

The latest version of *Spek* may be downloaded from <https://www.spek.cc>.
