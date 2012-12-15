#include <phoenix.hpp>
#include <cstdlib>
#include <nall/zip.hpp>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <signal.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "config_file.hpp"
#include <utility>
#include <functional>
#include "settings.hpp"
#include "dynamic.h"
#include "libretro.h"

#ifdef _WIN32
#include "updater.hpp"
#endif

using namespace nall;
using namespace phoenix;

namespace Internal
{
   static lstring split_strings(const char *text, unsigned size)
   {
      lstring list;

      unsigned ptr = 0;
      list.append(text);

      for (;;)
      {
         while (text[ptr] && ptr < size)
            ptr++;

         ptr++;

         if (ptr >= size)
            break;

         list.append(&text[ptr]);
      }

      return list;
   }

#ifndef _WIN32
   static volatile sig_atomic_t child_quit;
   static volatile sig_atomic_t status;
   static volatile sig_atomic_t abnormal_quit;
   static bool async;
   static pid_t child_pid;

   extern "C"
   {
      static void sigchld_handle(int);
   }

   static void sigchld_handle(int)
   {
      if (async)
         while (waitpid(-1, NULL, WNOHANG) > 0) print("Reaped zombie ...\n");
      else
      {
         int pstatus;
         waitpid(child_pid, &pstatus, 0);
         status = WEXITSTATUS(pstatus);
         child_quit = 1;
         abnormal_quit = !WIFEXITED(pstatus);
      }
   }
#endif
}

class LogWindow : public ToggleWindow
{
   public:
      LogWindow() : ToggleWindow("RetroArch || Log window")
      {
         label.setText("RetroArch output:");
         layout.append(label, 0, 0);
         layout.append(box, ~0, ~0);

         select_all.setText("Select all");
         copy_all.setText("Copy all");
         clear_all.setText("Clear all");
         hbox.append(select_all, 0, 0);
         hbox.append(copy_all, 0, 0);
         hbox.append(clear_all, 0, 0);
         layout.append(hbox);

         select_all.onTick = [this] { box.selectAll(); };
         copy_all.onTick = [this] { box.copyAll(); };
         clear_all.onTick = [this] { log = ""; box.setText(log); };

         box.setEditable(false);
         layout.setMargin(5);

         auto minimum = layout.minimumGeometry();
         setGeometry({128, 128, max(300u, minimum.width), max(300u, minimum.height)});
         append(layout);
      }

      void push(const char *text) { log.append(text); box.setText(log); }

      void push(const lstring &list)
      {
         foreach (str, list)
            log.append(str);

         box.setText(log);
      }

      // Push strings which are split by NUL. text[size] needs to be NUL.
      void push(const char *text, unsigned size)
      {
         push(Internal::split_strings(text, size));
      }

      void clear() { log = ""; box.setText(log); }

   private:
      VerticalLayout layout;
      TextEdit box;
      Label label;

      HorizontalLayout hbox;
      Button select_all;
      Button copy_all;
      Button clear_all;

      string log;

};

class Remote : public ToggleWindow
{
   public:
      Remote() : ToggleWindow("RetroArch || Remote"), logger(0)
      {
         save_state.setText("Save state");
         load_state.setText("Load state");
         fast_forward.setText("Fast forward toggle");
         fullscreen_toggle.setText("Fullscreen toggle");
         quit.setText("Quit");
         state_slot_plus.setText("State/movie slot (+)");
         state_slot_minus.setText("State/movie slot (-)");
         movie_record_toggle.setText("Movie record toggle");
         pause_toggle.setText("Pause toggle");
         frameadvance.setText("Frame advance");
         reset.setText("Reset");
         cheat_index_plus.setText("Cheat index (+)");
         cheat_index_minus.setText("Cheat index (-)");
         cheat_toggle.setText("Cheat toggle");
         screenshot.setText("Screenshot");
         dsp_config.setText("DSP config");
         vol_up.setText("Volume (+)");
         vol_down.setText("Volume (-)");
         mute.setText("Mute audio");
         shader.setText("Set shader ...");
         log.setText("Show log");

         save_state.onTick          = [this] { send_cmd("SAVE_STATE\n"); };
         load_state.onTick          = [this] { send_cmd("LOAD_STATE\n"); };
         fast_forward.onTick        = [this] { send_cmd("FAST_FORWARD\n"); };
         fullscreen_toggle.onTick   = [this] { send_cmd("FULLSCREEN_TOGGLE\n"); };
         quit.onTick                = [this] { send_cmd("QUIT\n"); };
         state_slot_plus.onTick     = [this] { send_cmd("STATE_SLOT_PLUS\n"); };
         state_slot_minus.onTick    = [this] { send_cmd("STATE_SLOT_MINUS\n"); };
         movie_record_toggle.onTick = [this] { send_cmd("MOVIE_RECORD_TOGGLE\n"); };
         pause_toggle.onTick        = [this] { send_cmd("PAUSE_TOGGLE\n"); };
         frameadvance.onTick        = [this] { send_cmd("FRAMEADVANCE\n"); };
         reset.onTick               = [this] { send_cmd("RESET\n"); };
         cheat_index_plus.onTick    = [this] { send_cmd("CHEAT_INDEX_PLUS\n"); };
         cheat_index_minus.onTick   = [this] { send_cmd("CHEAT_INDEX_MINUS\n"); };
         cheat_toggle.onTick        = [this] { send_cmd("CHEAT_TOGGLE\n"); };
         screenshot.onTick          = [this] { send_cmd("SCREENSHOT\n"); };
         dsp_config.onTick          = [this] { send_cmd("DSP_CONFIG\n"); };
         vol_up.onTick              = [this] { send_cmd("VOLUME_UP\n"); };
         vol_down .onTick           = [this] { send_cmd("VOLUME_DOWN\n"); };
         mute.onTick                = [this] { send_cmd("MUTE\n"); };

         shader.onTick              = [this] { send_arg_cmd("SET_SHADER"); };
         log.onTick                 = [this] { if (logger) logger->show(); };

         const int remote_button_w = 180;

         vbox[0].append(quit, remote_button_w, 0);

         vbox[1].append(shader, remote_button_w, 0);
         vbox[1].append(log, remote_button_w, 0);

         vbox[2].append(load_state, remote_button_w, 0);
         vbox[2].append(save_state, remote_button_w, 0);
         vbox[2].append(state_slot_plus, remote_button_w, 0);
         vbox[2].append(state_slot_minus, remote_button_w, 0);
         vbox[2].append(cheat_index_plus, remote_button_w, 0);
         vbox[2].append(cheat_index_minus, remote_button_w, 0);
         vbox[2].append(cheat_toggle, remote_button_w, 0);
         vbox[2].append(reset, remote_button_w, 0);

         vbox[3].append(pause_toggle, remote_button_w, 0);
         vbox[3].append(frameadvance, remote_button_w, 0);
         vbox[3].append(fast_forward, remote_button_w, 0);
         vbox[3].append(fullscreen_toggle, remote_button_w, 0);

         vbox[4].append(movie_record_toggle, remote_button_w, 0);
         vbox[4].append(screenshot, remote_button_w, 0);
         vbox[4].append(dsp_config, remote_button_w, 0);
         vbox[4].append(vol_up, remote_button_w, 0);
         vbox[4].append(vol_down, remote_button_w, 0);
         vbox[4].append(mute, remote_button_w, 0);

         foreach(v, vbox)
            layout.append(v);

         auto minimum = layout.minimumGeometry();
         setGeometry({100, 100, minimum.width, minimum.height});
         append(layout);
      }

