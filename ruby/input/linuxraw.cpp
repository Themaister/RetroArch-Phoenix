//================
//Linuxraw joypad input driver
//================
//Keyboard and mouse are controlled directly via Xlib.

#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <linux/joystick.h>

namespace ruby {

struct pInputLinuxRaw {
  #include "xlibkeys.hpp"

  struct Joystick {
    int fd;
    bool buttons[Joypad::Buttons];
    int16_t axes[Joypad::Axes];
  };

  struct {
    Display *display;
    Joystick gamepad[Joypad::Count];
  } device;

  int epfd;
  int notify;

  struct {
    uintptr_t handle;
  } settings;

  bool cap(const string& name) {
    if(name == Input::Handle) return true;
    if(name == Input::KeyboardSupport) return true;
    if(name == Input::JoypadSupport) return true;
    return false;
  }

  any get(const string& name) {
    if(name == Input::Handle) return (uintptr_t)settings.handle;
    return false;
  }

  bool set(const string& name, const any &value) {
    if(name == Input::Handle) {
      settings.handle = any_cast<uintptr_t>(value);
      return true;
    }

    return false;
  }

  bool acquire() {
     return true;
  }

  bool unacquire() {
     return true;
  }

  bool acquired() {
     return false;
  }

  void poll_pad(Joystick &pad)
  {
    struct js_event event;
    while (read(pad.fd, &event, sizeof(event)) == (ssize_t)sizeof(event))
    {
      unsigned type = event.type & ~JS_EVENT_INIT;

      switch (type)
      {
        case JS_EVENT_BUTTON:
          if (event.number < Joypad::Buttons)
            pad.buttons[event.number] = event.value;
          break;

        case JS_EVENT_AXIS:
          if (event.number < Joypad::Axes)
            pad.axes[event.number] = event.value;
          break;
      }
    }
  }

  void init_pad(const char *path, Joystick &pad)
  {
    if (pad.fd >= 0)
      return;

    // Device can be created, but no accessible (yet).
    // IN_ATTRIB will signal when permissions change.
    if (access(path, R_OK) < 0)
      return;

    pad.fd = open(path, O_RDONLY | O_NONBLOCK);

    if (pad.fd >= 0)
    {
      struct epoll_event event;
      event.events = EPOLLIN;
      event.data.ptr = &pad;
      epoll_ctl(epfd, EPOLL_CTL_ADD, pad.fd, &event);
    }
  }

  void handle_plugged_pad()
  {
    size_t event_size = sizeof(struct inotify_event) + NAME_MAX + 1;
    uint8_t *event_buf = (uint8_t*)calloc(1, event_size);
    if (!event_buf)
      return;

    int rc;
    while ((rc = read(notify, event_buf, event_size)) >= 0)
    {
      struct inotify_event *event = NULL;
      // Can read multiple events in one read() call.
      for (int i = 0; i < rc; i += event->len + sizeof(struct inotify_event))
      {
        event = (struct inotify_event*)&event_buf[i];

        if (strstr(event->name, "js") != event->name)
          continue;

        unsigned index = strtoul(event->name + 2, NULL, 0);
        if (index >= Joypad::Count)
          continue;

        if (event->mask & IN_DELETE)
        {
          if (device.gamepad[index].fd >= 0)
          {
            close(device.gamepad[index].fd);
            memset(&device.gamepad[index], 0, sizeof(device.gamepad[index]));
            device.gamepad[index].fd = -1;
          }
        }
        // Sometimes, device will be created before acess to it is established.
        else if (event->mask & (IN_CREATE | IN_ATTRIB))
        {
          char path[PATH_MAX];
          snprintf(path, sizeof(path), "/dev/input/%s", event->name);
          init_pad(path, device.gamepad[index]);
        }
      }
    }

    free(event_buf);
  }

  void joypad_poll()
  {
    int ret;
    struct epoll_event events[Joypad::Count + 1];

retry:
    ret = epoll_wait(epfd, events, Joypad::Count + 1, 0);
    if (ret < 0 && errno == EINTR)
      goto retry;

    for (int i = 0; i < ret; i++)
    {
      if (events[i].data.ptr)
        poll_pad(*(Joystick*)events[i].data.ptr);
      else
        handle_plugged_pad();
    }
  }

  void setup_notify()
  {
    fcntl(notify, F_SETFL, fcntl(notify, F_GETFL) | O_NONBLOCK);
    inotify_add_watch(notify, "/dev/input", IN_DELETE | IN_CREATE | IN_ATTRIB);
  }

  bool poll(int16_t *table) {
    memset(table, 0, Scancode::Limit * sizeof(int16_t));

    //========
    //Keyboard
    //========

    x_poll(table);

    //=========
    //Joypad(s)
    //=========

    joypad_poll();

    for (unsigned i = 0; i < Joypad::Count; i++) {
      if (device.gamepad[i].fd < 0)
        continue;

      for (unsigned b = 0; b < Joypad::Buttons; b++)
        table[joypad(i).button(b)] = device.gamepad[i].buttons[b];

      for (unsigned a = 0; a < Joypad::Axes; a++)
        table[joypad(i).axis(a)] = device.gamepad[i].axes[a];
    }

    return true;
  }

  bool init() {
    x_init();

    device.display = XOpenDisplay(0);

    epfd = epoll_create(Joypad::Count + 1);
    if (epfd < 0)
      return false;

    for (unsigned i = 0; i < Joypad::Count; i++)
    {
      Joystick &pad = device.gamepad[i];
      pad.fd = -1;

      char path[PATH_MAX];
      snprintf(path, sizeof(path), "/dev/input/js%u", i);

      init_pad(path, pad);
      if(pad.fd >= 0)
        poll_pad(pad);
    }

    notify = inotify_init();
    if (notify >= 0)
    {
      setup_notify();

      struct epoll_event event;
      event.events = EPOLLIN;
      event.data.ptr = 0;
      epoll_ctl(epfd, EPOLL_CTL_ADD, notify, &event);
    }

    return true;
  }

  void term() {
    for(unsigned i = 0; i < Joypad::Count; i++) {
      if(device.gamepad[i].fd >= 0)
        close(device.gamepad[i].fd);
      device.gamepad[i].fd = -1;
    }

    if(epfd >= 0)
      close(epfd);
    if(notify >= 0)
      close(notify);
    epfd = notify = -1;

    XCloseDisplay(device.display);
  }

  pInputLinuxRaw() {
    for(unsigned i = 0; i < Joypad::Count; i++)
      device.gamepad[i].fd = -1;
    epfd = -1;
    notify = -1;
    settings.handle = 0;
  }
};

DeclareInput(LinuxRaw)

};
