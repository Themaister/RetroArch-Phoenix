#undef dllexport
#include <SDL/SDL.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

using namespace nall;

namespace ruby {

   class pInputDISDL {
      public:
         struct {
            LPDIRECTINPUT8 context;
            LPDIRECTINPUTDEVICE8 keyboard;
            SDL_Joystick *gamepad[Joypad::Count];
         } device;

         struct {
            HWND handle;
         } settings;

         bool cap(const string& name) {
            if(name == Input::Handle) return true;
            if(name == Input::KeyboardSupport) return true;
            if(name == Input::MouseSupport) return true;
            if(name == Input::JoypadSupport) return true;
            return false;
         }

         any get(const string& name) {
            if(name == Input::Handle) return (uintptr_t)settings.handle;
            return false;
         }

         bool set(const string& name, const any &value) {
            if(name == Input::Handle) {
               settings.handle = any_cast<HWND>(value);
               return true;
            }

            return false;
         }

         bool acquire() { return false; }
         bool unacquire() { return true; }
         bool acquired() { return false; }

         bool poll(int16_t *table) {
            memset(table, 0, Scancode::Limit * sizeof(int16_t));

            poll_joypad(table);
            poll_keyboard(table);
            
            return true;
         }

         bool init() {
            device.context = 0;
            device.keyboard = 0;
            DirectInput8Create(GetModuleHandle(0), 0x0800, IID_IDirectInput8, (void**)&device.context, 0);

            device.context->CreateDevice(GUID_SysKeyboard, &device.keyboard, 0);
            device.keyboard->SetDataFormat(&c_dfDIKeyboard);
            device.keyboard->SetCooperativeLevel(settings.handle, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
            device.keyboard->Acquire();

            SDL_InitSubSystem(SDL_INIT_VIDEO);
            SDL_InitSubSystem(SDL_INIT_JOYSTICK);
            SDL_JoystickEventState(SDL_IGNORE);

            unsigned joypads = min((unsigned)Joypad::Count, SDL_NumJoysticks());
            for(unsigned i = 0; i < joypads; i++) device.gamepad[i] = SDL_JoystickOpen(i);

            return true;
         }

         void term() {
            if(device.keyboard) {
               device.keyboard->Unacquire();
               device.keyboard->Release();
               device.keyboard = 0;
            }

            if(device.context) {
               device.context->Release();
               device.context = 0;
            }

            for(unsigned i = 0; i < Joypad::Count; i++) {
               if(device.gamepad[i]) SDL_JoystickClose(device.gamepad[i]);
               device.gamepad[i] = 0;
            }
            SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
         }

         pInputDISDL() {
            device.context = 0;
            device.keyboard = 0;
            settings.handle = 0;
            
            for(unsigned i = 0; i < Joypad::Count; i++) device.gamepad[i] = 0;
            settings.handle = 0;
         }

         ~pInputDISDL() {
            term();
         }

      private:
         void poll_keyboard(int16_t *table) {
            if(device.keyboard) {
               uint8_t state[256];
               if(FAILED(device.keyboard->GetDeviceState(sizeof state, state))) {
                  device.keyboard->Acquire();
                  if(FAILED(device.keyboard->GetDeviceState(sizeof state, state))) {
                     memset(state, 0, sizeof state);
                  }
               }

               #define key(id) table[keyboard(0)[id]]

               key(Keyboard::Escape) = (bool)(state[0x01] & 0x80);
               key(Keyboard::F1    ) = (bool)(state[0x3b] & 0x80);
               key(Keyboard::F2    ) = (bool)(state[0x3c] & 0x80);
               key(Keyboard::F3    ) = (bool)(state[0x3d] & 0x80);
               key(Keyboard::F4    ) = (bool)(state[0x3e] & 0x80);
               key(Keyboard::F5    ) = (bool)(state[0x3f] & 0x80);
               key(Keyboard::F6    ) = (bool)(state[0x40] & 0x80);
               key(Keyboard::F7    ) = (bool)(state[0x41] & 0x80);
               key(Keyboard::F8    ) = (bool)(state[0x42] & 0x80);
               key(Keyboard::F9    ) = (bool)(state[0x43] & 0x80);
               key(Keyboard::F10   ) = (bool)(state[0x44] & 0x80);
               key(Keyboard::F11   ) = (bool)(state[0x57] & 0x80);
               key(Keyboard::F12   ) = (bool)(state[0x58] & 0x80);

               key(Keyboard::PrintScreen) = (bool)(state[0xb7] & 0x80);
               key(Keyboard::ScrollLock ) = (bool)(state[0x46] & 0x80);
               key(Keyboard::Pause      ) = (bool)(state[0xc5] & 0x80);
               key(Keyboard::Tilde      ) = (bool)(state[0x29] & 0x80);

               key(Keyboard::Num1) = (bool)(state[0x02] & 0x80);
               key(Keyboard::Num2) = (bool)(state[0x03] & 0x80);
               key(Keyboard::Num3) = (bool)(state[0x04] & 0x80);
               key(Keyboard::Num4) = (bool)(state[0x05] & 0x80);
               key(Keyboard::Num5) = (bool)(state[0x06] & 0x80);
               key(Keyboard::Num6) = (bool)(state[0x07] & 0x80);
               key(Keyboard::Num7) = (bool)(state[0x08] & 0x80);
               key(Keyboard::Num8) = (bool)(state[0x09] & 0x80);
               key(Keyboard::Num9) = (bool)(state[0x0a] & 0x80);
               key(Keyboard::Num0) = (bool)(state[0x0b] & 0x80);

               key(Keyboard::Dash     ) = (bool)(state[0x0c] & 0x80);
               key(Keyboard::Equal    ) = (bool)(state[0x0d] & 0x80);
               key(Keyboard::Backspace) = (bool)(state[0x0e] & 0x80);

               key(Keyboard::Insert  ) = (bool)(state[0xd2] & 0x80);
               key(Keyboard::Delete  ) = (bool)(state[0xd3] & 0x80);
               key(Keyboard::Home    ) = (bool)(state[0xc7] & 0x80);
               key(Keyboard::End     ) = (bool)(state[0xcf] & 0x80);
               key(Keyboard::PageUp  ) = (bool)(state[0xc9] & 0x80);
               key(Keyboard::PageDown) = (bool)(state[0xd1] & 0x80);

               key(Keyboard::A) = (bool)(state[0x1e] & 0x80);
               key(Keyboard::B) = (bool)(state[0x30] & 0x80);
               key(Keyboard::C) = (bool)(state[0x2e] & 0x80);
               key(Keyboard::D) = (bool)(state[0x20] & 0x80);
               key(Keyboard::E) = (bool)(state[0x12] & 0x80);
               key(Keyboard::F) = (bool)(state[0x21] & 0x80);
               key(Keyboard::G) = (bool)(state[0x22] & 0x80);
               key(Keyboard::H) = (bool)(state[0x23] & 0x80);
               key(Keyboard::I) = (bool)(state[0x17] & 0x80);
               key(Keyboard::J) = (bool)(state[0x24] & 0x80);
               key(Keyboard::K) = (bool)(state[0x25] & 0x80);
               key(Keyboard::L) = (bool)(state[0x26] & 0x80);
               key(Keyboard::M) = (bool)(state[0x32] & 0x80);
               key(Keyboard::N) = (bool)(state[0x31] & 0x80);
               key(Keyboard::O) = (bool)(state[0x18] & 0x80);
               key(Keyboard::P) = (bool)(state[0x19] & 0x80);
               key(Keyboard::Q) = (bool)(state[0x10] & 0x80);
               key(Keyboard::R) = (bool)(state[0x13] & 0x80);
               key(Keyboard::S) = (bool)(state[0x1f] & 0x80);
               key(Keyboard::T) = (bool)(state[0x14] & 0x80);
               key(Keyboard::U) = (bool)(state[0x16] & 0x80);
               key(Keyboard::V) = (bool)(state[0x2f] & 0x80);
               key(Keyboard::W) = (bool)(state[0x11] & 0x80);
               key(Keyboard::X) = (bool)(state[0x2d] & 0x80);
               key(Keyboard::Y) = (bool)(state[0x15] & 0x80);
               key(Keyboard::Z) = (bool)(state[0x2c] & 0x80);

               key(Keyboard::LeftBracket ) = (bool)(state[0x1a] & 0x80);
               key(Keyboard::RightBracket) = (bool)(state[0x1b] & 0x80);
               key(Keyboard::Backslash   ) = (bool)(state[0x2b] & 0x80);
               key(Keyboard::Semicolon   ) = (bool)(state[0x27] & 0x80);
               key(Keyboard::Apostrophe  ) = (bool)(state[0x28] & 0x80);
               key(Keyboard::Comma       ) = (bool)(state[0x33] & 0x80);
               key(Keyboard::Period      ) = (bool)(state[0x34] & 0x80);
               key(Keyboard::Slash       ) = (bool)(state[0x35] & 0x80);

               key(Keyboard::Keypad1) = (bool)(state[0x4f] & 0x80);
               key(Keyboard::Keypad2) = (bool)(state[0x50] & 0x80);
               key(Keyboard::Keypad3) = (bool)(state[0x51] & 0x80);
               key(Keyboard::Keypad4) = (bool)(state[0x4b] & 0x80);
               key(Keyboard::Keypad5) = (bool)(state[0x4c] & 0x80);
               key(Keyboard::Keypad6) = (bool)(state[0x4d] & 0x80);
               key(Keyboard::Keypad7) = (bool)(state[0x47] & 0x80);
               key(Keyboard::Keypad8) = (bool)(state[0x48] & 0x80);
               key(Keyboard::Keypad9) = (bool)(state[0x49] & 0x80);
               key(Keyboard::Keypad0) = (bool)(state[0x52] & 0x80);
               key(Keyboard::Point  ) = (bool)(state[0x53] & 0x80);

               key(Keyboard::Add     ) = (bool)(state[0x4e] & 0x80);
               key(Keyboard::Subtract) = (bool)(state[0x4a] & 0x80);
               key(Keyboard::Multiply) = (bool)(state[0x37] & 0x80);
               key(Keyboard::Divide  ) = (bool)(state[0xb5] & 0x80);
               key(Keyboard::Enter   ) = (bool)(state[0x9c] & 0x80);

               key(Keyboard::NumLock ) = (bool)(state[0x45] & 0x80);
               key(Keyboard::CapsLock) = (bool)(state[0x3a] & 0x80);

               key(Keyboard::Up   ) = (bool)(state[0xc8] & 0x80);
               key(Keyboard::Down ) = (bool)(state[0xd0] & 0x80);
               key(Keyboard::Left ) = (bool)(state[0xcb] & 0x80);
               key(Keyboard::Right) = (bool)(state[0xcd] & 0x80);

               key(Keyboard::Tab     ) = (bool)(state[0x0f] & 0x80);
               key(Keyboard::Return  ) = (bool)(state[0x1c] & 0x80);
               key(Keyboard::Spacebar) = (bool)(state[0x39] & 0x80);
               key(Keyboard::Menu   ) = (bool)(state[0xdd] & 0x80);

               key(Keyboard::Shift  ) = (bool)(state[0x2a] & 0x80) || (bool)(state[0x36] & 0x80);
               key(Keyboard::Control) = (bool)(state[0x1d] & 0x80) || (bool)(state[0x9d] & 0x80);
               key(Keyboard::Alt    ) = (bool)(state[0x38] & 0x80) || (bool)(state[0xb8] & 0x80);
               key(Keyboard::Super  ) = (bool)(state[0xdb] & 0x80) || (bool)(state[0xdc] & 0x80);

               #undef key
            }
         }

         void poll_joypad(int16_t *table) {
            // Joypad
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
         }
   };

DeclareInput(DISDL)

}