      // Comment out to test "remote".
      //void show() {}

#ifdef _WIN32
      void set_handle(HANDLE file) { this->handle = file; }
      void send_cmd(const char *cmd) { DWORD written; WriteFile(handle, cmd, strlen(cmd), &written, NULL); }
#else
      void set_fd(int fd) { this->fd = fd; }
      void send_cmd(const char *cmd) { write(fd, cmd, strlen(cmd)); }
#endif

      void send_arg_cmd(const char *cmd)
      {
         string file = OS::fileLoad(Window::None, "", "XML shader, Cg shader, Cg-meta shader (*.shader,*.cg,*.cgp)");
         if (file.length() == 0)
            return;

         string cmd_str = { cmd, " ", file, "\n" };
         send_cmd(cmd_str);
      }

      LogWindow *logger;

   private:
#ifdef _WIN32
      HANDLE handle;
#else
      int fd;
#endif
      VerticalLayout vbox[5];
      HorizontalLayout layout;
      Button save_state, load_state;
      Button fast_forward, fullscreen_toggle, quit, state_slot_plus, state_slot_minus;
      Button movie_record_toggle, pause_toggle, frameadvance, reset;
      Button cheat_index_plus, cheat_index_minus, cheat_toggle, screenshot, dsp_config, vol_up, vol_down;
      Button mute;
      Button shader, log;
};

class MainWindow : public Window
{
   public:
      MainWindow(const nall::string &libretro_path) :
         input(configs.cli), general(configs.gui, configs.cli), video(configs.cli), audio(configs.cli), ext_rom(configs.gui),
         m_cli_path(libretro_path), m_cli_custom_path(libretro_path.length())
      {
         setTitle("RetroArch || Phoenix");
         setIcon("/usr/share/icons/retroarch-phoenix.png");

         init_menu();
         onClose = []() { OS::quit(); };

         init_main_frame();

         vbox.setMargin(5);
         auto minimum = vbox.minimumGeometry();
         setGeometry({256, 256, max(750u, minimum.width), minimum.height});
         append(vbox);
         setMenuVisible();
         setStatusVisible();

         forked_timer.onTimeout = [this]() { this->forked_event(); };
         forked_timer.setInterval(100);

         init_config();
         setVisible();

         remote.logger = &log_win;
      }

      ~MainWindow()
      {
         save_controllers();
         foreach (tempfile, tempfiles)
         {
#ifdef _WIN32
            DeleteFileA(tempfile);
#else
            ::unlink(tempfile);
#endif
         }
      }

   private:
      VerticalLayout vbox;
      Menu file_menu, settings_menu, help_menu;

#ifdef _WIN32
      Menu updater_menu;
#endif

      Button start_btn;

      Input input;
      General general;
      Video video;
      Audio audio;
      Remote remote;
      ExtROM ext_rom;

#ifdef _WIN32
      Updater updater;
#endif

      string m_cli_path;
      bool m_cli_custom_path;

      lstring tempfiles;

      struct netplay
      {
         netplay() : file(0)
         {
            port.setText("55435");
            frames.setText("0");
            enable_label.setText("Netplay:");
            enable.setText("Enable");
            server.setText("Server");
            client.setText("Client");
            spectate.setText("Spectate mode");
            host_label.setText("Host IP:");
            port_label.setText("TCP/UDP Port:");
            frames_label.setText("Delay frames:");
            nick_label.setText("Nickname:");
            server.setChecked();
            RadioBox::group(server, client);

            hlayout[0].append(enable_label, 100, 0);
            hlayout[0].append(enable, 80, 0, 0);
            hlayout[0].append(server, 70, 20);
            hlayout[0].append(client, 70, 20);
            hlayout[0].append(spectate, 150, 20);

            hlayout[1].append(host_label, 80, 0, 20);
            hlayout[1].append(host, 200, 0, 20);
            hlayout[1].append(port_label, 120, 0, 20);
            hlayout[1].append(port, 100, 0);
            hlayout[2].append(frames_label, 80, 0, 20);
            hlayout[2].append(frames, 60, 0, 160);
            hlayout[2].append(nick_label, 120, 0, 20);
            hlayout[2].append(nick, 150, 0);

            nick.onChange = [this] {
               if (file)
                  file->set(file_key, nick.text());
            };
         }

         void setConfig(ConfigFile &config_file, const string &opt)
         {
            file = &config_file;
            file_key = opt;
         }

         HorizontalLayout hlayout[3];
         RadioBox server, client;

         Label port_label, host_label, frames_label, nick_label;
         LineEdit port;
         LineEdit host;
         LineEdit frames;
         LineEdit nick;
         CheckBox enable;
         CheckBox spectate;
         Label enable_label;

         ConfigFile *file;
         string file_key;
      } net;

      struct
      {
         ConfigFile gui, cli;
      } configs;

      LogWindow log_win;

      struct entry
      {
         HorizontalLayout hlayout;
         Label label;
         LineEdit edit;
         Button button;
         Button clear;
         string filter;
         string short_filter;
         bool save_file;

         string default_start_path;

         ConfigFile *conf;
         string config_key;

         function<void (const string&)> cb;

         entry(bool preapply = true) : conf(NULL), cb(&entry::dummy)
         {
            button.setText("Browse ...");
            clear.setText("Clear");
            save_file = false;

            if (preapply)
               apply_layout();

            button.onTick = [this]() {
               string start_path;
               string path;
               path = this->getPath();
               
               if (path.length() > 0)
               {
                  char buf[1024];
                  nall::strlcpy(buf, path, sizeof(buf));
                  char *delim = strrchr(buf, '/');
                  if (!delim)
                     delim = strrchr(buf, '\\');

                  if (delim)
                  {
                     *delim = '\0';
                     start_path = buf;
                  }
                  else
                     start_path = ".";
               }
               else if (default_start_path.length() > 0)
                  start_path = default_start_path;
               else
               {
                  const char *path = std::getenv("HOME");
                  if (path)
                     start_path = path;
               }

               string file;
               if (save_file)
               {
                  file = OS::fileSave(Window::None, start_path, filter);
                  if (file.length() > 0 && !file.endswith(short_filter))
                     file.append(short_filter);
               }
               else
                  file = OS::fileLoad(Window::None, start_path, filter);

               if (file.length() > 0)
               {
                  edit.setText(file);

                  if (short_filter.length() > 0 && !file.endswith(short_filter))
                  {
                     MessageWindow::warning(Window::None,
                           {"Filename extension does not match with the expected extension: \"",
                           short_filter, "\""});
                  }
               }

               if (conf)
                  conf->set(config_key, this->getPath());
               cb(this->getPath());
            };

            clear.onTick = [this]() {
               edit.setText("");
               if (conf)
                  conf->set(config_key, string(""));
               cb("");
            };

            //edit.setEditable(false);
         }

         ~entry()
         {
            saveConfig();
         }

         void saveConfig()
         {
            if (conf)
               conf->set(config_key, getPath());
         }

         void setFilter(const string& _filter, const string& _short_filter = "")
         {
            filter = _filter;
            short_filter = _short_filter;
         }

         void setLabel(const string& name) { label.setText(name); }
         void setPath(const string& name) { edit.setText(name.length() > 0 ? name : string("")); }
         void setConfig(ConfigFile& file, const string& key, const function<void (const string&)>& _cb = &entry::dummy) 
         { 
            conf = &file;
            config_key = key;
            cb = _cb;
         }
         void setStartPath(const string& _start_path) { default_start_path = _start_path; }
         string getPath() { return edit.text(); }
         
