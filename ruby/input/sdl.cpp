//================
//SDL input driver
//================
//Keyboard and mouse are controlled directly via Xlib,
//as SDL cannot capture input from windows it does not create itself.
//SDL is used only to handle joysticks / gamepads.

#include <SDL/SDL.h>
#include <sys/ipc.h>
#include <sys/shm.h>

namespace ruby {

struct pInputSDL {
  #include "xlibkeys.hpp"

  struct {
    Display *display;
    SDL_Joystick *gamepad[Joypad::Count];
  } device;

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

  bool poll(int16_t *table) {
    memset(table, 0, Scancode::Limit * sizeof(int16_t));

    //========
    //Keyboard
    //========

    x_poll(table);

    //=========
    //Joypad(s)
    //=========

    SDL_JoystickUpdate();
    for(unsigned i = 0; i < Joypad::Count; i++) {
      if(!device.gamepad[i]) continue;

      //POV hats
      unsigned hats = min((unsigned)Joypad::Hats, SDL_JoystickNumHats(device.gamepad[i]));
      for(unsigned hat = 0; hat < hats; hat++) {
        uint8_t state = SDL_JoystickGetHat(device.gamepad[i], hat);
        if(state & SDL_HAT_UP   ) table[joypad(i).hat(hat)] |= Joypad::HatUp;
        if(state & SDL_HAT_RIGHT) table[joypad(i).hat(hat)] |= Joypad::HatRight;
        if(state & SDL_HAT_DOWN ) table[joypad(i).hat(hat)] |= Joypad::HatDown;
        if(state & SDL_HAT_LEFT ) table[joypad(i).hat(hat)] |= Joypad::HatLeft;
      }

      //axes
      unsigned axes = min((unsigned)Joypad::Axes, SDL_JoystickNumAxes(device.gamepad[i]));
      for(unsigned axis = 0; axis < axes; axis++) {
        table[joypad(i).axis(axis)] = (int16_t)SDL_JoystickGetAxis(device.gamepad[i], axis);
      }

      //buttons
      for(unsigned button = 0; button < Joypad::Buttons; button++) {
        table[joypad(i).button(button)] = (bool)SDL_JoystickGetButton(device.gamepad[i], button);
      }
    }

    return true;
  }

  bool init() {
    x_init();
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_IGNORE);

    device.display = XOpenDisplay(0);

    unsigned joypads = min((unsigned)Joypad::Count, SDL_NumJoysticks());
    for(unsigned i = 0; i < joypads; i++) device.gamepad[i] = SDL_JoystickOpen(i);

    return true;
  }

  void term() {
    for(unsigned i = 0; i < Joypad::Count; i++) {
      if(device.gamepad[i]) SDL_JoystickClose(device.gamepad[i]);
      device.gamepad[i] = 0;
    }

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    XCloseDisplay(device.display);
  }

  pInputSDL() {
    for(unsigned i = 0; i < Joypad::Count; i++) device.gamepad[i] = 0;
    settings.handle = 0;
  }
};

DeclareInput(SDL)

};
