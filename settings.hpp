#ifndef __SSNES_SETTINGS_HPP
#define __SSNES_SETTINGS_HPP

#include "config_file.hpp"
#include <phoenix.hpp>
#include <utility>
#include "util.hpp"
#include "ruby/ruby.hpp"

#ifdef _WIN32
#define WIDGET_HEIGHT 24
#elif defined(__APPLE__)
#define WIDGET_HEIGHT 28
#else
#define WIDGET_HEIGHT 25
#endif

using namespace phoenix;
using namespace nall;

struct ToggleWindow : public Window
{
   ToggleWindow(const string& title) 
   { 
      setTitle(title); 
      onClose = [this]() { this->hide(); if (cb) cb(); }; 
      setGeometry({256, 256, 400, 240});
   }
   void show() { setVisible(); }
   void hide() { setVisible(false); }

   function<void ()> cb;
   void setCloseCallback(const function<void ()>& _cb) { cb = _cb; }
};

class SettingLayout : public util::SharedAbstract<SettingLayout>
{
   public:
      SettingLayout(ConfigFile &_conf, const string& _key, const string& _label, bool use_label = true) : conf(_conf), key(_key) 
      {
         label.setText(_label);
         if (use_label)
            hlayout.append(label, 225, WIDGET_HEIGHT);
      }
      HorizontalLayout& layout() { return hlayout; }

      virtual void update() = 0;

   protected:
      HorizontalLayout hlayout;
      ConfigFile &conf;
      string key;
      Label label;
};

class StringSetting : public SettingLayout, public util::Shared<StringSetting>
{
   public:
      StringSetting(ConfigFile &_conf, const string& _key, const string& label, const string& _default) : SettingLayout(_conf, _key, label), m_default(_default)
      {
         edit.onChange = [this]() { conf.set(key, edit.text()); };
         hlayout.append(edit, 0, WIDGET_HEIGHT); 
      }
      
      void update()
      {
         string tmp = m_default;
         conf.get(key, tmp);
         edit.setText(tmp);
      }

   private:
      TextEdit edit;
      string m_default;
};

class PathSetting : public SettingLayout, public util::Shared<PathSetting>
{
  public:
      PathSetting(ConfigFile &_conf, const string& _key, const string& label, const string& _default, const string& _filter) : SettingLayout(_conf, _key, label), m_default(_default), filter(_filter)
      {
         button.setText("Open ...");
         edit.onChange = [this]() { conf.set(key, edit.text()); };
         hlayout.append(edit, 0, WIDGET_HEIGHT); 
         hlayout.append(button, 100, WIDGET_HEIGHT);

         button.onTick = [this]() {
            string start_path;
            const char *path = std::getenv("HOME");
            if (path)
               start_path = path;
            string tmp = OS::fileLoad(Window::None, start_path, filter);
            if (tmp.length() > 0)
            {
               edit.setText(tmp);
               conf.set(key, tmp);
            }
         };
      }
      
      void update()
      {
         string tmp = m_default;
         conf.get(key, tmp);
         edit.setText(tmp);
      }

   private:
      TextEdit edit;
      Button button;
      string m_default;
      string filter;
};

class BoolSetting : public SettingLayout, public util::Shared<BoolSetting>
{
   public:
      BoolSetting(ConfigFile &_conf, const string& _key, const string& label, bool _default) : SettingLayout(_conf, _key, label), m_default(_default)
      {
         check.onTick = [this]() { conf.set(key, check.checked()); check.setText(check.checked() ? "Enabled" : "Disabled"); };
         hlayout.append(check, 120, WIDGET_HEIGHT);
      }

      void update()
      {
         bool tmp = m_default;
         conf.get(key, tmp);
         check.setChecked(tmp);
         check.setText(tmp ? "Enabled" : "Disabled");
      }

   private:
      CheckBox check;
      bool m_default;
};

class IntSetting : public SettingLayout, public util::Shared<IntSetting>
{
   public:
      IntSetting(ConfigFile &_conf, const string& _key, const string& label, int _default) : SettingLayout(_conf, _key, label), m_default(_default)
      {
         edit.onChange = [this]() { conf.set(key, (int)nall::decimal(edit.text())); };
         hlayout.append(edit, 0, WIDGET_HEIGHT);
      }

      void update()
      {
         int tmp = m_default;
         conf.get(key, tmp);
         edit.setText((unsigned)tmp);
      }

   private:
      TextEdit edit;
      int m_default;
};