         HorizontalLayout& layout() { return hlayout; }

         private:
            static void dummy(const string&) {}

            void apply_layout()
            {
               hlayout.append(label, 150, 0);
               hlayout.append(edit, ~0, 0);
               hlayout.append(clear, 0, 0);
               hlayout.append(button, 0, 0);
            }
      } rom, config, retroarch, libretro, record_config;

      struct bsv_entry : entry
      {
         bsv_entry(bool enable = true) : entry(false)
         {
            if (enable)
            {
               disabled.setText("Disabled");
               enable_playback.setText("Playback");
               enable_record.setText("Record");
               RadioBox::group(disabled, enable_playback, enable_record);

               enable_load.setText("Load SRAM");
               enable_save.setText("Save SRAM");

               hlayout.append(label, 150, 0);
               hlayout.append(edit, ~0, 0);
               hlayout.append(clear, 0, 0);
               hlayout.append(button, 0, 0);

               opts_label.setText("BSV options:");
               hlayout_opt.append(opts_label, 150, 0);
               hlayout_opt.append(disabled, 0, 0, 8);
               hlayout_opt.append(enable_playback, 0, 0, 8);
               hlayout_opt.append(enable_record, 0, 0, 38);
               hlayout_opt.append(enable_load, 0, 0, 8);
               hlayout_opt.append(enable_save, 0, 0);

               save_file = true;
               disabled.onTick        = [this] { this->save_file = true; };
               enable_playback.onTick = [this] { this->save_file = false; };
               enable_record.onTick   = [this] { this->save_file = true; };
               disabled.setChecked();
            }
         }

         bool is_playback() { return enable_playback.checked(); }
         bool is_record() { return enable_record.checked(); }
         bool load_sram() { return enable_load.checked(); }
         bool save_sram() { return enable_save.checked(); }

         HorizontalLayout hlayout_opt;
         HorizontalLayout& layout_opt() { return hlayout_opt; }

         RadioBox disabled;
         RadioBox enable_playback;
         RadioBox enable_record;

         CheckBox enable_load;
         CheckBox enable_save;
         Label opts_label;
      } bsv_movie;

      struct record_entry : entry
      {
         record_entry() : entry(false)
         {
            enable_tick.setText("Enable");
            hlayout.append(label, 150, 0);
            hlayout.append(edit, ~0, 0, 5);

            dim_label.setText("Dimensions:");
            hlayout.append(dim_label, 0, 0, 3);
            hlayout.append(dim_edit, 80, 0, 5);

            hlayout.append(enable_tick, 80, 0);
            hlayout.append(clear, 0, 0);
            hlayout.append(button, 0, 0);
         }

         bool is_enabled() { return enable_tick.checked(); }
         CheckBox enable_tick;

         Label dim_label;
         LineEdit dim_edit;
      } record;

      enum rom_type
      {
         Normal,
         SGB,
         Sufami,
         BSX,
         BSX_Slot
      };

      struct rom_type_box
      {
         rom_type_box()
         {
            box.append("Normal ROM");
            box.append("Super GameBoy");
            box.append("Sufami Turbo");
            box.append("BSX");
            box.append("BSX slotted");

            extract_tick.setText("Extract ZIPs");
            extract_tick.setChecked();
            extract_tick.onTick = [this] { file->set("extract_zip", extract_tick.checked()); };

            allow_patches.setText("Check for ROM patches");
            allow_patches.setChecked();
            allow_patches.onTick = [this] { file->set("allow_patches", allow_patches.checked()); };

            label.setText("ROM type:");
            hlayout.append(label, 150, 0);
            hlayout.append(box, 200, 0, 30);
            hlayout.append(extract_tick, 0, 0);
            hlayout.append(allow_patches, 0, 0);
         }

         void setConfig(ConfigFile &conf)
         {
            file = &conf;
         }

         enum rom_type type()
         {
            static const enum rom_type types[] = {Normal, SGB, Sufami, BSX, BSX_Slot};
            return types[box.selection()];
         }

         bool extract() { return extract_tick.checked(); }
         bool allow_patch() { return allow_patches.checked(); }
         void extract(bool allow) { extract_tick.setChecked(allow); }
         void allow_patch(bool allow) { allow_patches.setChecked(allow); }

         HorizontalLayout& layout() { return hlayout; }

         private:
            ComboBox box;
            Label label;
            HorizontalLayout hlayout;
            CheckBox extract_tick;
            CheckBox allow_patches;
            ConfigFile *file;
      } rom_type;

#ifdef _WIN32

      // Windows is completely batshit retarded and the relative path might "change" by going into the file manager, so we have to manually figure out the full path. :)
      static string basedir()
      {
         char dir_path[MAX_PATH];
         GetModuleFileNameA(GetModuleHandle(0), dir_path, sizeof(dir_path));
         char *split = strrchr(dir_path, '\\');
         if (!split) split = strrchr(dir_path, '/');
         if (split) split[1] = '\0';
         return dir_path;
      }

      static string gui_config_path()
      {
         string dir = basedir();

         // If we have retroarch-phoenix.cfg in same directory, use that ...
         WIN32_FIND_DATAA data;
         string gui_path = {dir, "\\retroarch-phoenix.cfg"};
         HANDLE find_file = FindFirstFileA(gui_path, &data);
         if (find_file != INVALID_HANDLE_VALUE)
         {
            FindClose(find_file);
            return gui_path;
         }

         const char *path = std::getenv("APPDATA");
         if (path)
            return {path, "\\phoenix.cfg"};
         else
            return gui_path;
      }

      string cli_config_path()
      {
         string tmp;
         if (configs.gui.get("config_path", tmp))
            return tmp;
         else
         {
            string dir = basedir();

            // If we have retroarch.cfg in same directory, use that ...
            WIN32_FIND_DATAA data;
            string cli_path = {dir, "\\retroarch.cfg"};
            HANDLE find_file = FindFirstFileA(cli_path, &data);
            if (find_file != INVALID_HANDLE_VALUE)
            {
               FindClose(find_file);
               return cli_path;
            }

            const char *path = std::getenv("APPDATA");
            if (path)
               return {path, "\\retroarch.cfg"};
            else
            {
               char dir[256];
               GetCurrentDirectoryA(sizeof(dir), dir);
               return {dir, "\\retroarch.cfg"};
            }
         }
      }
#elif defined(__APPLE__)
      static string gui_config_path()
      {
         string gui_path;
         const char *home = std::getenv("HOME");
         if (home)
         {
            gui_path = home;
            gui_path.append("/.retroarch_phoenix.cfg");
         }
         else
            gui_path = "/etc/retroarch_phoenix.cfg";

         return gui_path;
      }

      string cli_config_path()
      {
         const char *path = std::getenv("HOME");
         string cli_path;
         string tmp;
         if (configs.gui.get("config_path", tmp))
            cli_path = tmp;
         else
         {
            if (path)
            {
               cli_path = path;
               cli_path.append("/.retroarch.cfg");
            }
            else
               cli_path = "/etc/retroarch.cfg";
         }
         return cli_path;
      }
#else
      static string gui_config_path()
      {
         string gui_path;
         const char *path = std::getenv("XDG_CONFIG_HOME");
         const char *home_path = std::getenv("HOME");
         if (path)
         {
            string dir = {path, "/retroarch"};
            mkdir(dir, 0755);

            gui_path = path;
            gui_path.append("/retroarch/phoenix.cfg");
         }
         else if (home_path)
         {
            gui_path = home_path;
            gui_path.append("/.retroarch_phoenix.cfg");
         }
         else
            gui_path = "/etc/retroarch_phoenix.cfg";

         return gui_path;
      }

