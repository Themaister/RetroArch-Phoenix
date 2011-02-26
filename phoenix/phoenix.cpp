#if defined(PHOENIX_WINDOWS)
  #define UNICODE
  #define WINVER 0x0501
  #define _WIN32_WINNT 0x0501
  #define _WIN32_IE 0x0600
  #define NOMINMAX

  #include <windows.h>
  #include <windowsx.h>
  #include <commctrl.h>
  #include <io.h>
  #include <shlobj.h>
#elif defined(PHOENIX_QT)
#ifndef __APPLE__
  #include <QApplication>
  #include <QtGui>
#else
  #include <QtGui/QApplication>
  #include <QtGui/QtGui>
#endif
#elif defined(PHOENIX_GTK)
  #define None X11None
  #define Window X11Window

  #include <gtk/gtk.h>
  #include <gdk/gdkx.h>
  #include <cairo.h>
  #include <gdk/gdkkeysyms.h>

  #undef None
  #undef Window
#elif defined(PHOENIX_REFERENCE)
#else
  #error "phoenix: unrecognized target"
#endif

#include "phoenix.hpp"
using namespace nall;

namespace phoenix {
  #include "core/core.cpp"
}
