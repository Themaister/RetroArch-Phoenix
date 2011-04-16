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
#define WIDGET_HEIGHT 28
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
         clear.setText("Clear");
         edit.onChange = [this]() { conf.set(key, edit.text()); };
         hlayout.append(edit, 0, WIDGET_HEIGHT); 
         hlayout.append(clear, 80, WIDGET_HEIGHT);
         hlayout.append(button, 100, WIDGET_HEIGHT);

         edit.setEditable(false);

         button.onTick = [this]() {
            string start_path;
            const char *path = std::getenv("HOME");
            char base_path[1024];
            nall::strlcpy(base_path, edit.text(), sizeof(base_path));
            char *base_ptr = strrchr(base_path, '/');
            if (!base_ptr)
               base_ptr = strrchr(base_path, '\\');
            if (base_ptr)
               *base_ptr = '\0';

            if (strlen(base_path) > 0)
               start_path = base_path;
            else if (path)
               start_path = path;
            string tmp = OS::fileLoad(Window::None, start_path, filter);
            if (tmp.length() > 0)
            {
               edit.setText(tmp);
               conf.set(key, tmp);
            }
         };

         clear.onTick = [this]() {
            conf.set(key, string(""));
            edit.setText(string(""));
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
      Button clear;
      string m_default;
      string filter;
};

class DirSetting : public SettingLayout, public util::Shared<DirSetting>
{
   public:
      DirSetting(ConfigFile &_conf, const string& _key, const string& label, const string& _default) : SettingLayout(_conf, _key, label), m_default(_default)
      {
         button.setText("Open ...");
         clear.setText("Clear");
         edit.onChange = [this]() { conf.set(key, edit.text()); };
         hlayout.append(edit, 0, WIDGET_HEIGHT); 
         hlayout.append(clear, 80, WIDGET_HEIGHT);
         hlayout.append(button, 100, WIDGET_HEIGHT);

         edit.setEditable(false);

         button.onTick = [this]() {
            string start_path;
            const char *path = std::getenv("HOME");
            char base_path[1024];
            nall::strlcpy(base_path, edit.text(), sizeof(base_path));
            char *base_ptr = strrchr(base_path, '/');
            if (!base_ptr)
               base_ptr = strrchr(base_path, '\\');
            if (base_ptr)
               *base_ptr = '\0';

            if (strlen(base_path) > 0)
               start_path = base_path;
            else if (path)
               start_path = path;
            string tmp = OS::folderSelect(Window::None, start_path);
            if (tmp.length() > 0)
            {
               edit.setText(tmp);
               conf.set(key, tmp);
            }
         };

         clear.onTick = [this]() {
            conf.set(key, string(""));
            edit.setText(string(""));
         };
      }

      void update()
      {
         string tmp = m_default;
         conf.get(key, tmp);
         edit.setText(tmp);
      }

      TextEdit edit;
   private:
      Button button;
      Button clear;
      string m_default;
      string filter;
};


struct myRadioBox : public RadioBox, public util::Shared<myRadioBox>
{
   myRadioBox(ConfigFile& _conf, const string& _config_key, const string& _key = "") : conf(_conf), config_key(_config_key), key(_key) 
   {
      onTick = [this]() { conf.set(config_key, key); };
   }
   ConfigFile& conf;
   string config_key;
   string key;
};

class ShaderSetting : public SettingLayout, public util::Shared<ShaderSetting>
{
   public:
      ShaderSetting(ConfigFile &_conf, const string& _key, const linear_vector<string>& _values, const linear_vector<string>& _keys, linear_vector<PathSetting::Ptr>& _elems)
         : SettingLayout(_conf, _key, "", false), elems(_elems)
      {
         foreach(i, elems)
            vbox.append(i->layout(), 0, 0);

         assert(_keys.size() == _values.size());

         label.setText("Shader:");
         hbox.append(label, 120, WIDGET_HEIGHT);
         for (unsigned i = 0; i < _keys.size(); i++)
         {
            auto box = myRadioBox::shared(conf, key, _keys[i]);
            box->setText(_values[i]);
            boxes.append(box);
            hbox.append(*box, 120, WIDGET_HEIGHT);
         }

         reference_array<RadioBox&> list;
         foreach(i, boxes)
         {
            list.append(*i);
         }
         RadioBox::group(list);
         boxes[0]->setChecked();

         vbox.append(hbox, 0, 120);
         hlayout.append(vbox, 0, 3 * WIDGET_HEIGHT);
      }

