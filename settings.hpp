#ifndef __SSNES_SETTINGS_HPP
#define __SSNES_SETTINGS_HPP

#include "config_file.hpp"
#include <phoenix.hpp>
#include <utility>
#include "util.hpp"

#ifdef _WIN32
#define WIDGET_HEIGHT 24
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
      onClose = [this]() { this->hide(); }; 
      setGeometry({256, 256, 400, 240});
   }
   void show() { setVisible(); }
   void hide() { setVisible(false); }
};

class SettingLayout : public util::SharedAbstract<SettingLayout>
{
   public:
      SettingLayout(ConfigFile &_conf, const string& _key, const string& _label) : conf(_conf), key(_key) 
      {
         label.setText(_label);
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
         hlayout.append(button, 60, WIDGET_HEIGHT);

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
         check.onTick = [this]() { conf.set(key, check.checked()); };
         hlayout.append(check, 20, WIDGET_HEIGHT);
      }

      void update()
      {
         bool tmp = m_default;
         conf.get(key, tmp);
         check.setChecked(tmp);
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
         edit.onChange = [this]() { conf.set(key, (int)decimal(edit.text())); };
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
         edit.onChange = [this]() { conf.set(key, (double)fp(edit.text())); };
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
#ifndef _WIN32
      {"alsa", "ALSA"},
      {"pulse", "PulseAudio"},
      {"oss", "Open Sound System"},
      {"jack", "JACK Audio"},
      {"rsound", "RSound"},
      {"roar", "RoarAudio"},
      {"openal", "OpenAL"},
      {"sdl", "SDL"},
#else
      {"sdl", "SDL"},
      {"xaudio", "XAudio2"}
#endif
   };
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
         setGeometry({256, 256, 300, 200});
         widgets.append(DoubleSetting::shared(_conf, "input_axis_threshold", "Input axis threshold (0.0 to 1.0)", 0.5));
         widgets.append(BoolSetting::shared(_conf, "netplay_client_swap_input", "Use Player 1 binds as client", false));

         foreach(i, widgets) { vbox.append(i->layout(), 0, 0, 3); }
         vbox.setMargin(5);
         append(vbox);
      }

      void update() { foreach(i, widgets) i->update(); }

   private:
      linear_vector<SettingLayout::APtr> widgets;
      VerticalLayout vbox;
};


#endif