      string cli_config_path()
      {
         const char *path = std::getenv("XDG_CONFIG_HOME");
         const char *home_path = std::getenv("HOME");
         string cli_path;
         string tmp;
         if (configs.gui.get("config_path", tmp))
         {
            cli_path = tmp;
         }
         else
         {
            if (path)
            {
               string dir = {path, "/retroarch"};
               mkdir(dir, 0644);
               cli_path = path;
               cli_path.append("/retroarch/retroarch.cfg");
            }
            else if (home_path)
            {
               cli_path = home_path;
               cli_path.append("/.retroarch.cfg");
            }
            else
               cli_path = "/etc/retroarch.cfg";
         }

         return cli_path;
      }
#endif

      void init_controllers()
      {
         string tmp;
         if (configs.gui.get("controller_1", tmp))
         {
            if (tmp == "gamepad")
               settings.gamepad_1.setChecked();
            else if (tmp == "dualanalog")
               settings.dualanalog_1.setChecked();
            else if (tmp == "mouse")
               settings.mouse_1.setChecked();
            else if (tmp == "none")
               settings.none_1.setChecked();
         }

         if (configs.gui.get("controller_2", tmp))
         {
            if (tmp == "gamepad")
               settings.gamepad_2.setChecked();
            else if (tmp == "dualanalog")
               settings.dualanalog_2.setChecked();
            else if (tmp == "mouse")
               settings.mouse_2.setChecked();
            else if (tmp == "scope")
               settings.scope_2.setChecked();
            else if (tmp == "justifier")
               settings.justifier_2.setChecked();
            else if (tmp == "justifiers")
               settings.justifiers_2.setChecked();
            else if (tmp == "none")
               settings.none_2.setChecked();
         }
      }

      void save_controllers()
      {
         if (settings.gamepad_1.checked())
            configs.gui.set("controller_1", string("gamepad"));
         else if (settings.mouse_1.checked())
            configs.gui.set("controller_1", string("mouse"));
         else if (settings.none_1.checked())
            configs.gui.set("controller_1", string("none"));
         else if (settings.dualanalog_1.checked())
            configs.gui.set("controller_1", string("dualanalog"));

         if (settings.gamepad_2.checked())
            configs.gui.set("controller_2", string("gamepad"));
         else if (settings.mouse_2.checked())
            configs.gui.set("controller_2", string("mouse"));
         else if (settings.none_2.checked())
            configs.gui.set("controller_2", string("none"));
         else if (settings.dualanalog_2.checked())
            configs.gui.set("controller_2", string("dualanalog"));
         else if (settings.scope_2.checked())
            configs.gui.set("controller_2", string("scope"));
         else if (settings.justifier_2.checked())
            configs.gui.set("controller_2", string("justifier"));
         else if (settings.justifiers_2.checked())
            configs.gui.set("controller_2", string("justifiers"));
      }

      void save_config()
      {
         nall::string current_conf = config.getPath();
         char buf[1024];
         nall::strlcpy(buf, current_conf, sizeof(buf));
         char *delim = strrchr(buf, '/');
         if (!delim)
            delim = strrchr(buf, '\\');

         nall::string start_path;
         if (delim)
         {
            *delim = '\0';
            start_path = buf;
         }
         else
            start_path = ".";

         string file = OS::fileSave(Window::None, start_path, "Config File (*.cfg)");
         if (file.length() > 0 && !file.endswith(".cfg"))
            file.append(".cfg");

         if (file.length() > 0)
         {
            configs.cli.write(file);
            configs.cli.replace_path(file);
            config.setPath(file);
         }
      }

      void init_config()
      {
         string tmp;
         string gui_path = gui_config_path();

         configs.gui = ConfigFile(gui_path);

         if (configs.gui.get("retroarch_path", tmp)) retroarch.setPath(tmp);
         retroarch.setConfig(configs.gui, "retroarch_path");
         if (configs.gui.get("last_movie", tmp)) bsv_movie.setPath(tmp);
         bsv_movie.setConfig(configs.gui, "last_movie");
         if (configs.gui.get("record_path", tmp)) record.setPath(tmp);
         record.setConfig(configs.gui, "record_path");
         if (configs.gui.get("record_config_path", tmp)) record_config.setPath(tmp);
         record_config.setConfig(configs.gui, "record_config_path");

         if (configs.gui.get("nickname", tmp)) net.nick.setText(tmp);
         net.setConfig(configs.gui, "nickname");

         if (configs.gui.get("config_path", tmp)) config.setPath(tmp);
         config.setConfig(configs.gui, "config_path", {&MainWindow::reload_cli_config, this});
         libretro.setConfig(configs.cli, "libretro_path");

         init_controllers();

         bool zip;
         if (configs.gui.get("extract_zip", zip))
            rom_type.extract(zip);
         bool patches;
         if (configs.gui.get("allow_patches", patches))
            rom_type.allow_patch(patches);
         rom_type.setConfig(configs.gui);


         if (!m_cli_custom_path)
            m_cli_path = cli_config_path();

         print("Loading CLI path: ", m_cli_path, "\n");
         configs.cli = ConfigFile(m_cli_path);
         config.setPath(m_cli_path);
         rom.setConfig(configs.cli, "phoenix_last_rom");

         init_cli_config();
      }

      void reload_cli_config(const string& path)
      {
         print("Reloading config: ", path, "\n");
         if (path.length() > 0)
         {
            configs.cli = ConfigFile(path);
            m_cli_path = path;
         }
         else
         {
            m_cli_path = cli_config_path();
            configs.cli = ConfigFile(m_cli_path);
         }

         init_cli_config();
      }

      void init_cli_config()
      {
         string tmp;
         if (configs.cli.get("libretro_path", tmp))
            libretro.setPath(tmp);
         else
            libretro.setPath("");

         if (configs.cli.get("phoenix_last_rom", tmp))
            rom.setPath(tmp);
         else
            rom.setPath("");

         if (configs.cli.get("phoenix_default_rom_dir", tmp))
            rom.setStartPath(tmp);
         else
            rom.setStartPath("");

         if (configs.cli.get("phoenix_default_bsv_movie_dir", tmp))
            bsv_movie.setStartPath(tmp);
         else
            bsv_movie.setStartPath("");

         if (configs.cli.get("phoenix_default_record_dir", tmp))
            record.setStartPath(tmp);
         else
            record.setStartPath("");

         if (configs.cli.get("phoenix_default_record_config_dir", tmp))
            record_config.setStartPath(tmp);
         else
            record_config.setStartPath("");

         general.update();
         video.update();
         audio.update();
         input.update();
         ext_rom.update();
      }