      void update()
      {
         foreach(i, elems)
            i->update();

         string tmp;
         if (conf.get(key, tmp))
         {
            foreach(i, boxes)
            {
               if (i->key == tmp)
               {
                  i->setChecked();
                  break;
               }
            }
         }
         else
            boxes[0]->setChecked();
      }

   private:
      HorizontalLayout hbox;
      VerticalLayout vbox;
      linear_vector<myRadioBox::Ptr> boxes;
      linear_vector<PathSetting::Ptr>& elems;
      Label label;
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

      CheckBox check;
   private:
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
      InputSetting(ConfigFile &_conf, const linear_vector<linear_vector<Internal::input_selection>>& _list, const function<void (const string&)>& _msg_cb, const function<void ()>& _focus_cb)
         : SettingLayout(_conf, "", "Binds:", false), list(_list), msg_cb(_msg_cb), focus_cb(_focus_cb)
      {
         clear.setText("Clear");
         def.setText("Default");
         def_all.setText("Default all");
         erase.setText("Clear all");
         player.append("Player 1");
         player.append("Player 2");
         player.append("Player 3");
         player.append("Player 4");
         player.append("Player 5");
         player.append("Misc");
         hbox.append(player, 120, WIDGET_HEIGHT, 10);
         hbox.append(def, 80, WIDGET_HEIGHT);
         hbox.append(def_all, 80, WIDGET_HEIGHT);
         hbox.append(clear, 80, WIDGET_HEIGHT);
         hbox.append(erase, 80, WIDGET_HEIGHT);

         player.onChange = [this]() { this->update_list(); };

         clear.onTick = [this]() { this->set_single("None", "nul"); };
         def.onTick = [this]() { this->set_single("Default", ""); };

         def_all.onTick = [this]() { this->set_all_list("Default", ""); };
         erase.onTick = [this]() { this->set_all_list("None", "nul"); };

         list_view.setHeaderText(string("SSNES Bind"), string("Bind"));
         list_view.setHeaderVisible();
         list_view.onActivate = [this]() { this->update_bind(); };

         vbox.append(hbox, 0, 0);

         index_label.setText("Joypad #:");
         hbox_index.append(index_label, 60, WIDGET_HEIGHT);
         index_show.setEditable(false);
         hbox_index.append(index_show, 60, WIDGET_HEIGHT);
         vbox.append(hbox_index, 0, 0);

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
      function<void ()> focus_cb;
      HorizontalLayout hbox;
      VerticalLayout vbox;
      ListView list_view;
      ComboBox player;
      Button clear;
      Button def_all;
      Button def;
      Button erase;

      Label index_label;
      LineEdit index_show;
      HorizontalLayout hbox_index;

      linear_vector<linear_vector<Internal::input_selection>> list;

      void set_single(const string& display, const string& conf_string)
      {
         auto j = list_view.selection();
         unsigned i = player.selection();
         if (list_view.selected())
         {
            list[i][j].display = display;
            conf.set(list[i][j].config_base, conf_string);
            conf.set({list[i][j].config_base, "_btn"}, conf_string);
            conf.set({list[i][j].config_base, "_axis"}, conf_string);
            this->update_list();
         }
      }

      void set_all_list(const string& display, const string& conf_string)
      {
         foreach(i, list)
         {
            foreach(j, i)
            {
               j.display = display;
               conf.set(j.config_base, conf_string);
               conf.set({j.config_base, "_btn"}, conf_string);
               conf.set({j.config_base, "_axis"}, conf_string);
            }
         }
         
         conf.set("input_player1_joypad_index", (int)0);
         conf.set("input_player2_joypad_index", (int)1);
         conf.set("input_player3_joypad_index", (int)2);
         conf.set("input_player4_joypad_index", (int)3);
         conf.set("input_player5_joypad_index", (int)4);

         this->update_list();
      }

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
         list_view.autoSizeColumns();

         if (player.selection() == max_players) // Misc, only player 1
         {
            index_show.setText("0");
         }
         else
         {
            string index_conf_str {"input_player", (unsigned)(player.selection() + 1), "_joypad_index"};
            string tmp;
            if (conf.get(index_conf_str, tmp))
               index_show.setText(tmp);
            else
               index_show.setText((unsigned)player.selection());
         }
      }

