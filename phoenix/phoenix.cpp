#ifndef PHOENIX_CPP
#define PHOENIX_CPP

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
#ifdef __APPLE__
  #include <QtGui/QApplication>
  #include <QtGui/QtGui>
#else
  #include <QApplication>
  #include <QtGui>
#endif
#elif defined(PHOENIX_GTK)
  #define None
  #define Window X11Window
  #define X11None 0L

  #include <gtk/gtk.h>
  #include <gdk/gdk.h>
  #include <gdk/gdkx.h>
  #include <cairo.h>
  #include <X11/Xatom.h>
  #include <gdk/gdkkeysyms.h>

  // Support legacy version of gdk/gdkkeysyms.h (Debian Squeeze, Ubuntu 10.04 
  // among others)
  #ifndef GDK_KEY_Home
  #define GDK_KEY_Home GDK_Home
  #define GDK_KEY_End GDK_End
  #define GDK_KEY_Up GDK_Up
  #define GDK_KEY_Down GDK_Down
  #define GDK_KEY_Page_Up GDK_Page_Up
  #define GDK_KEY_Page_Down GDK_Page_Down
  #endif

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

#endif