      void init_main_frame()
      {
         rom.setFilter("Game ROM (*)");
         bsv_movie.setFilter("BSNES Movie (*.bsv)", ".bsv");
         config.setFilter("Config file (*.cfg)");
         libretro.setFilter("Dynamic library (" DYNAMIC_EXTENSION ")");
#ifdef _WIN32
         retroarch.setFilter("Executable file (*.exe)");
#else
         retroarch.setFilter("Any file (*)");
#endif

         rom.setLabel("Normal ROM path:");
         bsv_movie.setLabel("BSV movie:");
         record.setLabel("FFmpeg movie output:");
         record_config.setLabel("FFmpeg config:");
         config.setLabel("RetroArch config file:");
         retroarch.setLabel("RetroArch path:");
         libretro.setLabel("libretro core path:");

         start_btn.setText("Start RetroArch");
         vbox.append(rom.layout(), 3);
         vbox.append(rom_type.layout(), 5);
         vbox.append(bsv_movie.layout(), 3);
         vbox.append(bsv_movie.layout_opt(), 8);
         record.save_file = true;
         record.setFilter("Matroska (*.mkv)", ".mkv");
         vbox.append(record.layout(), 3);
         record_config.setFilter("Config file (*.cfg)");
         vbox.append(record_config.layout(), 10);
         vbox.append(config.layout(), 3);
         vbox.append(retroarch.layout(), 3);
         vbox.append(libretro.layout(), 3);
         vbox.append(start_btn, ~0, 0, 15);
         vbox.append(net.hlayout[0]);
         vbox.append(net.hlayout[1]);
         vbox.append(net.hlayout[2], 20);

         start_btn.onTick = [this]() { this->start_retroarch(); };
      }

      void show_error(const string& err)
      {
         MessageWindow::warning(Window::None, err);
      }

      lstring get_system_info(struct retro_system_info &sys_info, const string &path)
      {
         lstring ret;
         string roms;

         dylib_t lib = dylib_load(path);
         if (!lib)
            return ret;

         unsigned (*pver)() = NULL;
         void (*pgetinfo)(struct retro_system_info*) = NULL;

         pver = (unsigned (*)())dylib_proc(lib, "retro_api_version");
         if (!pver)
            goto end;

         if (pver() != RETRO_API_VERSION)
            goto end;

         pgetinfo = (void (*)(struct retro_system_info*))dylib_proc(lib, "retro_get_system_info");
         if (!pgetinfo)
            goto end;

         pgetinfo(&sys_info);

         if (sys_info.valid_extensions)
            roms = sys_info.valid_extensions;

end:
         dylib_close(lib);

         ret.split("|", roms);
         return ret;
      }

      bool check_zip(string& rom_path)
      {
         struct retro_system_info sys_info = {0};
         lstring exts = get_system_info(sys_info, libretro.getPath());
         if (sys_info.block_extract)
            return true;

         string orig_rom_path = rom_path;
         char *ext = strrchr(rom_path(), '.');

         if (!ext)
            return true;
         if (strcasecmp(ext, ".zip") != 0)
            return true;
         if (!rom_type.extract())
            return true;

         *ext = '\0';

         ext = strrchr(rom_path(), '/');
         if (!ext)
            ext = strrchr(rom_path(), '\\');

         *ext = '\0';
         string rom_dir = {rom_path, "/"};
         string rom_basename = ext + 1;
         string rom_extension;

         nall::zip z;
         if (!z.open(orig_rom_path))
         {
            MessageWindow::critical(*this, "Failed opening ZIP!");
            return false;
         }

         foreach (known_ext, exts)
            print("Known extension: ", known_ext, "\n");

         foreach (file, z.file)
         {
            print("Checking file: ", file.name, "\n");
            foreach (known_ext, exts)
            {
               if (!file.name.endswith(known_ext))
                  continue;

               rom_extension = known_ext;

               uint8_t *data;
               unsigned size;
               if (!z.extract(file, data, size))
               {
                  MessageWindow::critical(*this,
                        "Failed to load ROM from archive!");
                  return false;
               }

               if (!data || !size)
               {
                  MessageWindow::critical(*this,
                        "Received invalid result from nall::zip. This should not happen, but it did anyways ... ;)");
                  return false;
               }

               bool has_extracted;

               rom_path = {rom_dir, rom_basename, ".", rom_extension};

               bool already_extracted = tempfiles.find(rom_path);

               if (nall::file::exists(rom_path) && !already_extracted)
               {
                  auto response = MessageWindow::information(*this,
                        {"Attempting to extract ROM to ",
                        rom_path, ", but it already exists. Do you want to overwrite it?"},
                        MessageWindow::Buttons::YesNo);

                  if (response == MessageWindow::Response::No)
                  {
                     MessageWindow::information(*this,
                           "ROM loading aborted!");
                     delete [] data;
                     return false;
                  }
               }

               has_extracted = nall::file::write(rom_path, data, size);

               delete [] data;
               if (has_extracted)
                  goto extracted;
               else
               {
                  MessageWindow::critical(*this,
                        {"Failed extracting ROM to: ", rom_path, "!"});
                  return false;
               }
            }
         }
         MessageWindow::critical(*this, "Failed to find valid rom in archive!");
         return false;

extracted:
         tempfiles.append(rom_path);
         return true;
      }

      bool append_rom(string& rom_path, linear_vector<const char*>& vec_cmd)
      {
         // Need static since we're referencing a const char*.
         // If destructor hits we're screwed.
         static string rom_path_slot1;
         static string rom_path_slot2;

         bool sufami_a = false, sufami_b = false;
         bool slotted = false;

         switch (rom_type.type())
         {
            case Normal:
               rom_path = rom.getPath();
               if (rom_path.length() == 0)
               {
                  show_error("No ROM selected :(");
                  return false;
               }
               if (!check_zip(rom_path))
                  return false;
               vec_cmd.append(rom_path);
               return true;

            case SGB:
               if (!ext_rom.get_sgb_bios(rom_path))
               {
                  show_error("No Super GameBoy BIOS selected :(");
                  return false;
               }
               if (!ext_rom.get_sgb_rom(rom_path_slot1))
               {
                  show_error("No Super GameBoy BIOS selected :(");
                  return false;
               }
               vec_cmd.append(rom_path);
               vec_cmd.append("--gameboy");
               vec_cmd.append(rom_path_slot1);
               return true;

            case Sufami:
               if (!ext_rom.get_sufami_bios(rom_path))
               {
                  show_error("No Sufami Turbo BIOS selected :(");
                  return false;
               }
               if (ext_rom.get_sufami_slot_a(rom_path_slot1))
                  sufami_a = true;
               if (ext_rom.get_sufami_slot_b(rom_path_slot2))
                  sufami_b = true;

               vec_cmd.append(rom_path);

               if (!sufami_a && !sufami_b)
               {
                  show_error("At least pick one Sufami slot :(");
                  return false;
               }

               if (sufami_a) { vec_cmd.append("--sufamiA"); vec_cmd.append(rom_path_slot1); }
               if (sufami_b) { vec_cmd.append("--sufamiB"); vec_cmd.append(rom_path_slot2); }
               return true;

            case BSX_Slot:
               slotted = true;
            case BSX:
               if (!ext_rom.get_bsx_bios(rom_path))
               {
                  show_error("No BSX BIOS selected :(");
                  return false;
               }
               if (!ext_rom.get_bsx_rom(rom_path_slot1))
               {
                  show_error("No BSX ROM selected :(");
                  return false;
               }

               vec_cmd.append(rom_path);
               vec_cmd.append(slotted ? "--bsxslot" : "--bsx");
               vec_cmd.append(rom_path_slot1);
               return true;
         }

         return false;
      }