      void update_bind()
      {
         const string& opt = list[player.selection()][list_view.selection()].config_base;
         msg_cb({"Activate for bind: ", opt});

         focus_cb();

         auto& elem = list[player.selection()][list_view.selection()];
         list_view.setSelected(false);
         if (OS::pendingEvents()) OS::processEvents();

         string option, ext;
         auto type = poll(option);
         switch (type)
         {
            case Type::JoyAxis:
               conf.set({elem.config_base, "_axis"}, option);
               conf.set({elem.config_base, "_btn"}, string("nul"));
               conf.set({elem.config_base, ""}, string("nul"));
               ext = " (axis)";
               break;

            case Type::JoyButton:
               conf.set({elem.config_base, "_axis"}, string("nul"));
               conf.set({elem.config_base, "_btn"}, option);
               conf.set({elem.config_base, ""}, string("nul"));
               ext = " (button)";
               break;

            case Type::JoyHat:
               conf.set({elem.config_base, "_axis"}, string("nul"));
               conf.set({elem.config_base, "_btn"}, option);
               conf.set({elem.config_base, ""}, string("nul"));
               ext = " (hat)";
               break;

            case Type::Keyboard:
               conf.set({elem.config_base, "_axis"}, string("nul"));
               conf.set({elem.config_base, "_btn"}, string("nul"));
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
                        && player.selection() < max_players)
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
               string tmp[3];
               bool is_nul[3] = {false};
               bool is_valid[3] = {false};
               linear_vector<string> appends = {"", "_btn", "_axis"};

               foreach(i, appends, count)
               {
                  if (conf.get({j.config_base, i}, tmp[count]))
                  {
                     if (tmp[count] == "nul")
                        is_nul[count] = true;
                     else
                        is_valid[count] = true;
                  }
               }

               if (is_nul[0] && is_nul[1] && is_nul[2])
                  j.display = "None";
               else if (is_valid[0])
                  j.display = tmp[0];
               else if (is_valid[1])
                  j.display = {tmp[1], " (button)"};
               else if (is_valid[2])
                  j.display = {tmp[2], " (axis)"};
               else
                  j.display = "Default";
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

      static const unsigned max_players = 5;
};

class General : public ToggleWindow
{
   public:
      General(ConfigFile &_pconf, ConfigFile &_conf) : ToggleWindow("SSNES || General settings")
      {
         setGeometry({256, 256, 600, 300});
         widgets.append(BoolSetting::shared(_conf, "rewind_enable", "Enable rewind:", false));
         widgets.append(IntSetting::shared(_conf, "rewind_buffer_size", "Rewind buffer size (MB):", 20));
         widgets.append(IntSetting::shared(_conf, "rewind_granularity", "Rewind frames granularity:", 1));
         widgets.append(BoolSetting::shared(_conf, "pause_nonactive", "Pause when window loses focus:", true));
         widgets.append(IntSetting::shared(_conf, "autosave_interval", "Autosave interval (seconds):", 0));

         savefile_dir = DirSetting::shared(_pconf, "savefile_dir", "Savefile directory:", string(""));
         savestate_dir = DirSetting::shared(_pconf, "savestate_dir", "Savestate directory:", string(""));
         async_fork = BoolSetting::shared(_pconf, "async_fork", "Keep UI visible:", false);
         widgets.append(savefile_dir);
         widgets.append(savestate_dir);
         widgets.append(async_fork);

         foreach(i, widgets) { vbox.append(i->layout(), 0, 0, 3); }

         vbox.setMargin(5);
         append(vbox);
      }