class DoubleSetting : public SettingLayout, public util::Shared<DoubleSetting>
{
   public:
      DoubleSetting(ConfigFile &_conf, const string& _key, const string& label, double _default) 
         : SettingLayout(_conf, _key, label), m_default(_default)
      {
         edit.onChange = [this]() { conf.set(key, edit.text()); };
         hlayout.append(edit, 0, WIDGET_HEIGHT);
      }

      void update()
      {
         double tmp = m_default;
         conf.get(key, tmp);
         edit.setText(tmp);
      }

   private:
      TextEdit edit;
      double m_default;
};

namespace Internal
{
   struct combo_selection
   {
      string internal_name;
      string external_name;
   };

   struct input_selection
   {
      string config_base;
      string base;
      string display;
   };
}

class ComboSetting : public SettingLayout, public util::Shared<ComboSetting>
{
   public:
      ComboSetting(ConfigFile &_conf, const string& _key, const string& label, const linear_vector<Internal::combo_selection>& _list, unsigned _default) 
         : SettingLayout(_conf, _key, label), m_default(_default), list(_list)
      {
         foreach(i, list) box.append(i.external_name);
         box.setSelection(m_default);

         box.onChange = [this]() {
            conf.set(key, list[box.selection()].internal_name);
         };

         hlayout.append(box, 200, WIDGET_HEIGHT);
      }

      void update()
      {
         string tmp;
         conf.get(key, tmp);
         unsigned index = m_default;
         for (unsigned i = 0; i < list.size(); i++)
         {
            if (list[i].internal_name == tmp)
            {
               index = i;
               break;
            }
         }

         box.setSelection(index);
         conf.set(key, list[box.selection()].internal_name);
      }

   private:
      ComboBox box;
      unsigned m_default;   
      const linear_vector<Internal::combo_selection>& list;
};

namespace Internal
{
   static const char keymap[][64] = {
      "escape", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12",
      "print_screen", "scroll_lock", "pause", "tilde",
      "num1", "num2", "num3", "num4", "num5", "num6", "num7", "num8", "num9", "num0",
      "dash", "equal", "backspace",
      "insert", "del", "home", "end", "pageup", "pagedown",
      "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
      "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
      "leftbracket", "rightbracket", "backslash", "semicolon", "apostrophe", "comma", "period", "slash",
      "keypad1", "keypad2", "keypad3", "keypad4", "keypad5", "keypad6", "keypad7", "keypad8", "keypad9", "keypad0",
      "point", "enter", "add", "subtract", "multiply", "divide",
      "numlock", "capslock",
      "up", "down", "left", "right",
      "tab", "enter", "space", "menu",
      "shift", "ctrl", "alt", "super",
   };
}

class InputSetting : public SettingLayout, public util::Shared<InputSetting>
{
   public:
      InputSetting(ConfigFile &_conf, const linear_vector<linear_vector<Internal::input_selection>>& _list, const function<void (const string&)> _msg_cb)
         : SettingLayout(_conf, "", "Binds:", false), list(_list), msg_cb(_msg_cb)
      {
         clear.setText("Clear");
         player.append("Player 1");
         player.append("Player 2");
         player.append("Misc");
         hbox.append(player, 120, WIDGET_HEIGHT, 10);
         hbox.append(clear, 60, WIDGET_HEIGHT);

         player.onChange = [this]() { this->update_list(); };
         clear.onTick = [this]() {
            auto j = list_view.selection();
            unsigned i = player.selection();
            if (j)
            {
               list[i][j()].display = "None";
               conf.set(list[i][j()].config_base, string(""));
               conf.set({list[i][j()].config_base, "_btn"}, string(""));
               conf.set({list[i][j()].config_base, "_axis"}, string(""));
               this->update_list();
            }
         };

         list_view.setHeaderText("SSNES Bind", "Bind");
         list_view.setHeaderVisible();
         list_view.onActivate = [this]() { this->update_bind(); };

         //vbox.append(label, 120, WIDGET_HEIGHT, 0);
         vbox.append(hbox, 0, 0);
         vbox.append(list_view, 0, 300);
         hlayout.append(vbox, 0, 400);
      }

      void update()
      {
         update_from_config();
         update_list();
      }

      
   private:
      function<void (const string&)> msg_cb;
      HorizontalLayout hbox;
      VerticalLayout vbox;
      ListView list_view;
      ComboBox player;
      Button clear;
      linear_vector<linear_vector<Internal::input_selection>> list;