      void start_retroarch()
      {
#ifdef _WIN32
         updater.cancel();
#endif

         if (libretro.getPath() == "")
         {
            MessageWindow::warning(*this,
#ifdef _WIN32
                  "No libretro core is selected.\n"
                  "You can download a core from the RetroArch updater\n(RetroArch -> Update RetroArch)."
#else
                  "No libretro core is selected. Cannot continue.\n"
#endif
            );

            return;
         }

         // Need to force this for remote to work.
         configs.cli.set("stdin_cmd_enable", true);

#if 0
         string sysdir;
         if (!configs.cli.get("system_directory", sysdir) || sysdir.length() == 0)
         {
            MessageWindow::warning(*this,
                  "System directory (Settings -> General -> System directory) is not set.\n"
                  "Some libretro cores that rely on this might not work as intended.");
         }
#endif

         linear_vector<const char*> vec_cmd;
         string retroarch_path = retroarch.getPath();
         if (retroarch_path.length() == 0) retroarch_path = "retroarch";
         string rom_path;
         string config_path = config.getPath();
         string host;
         string port;
         string frames;
         string movie_path;
         string record_path;
         string record_config_path;
         string record_size;
         string nickname;

         vec_cmd.append(retroarch_path);

         if (!append_rom(rom_path, vec_cmd))
            return;

         vec_cmd.append("-c");
         if (config_path.length() > 0)
            vec_cmd.append(config_path);
         else
            vec_cmd.append(m_cli_path);

         vec_cmd.append("-v");

         if (net.enable.checked())
         {
            if (net.server.checked())
               vec_cmd.append("-H");
            else
            {
               vec_cmd.append("-C");
               host = net.host.text();
               vec_cmd.append(host);
            }

            if (net.spectate.checked())
               vec_cmd.append("--spectate");

            vec_cmd.append("--port");
            port = net.port.text();
            vec_cmd.append(port);

            vec_cmd.append("-F");
            frames = net.frames.text();
            vec_cmd.append(frames);

            nickname = net.nick.text();
            if (nickname.length() > 0)
            {
               vec_cmd.append("--nick");
               vec_cmd.append(nickname);
            }
         }

         if (bsv_movie.is_playback())
         {
            vec_cmd.append("-P");
            movie_path = bsv_movie.getPath();
            vec_cmd.append(movie_path);
         }
         else if (bsv_movie.is_record())
         {
            vec_cmd.append("-R");
            movie_path = bsv_movie.getPath();
            vec_cmd.append(movie_path);
         }

         if (bsv_movie.is_playback() || bsv_movie.is_record())
         {
            unsigned val = ((unsigned)bsv_movie.load_sram() << 1) | bsv_movie.save_sram();
            static const char *lut[] = {
               "noload-nosave",
               "noload-save",
               "load-nosave",
               "load-save",
            };

            vec_cmd.append("-M");
            vec_cmd.append(lut[val]);
         }

         if (record.is_enabled())
         {
            vec_cmd.append("--record");
            record_path = record.getPath();
            vec_cmd.append(record_path);
         }

         record_config_path = record_config.getPath();
         if (record_config_path.length() > 0)
         {
            vec_cmd.append("--recordconfig");
            vec_cmd.append(record_config_path);
         }

         if (record.dim_edit.text().length() > 0)
         {
            if (!record.is_enabled())
            {
               show_error("Recording size was set,\nbut FFmpeg recording is not enabled.");
               return;
            }

            auto str = record.dim_edit.text();
            auto pos = str.iposition("x");

            if (pos)
            {
               bool invalid = false;
               str[pos()] = '\0';
               invalid |= strtoul((const char*)str, 0, 0) == 0;
               invalid |= strtoul((const char*)str + pos() + 1, 0, 0) == 0;

               if (invalid)
               {
                  show_error("Invalid size parameter for recording. (Format: WIDTHxHEIGHT)");
                  return;
               }

               vec_cmd.append("--size");
               record_size = record.dim_edit.text();
               vec_cmd.append(record_size);
            }
            else
            {
               show_error("Invalid size parameter for recording. (Format: WIDTHxHEIGHT)");
               return;
            }
         }

         if (settings.mouse_1.checked())
         {
            vec_cmd.append("-m");
            vec_cmd.append("1");
         }
         else if (settings.none_1.checked())
         {
            vec_cmd.append("-N");
            vec_cmd.append("1");
         }
         else if (settings.dualanalog_1.checked())
         {
            vec_cmd.append("-A");
            vec_cmd.append("1");
         }

         if (settings.multitap_2.checked())
            vec_cmd.append("--multitap");
         else if (settings.mouse_2.checked())
         {
            vec_cmd.append("-m");
            vec_cmd.append("2");
         }
         else if (settings.scope_2.checked())
            vec_cmd.append("-p");
         else if (settings.justifier_2.checked())
            vec_cmd.append("-j");
         else if (settings.justifiers_2.checked())
            vec_cmd.append("-J");
         else if (settings.none_2.checked())
         {
            vec_cmd.append("-N");
            vec_cmd.append("2");
         }
         else if (settings.dualanalog_2.checked())
         {
            vec_cmd.append("-A");
            vec_cmd.append("2");
         }

         if (!rom_type.allow_patch())
            vec_cmd.append("--no-patch");

         string savefile_dir, savestate_dir;
         char basename_buf[1024];

         nall::strlcpy(basename_buf, rom_path, sizeof(basename_buf));
         char *basename_ptr;
         
         if ((basename_ptr = strrchr(basename_buf, '.')))
         {
            *basename_ptr = '\0';
            nall::strlcat(basename_buf, ".srm", sizeof(basename_buf));
         }

         if (!(basename_ptr = strrchr(basename_buf, '/')))
            basename_ptr = strrchr(basename_buf, '\\');
         if (basename_ptr)
            basename_ptr++;

         if (general.getSavefileDir(savefile_dir))
         {
            if (basename_ptr)
               savefile_dir.append(basename_ptr);
            vec_cmd.append("-s");
            vec_cmd.append(savefile_dir);
         }

         nall::strlcpy(basename_buf, rom_path, sizeof(basename_buf));
         if ((basename_ptr = strrchr(basename_buf, '.')))
         {
            *basename_ptr = '\0';
            nall::strlcat(basename_buf, ".state", sizeof(basename_buf));
         }

         if (!(basename_ptr = strrchr(basename_buf, '/')))
            basename_ptr = strrchr(basename_buf, '\\');
         if (basename_ptr)
            basename_ptr++;

         if (general.getSavestateDir(savestate_dir))
         {
            if (basename_ptr)
               savestate_dir.append(basename_ptr);
            vec_cmd.append("-S");
            vec_cmd.append(savestate_dir);
         }

         vec_cmd.append(NULL);
         configs.gui.write();
         configs.cli.write();

         log_win.clear();
         string commandline("RetroArch CMD: ");
         commandline.append(retroarch_path);
         for (unsigned i = 1; i < vec_cmd.size() - 1; i++)
            commandline.append(" ", vec_cmd[i]);
         commandline.append("\n\n");
         log_win.push(commandline);

         fork_retroarch(retroarch_path, &vec_cmd[0]);
      }

      static void print_cmd(const string& str, const char **cmd)
      {
         print("CMD: ", str, "\n");
         print("Args: ");
         unsigned cnt = 0;
         const char *head = cmd[cnt];
         while (head)
         {
            print(head, " ");
            head = cmd[++cnt];
         }
         print("\n");
      }

      Timer forked_timer;

#ifdef _WIN32
      HANDLE fork_file;
      HANDLE fork_stdin_file;
      PROCESS_INFORMATION forked_pinfo;
      bool forked_async;