      void update() { foreach(i, widgets) i->update(); }

      bool getSavefileDir(string& dir)
      {
         string str = savefile_dir->edit.text();
         if (str.length() > 0)
         {
            dir = str;
            return true;
         }
         else
            return false;
      }

      bool getSavestateDir(string& dir)
      {
         string str = savestate_dir->edit.text();
         if (str.length() > 0)
         {
            dir = str;
            return true;
         }
         else
            return false;
      }

      bool getAsyncFork()
      {
         return async_fork->check.checked();
      }

   private:
      linear_vector<SettingLayout::APtr> widgets;
      VerticalLayout vbox;
      DirSetting::Ptr savefile_dir, savestate_dir;
      BoolSetting::Ptr async_fork;

};

namespace Internal
{
   static const linear_vector<combo_selection> video_drivers = {
#if defined(_WIN32) || defined(__APPLE__)
      { "gl", "OpenGL" },
#else
      { "gl", "OpenGL" },
      { "xvideo", "XVideo" },
#endif
   };
}

class Video : public ToggleWindow
{
   public:
      Video(ConfigFile &_conf) : ToggleWindow("SSNES || Video settings")
      {
         setGeometry({256, 256, 600, 740});
         widgets.append(ComboSetting::shared(_conf, "video_driver", "Video driver:", Internal::video_drivers, 0));
         widgets.append(DoubleSetting::shared(_conf, "video_xscale", "Windowed X scale:", 3.0));
         widgets.append(DoubleSetting::shared(_conf, "video_xscale", "Windowed Y scale:", 3.0));
         widgets.append(IntSetting::shared(_conf, "video_fullscreen_x", "Fullscreen X resolution:", 0));
         widgets.append(IntSetting::shared(_conf, "video_fullscreen_y", "Fullscreen Y resolution:", 0));
         widgets.append(BoolSetting::shared(_conf, "video_fullscreen", "Start in fullscreen:", false));
         widgets.append(BoolSetting::shared(_conf, "video_smooth", "Bilinear filtering:", true));
         widgets.append(BoolSetting::shared(_conf, "video_force_aspect", "Lock aspect ratio:", true));
         widgets.append(DoubleSetting::shared(_conf, "video_aspect_ratio", "Aspect ratio:", 1.333));


         widgets.append(PathSetting::shared(_conf, "video_font_path", "On-screen message font:", "", "TTF font (*.ttf)"));
         widgets.append(IntSetting::shared(_conf, "video_font_size", "On-screen font size:", 48));
         widgets.append(DoubleSetting::shared(_conf, "video_message_pos_x", "On-screen message pos X:", 0.05));
         widgets.append(DoubleSetting::shared(_conf, "video_message_pos_y", "On-screen message pos Y:", 0.05));
         widgets.append(PathSetting::shared(_conf, "video_filter", "bSNES video filter:", "", "bSNES filter (*.filter)"));

         paths.append(PathSetting::shared(_conf, "video_cg_shader", "Cg pixel shader:", "", "Cg shader (*.cg)"));
         paths.append(PathSetting::shared(_conf, "video_bsnes_shader", "bSNES XML shader:", "", "XML shader (*.shader)"));
         widgets.append(ShaderSetting::shared(_conf, "video_shader_type", 
                  linear_vector<string>({"Automatic", "Cg", "bSNES XML", "None"}), 
                  linear_vector<string>({"auto", "cg", "bsnes", "none"}), paths));
         widgets.append(DirSetting::shared(_conf, "video_shader_dir", "XML shader directory:", ""));
         widgets.append(BoolSetting::shared(_conf, "video_render_to_texture", "Render-to-texture (2-pass rendering):", false));
         widgets.append(DoubleSetting::shared(_conf, "video_fbo_scale_x", "FBO Scale X:", 2.0));
         widgets.append(DoubleSetting::shared(_conf, "video_fbo_scale_y", "FBO Scale Y:", 2.0));
         widgets.append(BoolSetting::shared(_conf, "video_second_pass_smooth", "Bilinear filtering (2. pass):", true));
         widgets.append(PathSetting::shared(_conf, "video_second_pass_shader", "Shader (2. pass):", "", "Cg shader, XML shader (*.cg, *.shader)"));

         foreach(i, widgets) { vbox.append(i->layout(), 0, 0, 3); }
         vbox.setMargin(5);
         append(vbox);
      }

