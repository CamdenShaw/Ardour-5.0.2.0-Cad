#include "ardour/ardour.h"
namespace ARDOUR {
const char* const ardour_config_info = "\n\
Build documentation: False\n\
Debuggable build: True\n\
Export all symbols (backtrace): True\n\
Install prefix: /usr/local\n\
Strict compiler flags: []\n\
Internal Shared Libraries: True\n\
Use External Libraries: False\n\
Library exports hidden: True\n\
ALSA DBus Reservation: True\n\
Architecture flags: None\n\
Aubio: True\n\
AudioUnits: False\n\
Free/Demo copy: False\n\
Build target: x86_64\n\
CoreAudio: False\n\
CoreAudio 10.5 compat: False\n\
Debug RT allocations: False\n\
Debug Symbols: True\n\
Process thread timing: False\n\
Denormal exceptions: False\n\
FLAC: True\n\
FPU optimization: True\n\
Freedesktop files: False\n\
Libjack linking: link\n\
Libjack metadata: True\n\
Lua Binding Doc: False\n\
LV2 UI embedding: True\n\
LV2 support: True\n\
LV2 extensions: True\n\
LXVST support: True\n\
OGG: True\n\
Phone home: True\n\
Program name: Ardour\n\
Samplerate: True\n\
PT format: False\n\
PTW32 Semaphore: False\n\
Translation: True\n\
Unit tests: False\n\
Mac i386 Architecture: False\n\
Mac ppc Architecture: False\n\
Windows VST support: False\n\
Wiimote support: True\n\
Windows key: Mod4><Super\n\
PortAudio Backend: False\n\
CoreAudio/Midi Backend: False\n\
ALSA Backend: True\n\
Dummy backend: False\n\
JACK Backend: True\n\
C compiler flags: ['-I/usr/local/music/src/ardour/4.0', '-DWAF_BUILD', '-g', '-std=c99', '-pedantic', '-Wshadow', '-Wall', '-Wcast-align', '-Wextra', '-Wwrite-strings', '-Wunsafe-loop-optimizations', '-Wlogical-op', '-fshow-column', '-DARCH_X86', '-mmmx', '-msse', '-mfpmath=sse', '-DUSE_XMMINTRIN', '-DBUILD_SSE_OPTIMIZATIONS', '-DLXVST_64BIT', '-Wall', '-Wpointer-arith', '-Wcast-qual', '-Wcast-align', '-Wno-unused-parameter', '-DBOOST_SYSTEM_NO_DEPRECATED', '-D_ISOC9X_SOURCE', '-D_LARGEFILE64_SOURCE', '-D_FILE_OFFSET_BITS=64', '-DENABLE_NLS', '-DPROGRAM_NAME=\"Ardour\"', u'-DPROGRAM_VERSION=\"5\"', '-Wstrict-prototypes', '-Wmissing-prototypes']\n\
C++ compiler flags: ['-I/usr/local/music/src/ardour/4.0', '-DWAF_BUILD', '-g', '-ansi', '-Wnon-virtual-dtor', '-Woverloaded-virtual', '-fstrict-overflow', '-Wall', '-Wcast-align', '-Wextra', '-Wwrite-strings', '-Wunsafe-loop-optimizations', '-Wlogical-op', '-fshow-column', '-DARCH_X86', '-mmmx', '-msse', '-mfpmath=sse', '-DUSE_XMMINTRIN', '-DBUILD_SSE_OPTIMIZATIONS', '-DLXVST_64BIT', '-Wall', '-Wpointer-arith', '-Wcast-qual', '-Wcast-align', '-Wno-unused-parameter', '-DBOOST_SYSTEM_NO_DEPRECATED', '-D_ISOC9X_SOURCE', '-D_LARGEFILE64_SOURCE', '-D_FILE_OFFSET_BITS=64', '-DENABLE_NLS', '-DPROGRAM_NAME=\"Ardour\"', u'-DPROGRAM_VERSION=\"5\"', '-Woverloaded-virtual', '-Wno-unused-local-typedefs', '-D__STDC_LIMIT_MACROS', '-D__STDC_FORMAT_MACROS', '-DCANVAS_COMPATIBILITY', '-DCANVAS_DEBUG']\n\
Linker flags: ['', '-rdynamic']\n\
";
}