      void forked_event()
      {
         bool should_restore = false;
         DWORD exit_status = 255;

         if (WaitForSingleObject(forked_pinfo.hProcess, 0) == WAIT_OBJECT_0)
         {
            GetExitCodeProcess(forked_pinfo.hProcess, &exit_status);
            CloseHandle(forked_pinfo.hThread);
            CloseHandle(forked_pinfo.hProcess);
            memset(&forked_pinfo, 0, sizeof(forked_pinfo));
            should_restore = true;
         }

         if (!forked_async)
         {
            char buffer[1024];
            DWORD has_read;
            DWORD can_read;
            while (PeekNamedPipe(fork_file, NULL, 0, NULL, &can_read, NULL) && can_read > 0)
            {
               if (can_read >= sizeof(buffer))
                  can_read = sizeof(buffer) - 1;

               if (ReadFile(fork_file, buffer, can_read, &has_read, NULL) && has_read > 0)
               {
                  buffer[has_read] = '\0';
                  log_win.push(buffer);
               }
            }
         }

         if (!should_restore)
            return;

         if (!forked_async)
         {
            setVisible();
            if (exit_status == 255)
               setStatusText("Something unexpected happened ...");
            else if (exit_status == 2)
               setStatusText("RetroArch failed with assertion. Check log!");
            else if (exit_status == 0)
               setStatusText("RetroArch returned successfully!");
            else
               setStatusText({"RetroArch returned with an error! Code: ", (unsigned)exit_status});
         }

         forked_timer.setEnabled(false);

         if (forked_pinfo.hThread)
            CloseHandle(forked_pinfo.hThread);
         if (forked_pinfo.hProcess)
            CloseHandle(forked_pinfo.hProcess);
         memset(&forked_pinfo, 0, sizeof(forked_pinfo));

         if (fork_file)
         {
            // Drain out the remaining data in the FIFO.
            char final_buf[1024];
            DWORD has_read;
            DWORD can_read;

            while (PeekNamedPipe(fork_file, NULL, 0, NULL, &can_read, NULL) && can_read > 0)
            {
               if (can_read >= sizeof(final_buf))
                  can_read = sizeof(final_buf) - 1;

               if (ReadFile(fork_file, final_buf, can_read, &has_read, NULL) && has_read > 0)
               {
                  final_buf[has_read] = '\0';
                  log_win.push(final_buf);
               }
            }

            CloseHandle(fork_file);
            CloseHandle(fork_stdin_file);
            fork_file       = NULL;
            fork_stdin_file = NULL;

            remote.hide();
         }
      }
#else
      int fork_fd;
      int fork_stdin_fd;
      void forked_event()
      {
         if (Internal::child_quit)
         {
            forked_timer.setEnabled(false);

            // Flush out log messages ...
            char line[2048];
            ssize_t ret;
            while ((ret = read(fork_fd, line, sizeof(line) - 1)) > 0)
            {
               line[ret] = '\0';
               log_win.push(line, ret);
            }

            close(fork_fd);
            close(fork_stdin_fd);
            fork_fd       = -1;
            fork_stdin_fd = -1;

            if (Internal::abnormal_quit)
               setStatusText("RetroArch exited abnormally! Check log!");
            else if (Internal::status == 255)
               setStatusText("Could not find RetroArch!");
            else if (Internal::status == 2)
               setStatusText("RetroArch failed with assertion. Check log!");
            else if (Internal::status != 0)
               setStatusText("Failed to open ROM!");
            else
               setStatusText("RetroArch exited successfully.");

            setVisible();
            remote.hide();
         }

         char line[2048];
         ssize_t ret;

         struct pollfd poll_fd = {0};
         poll_fd.fd = fork_fd;
         poll_fd.events = POLLIN;

         if (poll(&poll_fd, 1, 0) <= 0)
            return;

         if (poll_fd.revents & POLLHUP)
            return;

         if (poll_fd.revents & POLLIN)
         {
            ret = read(fork_fd, line, sizeof(line) - 1);
            if (ret < 0)
               return;

            line[ret] = '\0';
            log_win.push(line, ret);
         }
      }
#endif

#ifndef _WIN32
      void fork_retroarch(const string& path, const char **cmd)
      {
         Internal::async = general.getAsyncFork();
         bool can_hide = !Internal::async;

         // Gotta love Unix :)
         int fds[2];
         int stdin_fds[2];

         Internal::status = 0;
         Internal::child_quit = 0;
         Internal::abnormal_quit = 0;

         struct sigaction sa;
         sa.sa_handler = Internal::sigchld_handle;
         sa.sa_flags   = SA_RESTART;
         sigemptyset(&sa.sa_mask);
         sigaction(SIGCHLD, &sa, NULL);

         if (can_hide)
         {
            if (pipe(fds) < 0)
               return;
            if (pipe(stdin_fds) < 0)
               return;

            fork_fd       = fds[0];
            fork_stdin_fd = stdin_fds[1];

            for (unsigned i = 0; i < 2; i++)
            {
               fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK);
               fcntl(stdin_fds[i], F_SETFL, fcntl(stdin_fds[i], F_GETFL) | O_NONBLOCK);
            }

            setVisible(false);
            general.hide();
            video.hide();
            audio.hide();
            input.hide();

            remote.show();
            remote.set_fd(fork_stdin_fd);
         }

         if ((Internal::child_pid = fork()))
         {
            if (can_hide)
            {
               close(fds[1]);
               close(stdin_fds[0]);
               forked_timer.setEnabled();
            }
         }
         else
         {
            if (can_hide)
            {
               // Redirect GUI to stdin.
               close(0);
               if (dup(stdin_fds[0]) < 0)
                  exit(255);

               // Redirect stderr to GUI reader.
               close(2);
               if (dup(fds[1]) < 0)
                  exit(255);
            }

            print_cmd(path, cmd);

            if (execvp(path, const_cast<char**>(cmd)) < 0)
               exit(255);
         }
      }