      enum class Type
      {
         Keyboard,
         JoyButton,
         JoyAxis,
         JoyHat,
         None
      };
      
      void update_list()
      {
         list_view.reset();

         foreach(i, list[player.selection()])
         {
            list_view.append(i.base, i.display);
         }
         list_view.autosizeColumns();
      }

      void update_bind()
      {
         const string& opt = list[player.selection()][list_view.selection()()].config_base;
         msg_cb({"Activate for bind: ", opt});

         while (OS::pending()) OS::process();

         auto& elem = list[player.selection()][list_view.selection()()];
         string option, ext;
         auto type = poll(option);
         switch (type)
         {
            case Type::JoyAxis:
               conf.set({elem.config_base, "_axis"}, option);
               conf.set({elem.config_base, "_btn"}, string(""));
               conf.set({elem.config_base, ""}, string(""));
               ext = " (axis)";
               break;

            case Type::JoyButton:
               conf.set({elem.config_base, "_axis"}, string(""));
               conf.set({elem.config_base, "_btn"}, option);
               conf.set({elem.config_base, ""}, string(""));
               ext = " (button)";
               break;

            case Type::JoyHat:
               conf.set({elem.config_base, "_axis"}, string(""));
               conf.set({elem.config_base, "_btn"}, option);
               conf.set({elem.config_base, ""}, string(""));
               ext = " (hat)";
               break;

            case Type::Keyboard:
               conf.set({elem.config_base, "_axis"}, string(""));
               conf.set({elem.config_base, "_btn"}, string(""));
               conf.set({elem.config_base, ""}, option);
               break;

            default:
               break;
         }

         elem.display = option;
         if (ext.length() > 0)
            elem.display.append(ext);

         update_list();
         msg_cb("");
      }

      Type poll(string& opt)
      {
         auto type = Type::None;
         int16_t old_data[Scancode::Limit] = {0};
         int16_t new_data[Scancode::Limit] = {0};
         opt = "";

         ruby::input.poll(old_data);

         for (;;)
         {
            // TODO: Find some better way to block ... xD
            usleep(10000);

            ruby::input.poll(new_data);
            for (int i = 0; i < Scancode::Limit; i++)
            {
               if (old_data[i] != new_data[i])
               {
                  if (Mouse::isAnyButton(i) || Mouse::isAnyAxis(i))
                     continue;

                  if (Joypad::isAnyAxis(i))
                  {
                     if (std::abs((int)old_data[i] - (int)new_data[i]) < 20000)
                        continue;

                     type = Type::JoyAxis;
                  }
                  else if (Joypad::isAnyButton(i))
                     type = Type::JoyButton;
                  else if (Joypad::isAnyHat(i))
                     type = Type::JoyHat;
                  else
                     type = Type::Keyboard;

                  if ((Joypad::isAnyAxis(i) || Joypad::isAnyButton(i) || Joypad::isAnyHat(i)) 
                        && player.selection() <= 1)
                  {
                     conf.set(string("input_player", 
                           (unsigned)(player.selection() + 1), "_joypad_index"), 
                           Joypad::numberDecode(i));
                  }

                  signed code = 0;
                  if ((code = Joypad::buttonDecode(i)) >= 0)
                     opt = string((unsigned)code);
                  else if ((code = Joypad::axisDecode(i)) >= 0)
                  {
                     if (old_data[i] < new_data[i])
                        opt = string("+", (unsigned)code);
                     else
                        opt = string("-", (unsigned)code);
                  }
                  else if ((code = Joypad::hatDecode(i)) >= 0)
                  {
                     opt = {"h", (unsigned)code};
                     int16_t j = new_data[i];
                     if (j & Joypad::HatUp)
                        opt.append("up");
                     else if (j & Joypad::HatDown)
                        opt.append("down");
                     else if (j & Joypad::HatRight)
                        opt.append("right");
                     else if (j & Joypad::HatLeft)
                        opt.append("left");
                  }
                  else
                  {
                     opt = encode(i);
                  }

                  return type;
               }
            }
         }
      }

      void update_from_config()
      {
         foreach(i, list)
         {
            foreach(j, i)
            {
               string tmp;
               if (conf.get(j.config_base, tmp))
                  j.display = tmp;
               else if (conf.get({j.config_base, "_btn"}, tmp))
                  j.display = {tmp, " (button)"};
               else if (conf.get({j.config_base, "_axis"}, tmp))
                  j.display = {tmp, " (axis)"};
               else
                  j.display = "None";
            }
         }
      }