      void update() { foreach(i, widgets) i->update(); }

   private:
      linear_vector<SettingLayout::APtr> widgets;
      VerticalLayout vbox;

      linear_vector<PathSetting::Ptr> paths;

};


namespace Internal
{
   static const linear_vector<combo_selection> audio_drivers = {
#ifdef _WIN32
      {"sdl", "SDL"},
      {"xaudio", "XAudio2"},
      {"rsound", "RSound"},
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

#define DEFINE_BINDS(n_player) \
      { "input_player" #n_player "_a", "A (right)", "" }, \
      { "input_player" #n_player "_b", "B (down)", "" }, \
      { "input_player" #n_player "_y", "Y (left)", "" }, \
      { "input_player" #n_player "_x", "X (up)", "" }, \
      { "input_player" #n_player "_start", "Start", "" }, \
      { "input_player" #n_player "_select", "Select", "" }, \
      { "input_player" #n_player "_l", "L", "" }, \
      { "input_player" #n_player "_r", "R", "" }, \
      { "input_player" #n_player "_left", "D-pad Left", "" }, \
      { "input_player" #n_player "_right", "D-pad Right", "" }, \
      { "input_player" #n_player "_up", "D-pad Up", "" }, \
      { "input_player" #n_player "_down", "D-pad Down", "" },
 

   static const linear_vector<input_selection> player1 = {
      DEFINE_BINDS(1)
   };

   static const linear_vector<input_selection> player2 = {
      DEFINE_BINDS(2)
   };

   static const linear_vector<input_selection> player3 = {
      DEFINE_BINDS(3)
   };

   static const linear_vector<input_selection> player4 = {
      DEFINE_BINDS(4)
   };

   static const linear_vector<input_selection> player5 = {
      DEFINE_BINDS(5)
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
      { "input_reset", "Reset", "" },
      { "input_shader_next", "Next shader", "" },
      { "input_shader_prev", "Previous shader", "" },
   };

   static const linear_vector<linear_vector<input_selection>> binds = { 
      player1, player2, player3, player4, player5, misc 
   };
}

class Audio : public ToggleWindow
{
   public:
      Audio(ConfigFile &_conf) : ToggleWindow("SSNES || Audio settings")
      {
         setGeometry({256, 256, 450, 300});
         widgets.append(BoolSetting::shared(_conf, "audio_enable", "Enable audio:", true));
         widgets.append(IntSetting::shared(_conf, "audio_out_rate", "Audio sample rate:", 48000));
         widgets.append(DoubleSetting::shared(_conf, "audio_in_rate", "Audio input rate:", 31980.0));
         widgets.append(DoubleSetting::shared(_conf, "audio_rate_step", "Audio rate step:", 0.25));
         widgets.append(ComboSetting::shared(_conf, "audio_driver", "Audio driver:", Internal::audio_drivers, 0));
         widgets.append(StringSetting::shared(_conf, "audio_device", "Audio device:", ""));
         widgets.append(BoolSetting::shared(_conf, "audio_sync", "Audio sync:", true));
         widgets.append(IntSetting::shared(_conf, "audio_latency", "Audio latency (ms):", 64));
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
         setGeometry({256, 256, 500, 450});
         widgets.append(DoubleSetting::shared(_conf, "input_axis_threshold", "Input axis threshold (0.0 to 1.0):", 0.5));
         widgets.append(BoolSetting::shared(_conf, "netplay_client_swap_input", "Use Player 1 binds as client:", false));
         widgets.append(InputSetting::shared(_conf, Internal::binds, 
                  [this](const string& msg) { this->setStatusText(msg); }, [this]() { this->setFocused(); }));

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
#elif defined(__APPLE__)
         ruby::input.driver("Carbon");
#else
         ruby::input.driver("SDL");
#endif
         ruby::input.init();
      }
};


#endif
