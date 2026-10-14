# Third party notices

This plugin's original sources are licensed under GPL-2.0-or-later.
The full GPL version 2 text is in LICENSE. Binary redistribution must include
the corresponding source under those terms.

## libebur128 1.2.6

Copyright (c) 2011 Jan Kokemüller. MIT license.
Source: https://github.com/jiixyj/libebur128
Vendored revision: 67b33abe1558160ed76ada1322329b0e9e058b02 (line endings may differ).
The unchanged measurement source is vendored in third_party/libebur128.
The complete MIT notice is in third_party/libebur128/COPYING and in the binary
package's licenses/libebur128-MIT.txt.

Its bundled queue/sys/queue.h has a BSD 3-Clause notice from The Regents of
the University of California (1991, 1993). Preserve the header and include
licenses/queue-BSD.txt with binary distributions.

## OBS Studio SDK

https://github.com/obsproject/obs-studio
GPL-2.0-or-later. Public headers and OBS runtime functions are used.
OBS runtime DLLs are not redistributed in this plugin package.

## Qt 6 Widgets

https://www.qt.io/licensing/open-source-lgpl-obligations
The plugin dynamically links Qt Core, Gui and Widgets provided by OBS.
Qt SDK/runtime DLLs are not included in the plugin distribution. Qt's applicable
LGPL/GPL/commercial terms and notices remain with the OBS/Qt distribution.
The local preview executable is a development tool; the downloaded Qt SDK
remains under deps/ and is excluded from source/binary release archives.

No code from Loudness Dock or Live Audio Assist is included in this plugin.