      static string encode(unsigned i)
      {
         auto j = Keyboard::keyDecode(i);
         if (j >= 0)
            return Internal::keymap[j];
         else
            return ":v";
      }
};

class General : public ToggleWindow
{
   public:
      General(ConfigFile &_conf) : ToggleWindow("SSNES || General settings")
      {
         widgets.append(BoolSetting::shared(_conf, "rewind_enable", "Enable rewind", false));
         widgets.append(IntSetting::shared(_conf, "rewind_buffer_size", "Rewind buffer size (MB)", 20));
         widgets.append(IntSetting::shared(_conf, "rewind_granularity", "Rewind frames granularity", 1));
         widgets.append(BoolSetting::shared(_conf, "pause_nonactive", "Pause when window loses focus", true));
         widgets.append(IntSetting::shared(_conf, "autosave_interval", "Autosave interval (seconds)", 0));
         foreach(i, widgets) { vbox.append(i->layout(), 0, 0, 3); }

         vbox.setMargin(5);
         append(vbox);
      }

      void update() { foreach(i, widgets) i->update(); }

   private:
      linear_vector<SettingLayout::APtr> widgets;
      VerticalLayout vbox;

};

class Video : public ToggleWindow
{
   public:
      Video(ConfigFile &_conf) : ToggleWindow("SSNES || Video settings")
      {
         setGeometry({256, 256, 600, 500});
         widgets.append(DoubleSetting::shared(_conf, "video_xscale", "Windowed X scale", 3.0));
         widgets.append(DoubleSetting::shared(_conf, "video_xscale", "Windowed Y scale", 3.0));
         widgets.append(IntSetting::shared(_conf, "video_fullscreen_x", "Fullscreen X resolution", 1280));
         widgets.append(IntSetting::shared(_conf, "video_fullscreen_y", "Fullscreen Y resolution", 720));
         widgets.append(BoolSetting::shared(_conf, "video_fullscreen", "Start in fullscreen", false));
         widgets.append(BoolSetting::shared(_conf, "video_smooth", "Bilinear filtering", true));
         widgets.append(BoolSetting::shared(_conf, "video_force_aspect", "Lock aspect ratio", true));
         widgets.append(DoubleSetting::shared(_conf, "video_aspect_ratio", "Aspect ratio", 1.333));
         widgets.append(PathSetting::shared(_conf, "video_cg_shader", "Cg pixel shader", "", "Cg shader (*.cg)"));
         widgets.append(PathSetting::shared(_conf, "video_bsnes_shader", "bSNES XML shader", "", "XML shader (*.shader)"));
         widgets.append(PathSetting::shared(_conf, "video_font_path", "Path to on-screen font", "", "TTF font (*.ttf)"));
         widgets.append(IntSetting::shared(_conf, "video_font_size", "On-screen font size", 48));
         widgets.append(DoubleSetting::shared(_conf, "video_message_pos_x", "On-screen message pos X", 0.05));
         widgets.append(DoubleSetting::shared(_conf, "video_message_pos_y", "On-screen message pos Y", 0.05));

         foreach(i, widgets) { vbox.append(i->layout(), 0, 0, 3); }
         vbox.setMargin(5);
         append(vbox);
      }

      void update() { foreach(i, widgets) i->update(); }

   private:
      linear_vector<SettingLayout::APtr> widgets;
      VerticalLayout vbox;

};


namespace Internal
{
   static const linear_vector<combo_selection> audio_drivers = {
#ifdef _WIN32
      {"sdl", "SDL"},
      {"xaudio", "XAudio2"},
#elif defined(__APPLE__)
      {"openal", "OpenAL"},
      {"sdl", "SDL"},
#else
      {"alsa", "ALSA"},
      {"pulse", "PulseAudio"},
      {"oss", "Open Sound System"},
      {"jack", "JACK Audio"},
      {"rsound", "RSound"},
      {"roar", "RoarAudio"},
      {"openal", "OpenAL"},
      {"sdl", "SDL"},

#endif
   };

   static const linear_vector<input_selection> player1 = {
      { "input_player1_a", "A (right)", "" },
      { "input_player1_b", "B (down)", "" },
      { "input_player1_y", "Y (left)", "" },
      { "input_player1_x", "X (up)", "" },
      { "input_player1_start", "Start", "" },
      { "input_player1_select", "Select", "" },
      { "input_player1_l", "L", "" },
      { "input_player1_r", "R", "" },
      { "input_player1_left", "D-pad Left", "" },
      { "input_player1_right", "D-pad Right", "" },
      { "input_player1_up", "D-pad Up", "" },
      { "input_player1_down", "D-pad Down", "" },
   };