#else
      void fork_retroarch(const string& path, const char **cmd)
      {
         print_cmd(path, cmd);

         HANDLE reader, writer, reader_stdin, writer_stdin;
         SECURITY_ATTRIBUTES saAttr;
         ZeroMemory(&saAttr, sizeof(saAttr));
         saAttr.nLength = sizeof(saAttr);
         saAttr.bInheritHandle = TRUE;
         saAttr.lpSecurityDescriptor = NULL;
         CreatePipe(&reader, &writer, &saAttr, 0);
         CreatePipe(&reader_stdin, &writer_stdin, &saAttr, 0);
         SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);

         string cmdline = {"\"", path, "\" "};
         cmd++;
         while (*cmd) 
         { 
            cmdline.append("\""); 
            cmdline.append(*cmd++); 
            cmdline.append("\" "); 
         }

         PROCESS_INFORMATION piProcInfo;
         STARTUPINFO siStartInfo;
         BOOL bSuccess = FALSE;
         ZeroMemory(&piProcInfo, sizeof(piProcInfo));

         forked_async = general.getAsyncFork();

         ZeroMemory(&siStartInfo, sizeof(siStartInfo));
         siStartInfo.cb = sizeof(siStartInfo);
         siStartInfo.hStdError = writer;
         siStartInfo.hStdOutput = NULL;
         siStartInfo.hStdInput = reader_stdin;
         siStartInfo.dwFlags |= forked_async ? 0 : STARTF_USESTDHANDLES;

         WCHAR wcmdline[1024];
         if (MultiByteToWideChar(CP_UTF8, 0, cmdline, -1, wcmdline, 1024) == 0)
         {
            if (reader)
               CloseHandle(reader);
            if (reader_stdin)
               CloseHandle(reader_stdin);
            if (writer)
               CloseHandle(writer);
            if (writer_stdin)
               CloseHandle(writer_stdin);
            return;
         }

         bSuccess = CreateProcessW(NULL,
               wcmdline,
               NULL,
               NULL,
               TRUE,
               CREATE_NO_WINDOW,
               NULL,
               NULL,
               &siStartInfo,
               &piProcInfo);

         bool can_hide = !forked_async;

         if (can_hide)
         {
            if (bSuccess)
            {
               setVisible(false);
               general.hide();
               video.hide();
               audio.hide();
               input.hide();
               remote.show();
               remote.set_handle(writer_stdin);

               CloseHandle(writer);
               CloseHandle(reader_stdin);
               writer       = NULL;
               reader_stdin = NULL;

               fork_file       = reader;
               fork_stdin_file = writer_stdin;
               forked_pinfo    = piProcInfo;

               forked_timer.setEnabled();
            }
            else
               setStatusText("Failed to start RetroArch");
         }
         else
         {
            fork_file = NULL;
            fork_stdin_file = NULL;
         }

         if (writer)
            CloseHandle(writer);
         if (reader_stdin)
            CloseHandle(reader_stdin);
      }
#endif

      struct
      {
         Item ext_rom;
         CheckItem log;
         Separator sep;
         Item quit;
      } file;

      struct
      {
         Item save_config;
         Item general;
         Separator sep1, sep2, sep3;
         Item video;
         Item audio;
         Item input;

         Menu controllers;
         Menu port_1;
         Menu port_2;

         RadioItem gamepad_1;
         RadioItem mouse_1;
         RadioItem none_1;
         RadioItem dualanalog_1;

         RadioItem gamepad_2;
         RadioItem multitap_2;
         RadioItem mouse_2;
         RadioItem scope_2;
         RadioItem justifier_2;
         RadioItem justifiers_2;
         RadioItem none_2;
         RadioItem dualanalog_2;
      } settings;

#ifdef _WIN32
      struct
      {
         Item update;
      } updater_elems;
#endif

      struct
      {
         Item about;
      } help;

      void init_menu()
      {
         file_menu.setText("File");
         settings_menu.setText("Settings");
#ifdef _WIN32
         updater_menu.setText("RetroArch");
#endif
         help_menu.setText("Help");
         append(file_menu);
         append(settings_menu);
#ifdef _WIN32
         append(updater_menu);
#endif
         append(help_menu);

         file.ext_rom.setText("Special ROM");
         file.log.setText("Show log");
         file.quit.setText("Quit");

         settings.save_config.setText("Save RetroArch config");
         settings.general.setText("General");
         settings.video.setText("Video");
         settings.audio.setText("Audio");
         settings.input.setText("Input");

         help.about.setText("About");

         file_menu.append(file.ext_rom);
         file_menu.append(file.log);
         file_menu.append(file.sep);
         file_menu.append(file.quit);
         settings_menu.append(settings.save_config);
         settings_menu.append(settings.sep1);
         settings_menu.append(settings.general);
         settings_menu.append(settings.sep2);
         settings_menu.append(settings.video);
         settings_menu.append(settings.audio);
         settings_menu.append(settings.input);
         settings_menu.append(settings.sep3);

         settings.controllers.setText("Controllers");
         settings.port_1.setText("Port 1");
         settings.port_2.setText("Port 2");

         settings.gamepad_1.setText("Gamepad");
         settings.mouse_1.setText("Mouse");
         settings.none_1.setText("None");
         settings.dualanalog_1.setText("DualAnalog");

         settings.gamepad_2.setText("Gamepad");
         settings.multitap_2.setText("Multitap (SNES)");
         settings.mouse_2.setText("Mouse");
         settings.scope_2.setText("SuperScope (SNES)");
         settings.justifier_2.setText("Justifier (SNES)");
         settings.justifiers_2.setText("Two Justifiers (SNES)");
         settings.none_2.setText("None");
         settings.dualanalog_2.setText("DualAnalog");

         settings_menu.append(settings.controllers);
         settings.controllers.append(settings.port_1);
         settings.controllers.append(settings.port_2);
         settings.port_1.append(settings.gamepad_1);
         settings.port_1.append(settings.dualanalog_1);
         settings.port_1.append(settings.mouse_1);
         settings.port_1.append(settings.none_1);

         settings.port_2.append(settings.gamepad_2);
         settings.port_2.append(settings.dualanalog_2);
         settings.port_2.append(settings.multitap_2);
         settings.port_2.append(settings.mouse_2);
         settings.port_2.append(settings.scope_2);
         settings.port_2.append(settings.justifier_2);
         settings.port_2.append(settings.justifiers_2);
         settings.port_2.append(settings.none_2);

         RadioItem::group(settings.gamepad_1, settings.dualanalog_1, settings.mouse_1, settings.none_1);
         RadioItem::group(settings.gamepad_2, settings.dualanalog_2, settings.multitap_2, settings.mouse_2, settings.scope_2, settings.justifier_2, settings.justifiers_2, settings.none_2);

#ifdef _WIN32
         updater_elems.update.setText("Update RetroArch");
         updater_menu.append(updater_elems.update);
#endif

         help_menu.append(help.about);
         init_menu_callbacks();
      }

      void init_menu_callbacks()
      {
         file.log.onTick = [this]() { if (file.log.checked()) log_win.show(); else log_win.hide(); };
         log_win.setCloseCallback([this]() { file.log.setChecked(false); });
         file.quit.onTick = []() { OS::quit(); };
         help.about.onTick = [this]() { MessageWindow::information(*this, "RetroArch/Phoenix\nHans-Kristian Arntzen (Themaister) (C) - 2011-2012\nThis is free software released under GNU GPLv3\nPhoenix (C) byuu - 2011"); };

         settings.save_config.onTick = {&MainWindow::save_config, this};
         settings.general.onTick = [this]() { general.show(); };
         settings.video.onTick = [this]() { video.show(); };
         settings.audio.onTick = [this]() { audio.show(); };
         settings.input.onTick = [this]() { input.show(); };
         file.ext_rom.onTick = [this]() { ext_rom.show(); };

#ifdef _WIN32
         updater_elems.update.onTick = [this]() { updater.show(); };
         updater.libretro_path_cb = [this](const nall::string &path_)
         {
            auto path = path_;
            path.transform("\\", "/");
            configs.cli.set("libretro_path", path);
            libretro.setPath(path);
         };

         // Might use config later.
         updater.retroarch_path_cb = [this]() -> nall::string { return "retroarch"; };
#endif
      }
};

int main(int argc, char *argv[])
{
#ifndef _WIN32
   struct sigaction sa;
   sa.sa_handler = SIG_IGN;
   sa.sa_flags = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGPIPE, &sa, NULL);
#endif

   if (argc > 2)
   {
      print("Usage: retroarch-phoenix [RetroArch config file]\n");
      return 1;
   }
   else if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
   {
      print("Usage: retroarch-phoenix [RetroArch config file]\n");
      return 0;
   }

   nall::string libretro_path;
   if (argc == 2)
      libretro_path = argv[1];

   MainWindow win(libretro_path);
   OS::main();
}