   static const linear_vector<input_selection> player2 = {
      { "input_player2_a", "A (right)", "" },
      { "input_player2_b", "B (down)", "" },
      { "input_player2_y", "Y (left)", "" },
      { "input_player2_x", "X (up)", "" },
      { "input_player2_start", "Start", "" },
      { "input_player2_select", "Select", "" },
      { "input_player2_l", "L", "" },
      { "input_player2_r", "R", "" },
      { "input_player2_left", "D-pad Left", "" },
      { "input_player2_right", "D-pad Right", "" },
      { "input_player2_up", "D-pad Up", "" },
      { "input_player2_down", "D-pad Down", "" },
   };

   static const linear_vector<input_selection> misc = {
      { "input_save_state", "Save state", "" },
      { "input_load_state", "Load state", "" },
      { "input_state_slot_increase", "Increase state slot", "" },
      { "input_state_slot_decrease", "Decrease state slot", "" },
      { "input_toggle_fast_forward", "Toggle fast forward", "" },
      { "input_exit_emulator", "Exit emulator", "" },
      { "input_toggle_fullscreen", "Toggle fullscreen", "" },
      { "input_pause_toggle", "Pause toggle", "" },
      { "input_movie_record_toggle", "Movie record toggle", "" },
      { "input_rate_step_up", "Audio input rate step up", "" },
      { "input_rate_step_down", "Audio input rate step down", "" },
      { "input_rewind", "Rewind", "" },
   };

   static const linear_vector<linear_vector<input_selection>> binds = { player1, player2, misc };
}

class Audio : public ToggleWindow
{
   public:
      Audio(ConfigFile &_conf) : ToggleWindow("SSNES || Audio settings")
      {
         setGeometry({256, 256, 450, 300});
         widgets.append(BoolSetting::shared(_conf, "audio_enable", "Enable audio", true));
         widgets.append(IntSetting::shared(_conf, "audio_out_rate", "Audio sample rate", 48000));
         widgets.append(DoubleSetting::shared(_conf, "audio_in_rate", "Audio input rate", 31980.0));
         widgets.append(DoubleSetting::shared(_conf, "audio_rate_step", "Audio rate step", 0.25));
         widgets.append(ComboSetting::shared(_conf, "audio_driver", "Audio driver", Internal::audio_drivers, 0));
         widgets.append(StringSetting::shared(_conf, "audio_device", "Audio device", ""));
         widgets.append(BoolSetting::shared(_conf, "audio_sync", "Audio sync", true));
         widgets.append(IntSetting::shared(_conf, "audio_latency", "Audio latency (ms)", 64));
         //widgets.append(IntSetting::shared(_conf, "audio_src_quality", "libsamplerate quality (1 - worst, 5 - best)", 2));

         foreach(i, widgets) { vbox.append(i->layout(), 0, 0, 3); }
         vbox.setMargin(5);
         append(vbox);
      }

      void update() { foreach(i, widgets) i->update(); }

   private:
      linear_vector<SettingLayout::APtr> widgets;
      VerticalLayout vbox;

};


class Input : public ToggleWindow
{
   public:
      Input(ConfigFile &_conf) : ToggleWindow("SSNES || Input settings")
      {
         setGeometry({256, 256, 400, 400});
         widgets.append(DoubleSetting::shared(_conf, "input_axis_threshold", "Input axis threshold (0.0 to 1.0)", 0.5));
         widgets.append(BoolSetting::shared(_conf, "netplay_client_swap_input", "Use Player 1 binds as client", false));
         widgets.append(InputSetting::shared(_conf, Internal::binds, 
                  [this](const string& msg) { this->setStatusText(msg); }));

         foreach(i, widgets) { vbox.append(i->layout(), 0, 0, 3); }
         vbox.setMargin(5);
         append(vbox);

         init_input();

         setStatusVisible();
      }

      void update() { foreach(i, widgets) i->update(); }

   private:
      linear_vector<SettingLayout::APtr> widgets;
      VerticalLayout vbox;

      void init_input()
      {
#ifdef _WIN32
         ruby::input.driver("DirectInput");
#else
         ruby::input.driver("SDL");
#endif
         ruby::input.init();
      }
};


#endif
