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
#else

   // Windows is stupid, and doesn't even have something as basic as pollable non-blocking files.
   // So here we go with threading ...
   static lstring log_strings;
   static CRITICAL_SECTION crit;
   static volatile bool thread_alive;

   static DWORD CALLBACK read_thread(void *data)
   {
      HANDLE read_handle = (HANDLE)data;
      log_strings.reset();
      DWORD read_bytes;

      while (thread_alive)
      {
         char tmp_buf[2048];

         if (ReadFile(read_handle, tmp_buf, sizeof(tmp_buf) - 1, &read_bytes, NULL) && read_bytes > 0)
         {
            tmp_buf[read_bytes] = '\0';
            auto list = split_strings(tmp_buf, read_bytes);

            EnterCriticalSection(&crit);
            foreach (str, list)
               log_strings.append(str);
            LeaveCriticalSection(&crit);
         }
         else
            break;
      }

      ExitThread(0);
   }
#endif
}

class LogWindow : public ToggleWindow
{
   public:
      LogWindow() : ToggleWindow("SSNES || Log window")
      {
         label.setText("SSNES output:");
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
         setGeometry({128, 128, max(300, minimum.width), max(300, minimum.height)});
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

class MainWindow : public Window
{
   public:
      MainWindow() : input(configs.cli), general(configs.gui, configs.cli), video(configs.cli), audio(configs.cli), ext_rom(configs.gui)
      {
         setTitle("SSNES || Phoenix");

         init_menu();
         onClose = []() { OS::quit(); };

         init_main_frame();

         vbox.setMargin(5);
         auto minimum = vbox.minimumGeometry();
         setGeometry({256, 256, max(750, minimum.width), minimum.height});
         append(vbox);
         setMenuVisible();
         setStatusVisible();

         forked_timer.onTimeout = [this]() { this->forked_event(); };
         forked_timer.setInterval(100);

         init_config();
         setVisible();
      }

      ~MainWindow()
      {
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
      ExtROM ext_rom;

#ifdef _WIN32
      Updater updater;
#endif

      string m_cli_path;

      lstring tempfiles;

      struct netplay
      {
         netplay() 
         {
            port.setText("55435");
            frames.setText("0");
            enable_label.setText("Netplay:");
            enable.setText("Enable");
            server.setText("Server");
            client.setText("Client");
            host_label.setText("Host IP:");
            port_label.setText("TCP/UDP Port:");
            frames_label.setText("Delay frames:");
            server.setChecked();
            RadioBox::group(server, client);

            hlayout[0].append(enable_label, 100, 0);
            hlayout[0].append(enable, 80, 0, 0);
            hlayout[0].append(server, 70, 20);
            hlayout[0].append(client, 70, 20);

            hlayout[1].append(host_label, 80, 0, 20);
            hlayout[1].append(host, 200, 0, 20);
            hlayout[1].append(port_label, 120, 0, 20);
            hlayout[1].append(port, 100, 0);
            hlayout[2].append(frames_label, 80, 0, 20);
            hlayout[2].append(frames, 60, 0);
         }

         HorizontalLayout hlayout[3];
         RadioBox server, client;

         Label port_label, host_label, frames_label;
         LineEdit port;
         LineEdit host;
         LineEdit frames;
         CheckBox enable;
         Label enable_label;
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

         ConfigFile *conf;
         string config_key;

         function<void (const string&)> cb;

         entry(bool preapply = true) : conf(NULL), cb(&entry::dummy)
         {
            button.setText("Open ...");
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
               }
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
                  if (file.length() > 0 && !strchr(file, '.'))
                     file.append(short_filter);
               }
               else
                  file = OS::fileLoad(Window::None, start_path, filter);

               if (file.length() > 0)
                  edit.setText(file);

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

            edit.setEditable(false);
         }

         void setFilter(const string& _filter, const string& _short_filter = "")
         {
            filter = _filter;
            short_filter = _short_filter;
         }

         void setLabel(const string& name) { label.setText(name); }
         void setPath(const string& name) { edit.setText(name.length() > 0 ? name : " "); } // Really weird bug... :v Empty strings will simply be bool and evaluate to true for some reason...
         void setConfig(ConfigFile& file, const string& key, const function<void (const string&)>& _cb = &entry::dummy) 
         { 
            conf = &file;
            config_key = key;
            cb = _cb;
         }
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
      } rom, config, ssnes, libsnes;

      struct enable_entry : entry
      {
         enable_entry(bool enable = true) : entry(false)
         {
            if (enable)
            {
               enable_tick.setText("Enable");
               hlayout.append(label, 150, 0);
               hlayout.append(edit, ~0, 0, 5);
               hlayout.append(enable_tick, 80, 0);
               hlayout.append(clear, 0, 0);
               hlayout.append(button, 0, 0);
            }
         }

         bool is_enabled() { return enable_tick.checked(); }

         CheckBox enable_tick;
      } movie_play;

      struct record_entry : enable_entry
      {
         record_entry() : enable_entry(false)
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

            label.setText("ROM type:");
            hlayout.append(label, 150, 0);
            hlayout.append(box, 200, 0, 30);
            hlayout.append(extract_tick, 0, 0);
         }

         enum rom_type type()
         {
            static const enum rom_type types[] = {Normal, SGB, Sufami, BSX, BSX_Slot};
            return types[box.selection()];
         }

         bool extract() { return extract_tick.checked(); }

         HorizontalLayout& layout() { return hlayout; }

         private:
            ComboBox box;
            Label label;
            HorizontalLayout hlayout;
            CheckBox extract_tick;
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

         // If we have ssnes-phoenix.cfg in same directory, use that ...
         WIN32_FIND_DATAA data;
         string gui_path = {dir, "\\ssnes-phoenix.cfg"};
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

            // If we have ssnes.cfg in same directory, use that ...
            WIN32_FIND_DATAA data;
            string cli_path = {dir, "\\ssnes.cfg"};
            HANDLE find_file = FindFirstFileA(cli_path, &data);
            if (find_file != INVALID_HANDLE_VALUE)
            {
               FindClose(find_file);
               return cli_path;
            }

            const char *path = std::getenv("APPDATA");
            if (path)
               return {path, "\\ssnes.cfg"};
            else
            {
               char dir[256];
               GetCurrentDirectoryA(sizeof(dir), dir);
               return {dir, "\\ssnes.cfg"};
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
            gui_path.append("/.ssnes_phoenix.cfg");
         }
         else
            gui_path = "/etc/ssnes_phoenix.cfg";

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
               cli_path.append("/.ssnes.cfg");
            }
            else
               cli_path = "/etc/ssnes.cfg";
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
            string dir = {path, "/ssnes"};
            mkdir(dir, 0755);

            gui_path = path;
            gui_path.append("/ssnes/phoenix.cfg");
         }
         else if (home_path)
         {
            gui_path = home_path;
            gui_path.append("/.ssnes_phoenix.cfg");
         }
         else
            gui_path = "/etc/ssnes_phoenix.cfg";

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
               string dir = {path, "/ssnes"};
               mkdir(dir, 0644);
               cli_path = path;
               cli_path.append("/ssnes/ssnes.cfg");
            }
            else if (home_path)
            {
               cli_path = home_path;
               cli_path.append("/.ssnes.cfg");
            }
            else
               cli_path = "/etc/ssnes.cfg";
         }

         return cli_path;
      }
#endif

      void init_config()
      {
         string tmp;
         string gui_path = gui_config_path();

         configs.gui = ConfigFile(gui_path);

         if (configs.gui.get("ssnes_path", tmp)) ssnes.setPath(tmp);
         ssnes.setConfig(configs.gui, "ssnes_path");
         if (configs.gui.get("last_rom", tmp)) rom.setPath(tmp);
         rom.setConfig(configs.gui, "last_rom");
         if (configs.gui.get("last_movie", tmp)) movie_play.setPath(tmp);
         movie_play.setConfig(configs.gui, "last_movie");
         if (configs.gui.get("record_path", tmp)) record.setPath(tmp);
         record.setConfig(configs.gui, "record_path");

         if (configs.gui.get("config_path", tmp)) config.setPath(tmp);
         config.setConfig(configs.gui, "config_path", {&MainWindow::reload_cli_config, this});
         libsnes.setConfig(configs.cli, "libsnes_path");

         m_cli_path = cli_config_path();
         configs.cli = ConfigFile(m_cli_path);

         init_cli_config();
      }

      void reload_cli_config(const string& path)
      {
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
         if (configs.cli.get("libsnes_path", tmp)) libsnes.setPath(tmp); else libsnes.setPath("");

         general.update();
         video.update();
         audio.update();
         input.update();
         ext_rom.update();
      }

      void init_main_frame()
      {
         rom.setFilter("Game ROM (*)");
         movie_play.setFilter("BSNES Movie (*.bsv)");
         config.setFilter("Config file (*.cfg)");
         libsnes.setFilter("Dynamic library (" DYNAMIC_EXTENSION ")");
#ifdef _WIN32
         ssnes.setFilter("Executable file (*.exe)");
#else
         ssnes.setFilter("Any file (*)");
#endif

         rom.setLabel("Normal ROM path:");
         movie_play.setLabel("BSV movie:");
         record.setLabel("FFmpeg movie output:");
         config.setLabel("SSNES config file:");
         ssnes.setLabel("SSNES path:");
         libsnes.setLabel("libsnes path:");

         start_btn.setText("Start SSNES");
         vbox.append(rom.layout(), 3);
         vbox.append(rom_type.layout(), 5);
         vbox.append(movie_play.layout(), 5);
         record.save_file = true;
         record.setFilter("Matroska (*.mkv)", ".mkv");
         vbox.append(record.layout(), 10);
         vbox.append(config.layout(), 3);
         vbox.append(ssnes.layout(), 3);
         vbox.append(libsnes.layout(), 3);
         vbox.append(start_btn, ~0, 0, 15);
         vbox.append(net.hlayout[0]);
         vbox.append(net.hlayout[1]);
         vbox.append(net.hlayout[2], 20);

         start_btn.onTick = [this]() { this->start_ssnes(); };
      }

      void show_error(const string& err)
      {
         MessageWindow::warning(Window::None, err);
      }

      bool check_zip(string& rom_path)
      {
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

         static const char *known_exts[] = {
            ".smc", ".sfc",
            ".nes",
            ".gba", ".gb", ".gbc",
            ".bin", ".smd", ".gen",
         };

         foreach (file, z.file)
         {
            print("Checking file: ", file.name, "\n");
            foreach (known_ext, known_exts)
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

               rom_path = {rom_dir, rom_basename, rom_extension};

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

      void start_ssnes()
      {
#ifdef _WIN32
         updater.cancel();
#endif

         linear_vector<const char*> vec_cmd;
         string ssnes_path = ssnes.getPath();
         if (ssnes_path.length() == 0) ssnes_path = "ssnes";
         string rom_path;
         string config_path = config.getPath();
         string host;
         string port;
         string frames;
         string movie_path;
         string record_path;
         string record_size;

         vec_cmd.append(ssnes_path);

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

            vec_cmd.append("--port");
            port = net.port.text();
            vec_cmd.append(port);

            vec_cmd.append("-F");
            frames = net.frames.text();
            vec_cmd.append(frames);
         }

         if (movie_play.is_enabled())
         {
            vec_cmd.append("-P");
            movie_path = movie_play.getPath();
            vec_cmd.append(movie_path);
         }

         if (record.is_enabled())
         {
            vec_cmd.append("--record");
            record_path = record.getPath();
            vec_cmd.append(record_path);
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
         string commandline("SSNES CMD: ");
         commandline.append(ssnes_path);
         for (unsigned i = 1; i < vec_cmd.size() - 1; i++)
            commandline.append(" ", vec_cmd[i]);
         commandline.append("\n\n");
         log_win.push(commandline);

         fork_ssnes(ssnes_path, &vec_cmd[0]);
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
      PROCESS_INFORMATION forked_pinfo;
      bool forked_async;

      HANDLE forked_read_thread;

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
            EnterCriticalSection(&Internal::crit);

            log_win.push(Internal::log_strings);
            Internal::log_strings.reset();
            LeaveCriticalSection(&Internal::crit);
         }

         if (!should_restore)
            return;

set_visible:
         if (!forked_async)
         {
            setVisible();
            if (exit_status == 255)
               setStatusText("Something unexpected happened ...");
            else if (exit_status == 0)
               setStatusText("SSNES returned successfully!");
            else
               setStatusText({"SSNES returned with an error! Code: ", (unsigned)exit_status});
         }

         forked_timer.setEnabled(false);

         if (forked_pinfo.hThread)
            CloseHandle(forked_pinfo.hThread);
         if (forked_pinfo.hProcess)
            CloseHandle(forked_pinfo.hProcess);
         memset(&forked_pinfo, 0, sizeof(forked_pinfo));

         if (!forked_async)
         {
            Internal::thread_alive = false;
            WaitForSingleObject(forked_read_thread, INFINITE);
            CloseHandle(forked_read_thread);
            DeleteCriticalSection(&Internal::crit);
         }

         if (fork_file)
         {
            // Drain out the remaining data in the FIFO.
            char final_buf[512];
            DWORD read_bytes;
            while (ReadFile(fork_file, final_buf, sizeof(final_buf) - 1, &read_bytes, NULL) && read_bytes > 0)
            {
               final_buf[read_bytes] = '\0';
               log_win.push(final_buf, read_bytes);
            }

            CloseHandle(fork_file);
            fork_file = NULL;
         }
      }
#else
      int fork_fd;
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

            if (Internal::abnormal_quit)
               setStatusText("SSNES exited abnormally!");
            else if (Internal::status == 255)
               setStatusText("Could not find SSNES!");
            else if (Internal::status != 0)
               setStatusText("Failed to open ROM!");
            else
               setStatusText("SSNES exited successfully.");

            setVisible();
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
      void fork_ssnes(const string& path, const char **cmd)
      {
         Internal::async = general.getAsyncFork();
         bool can_hide = !Internal::async;

         // Gotta love Unix :)
         int fds[2];

         Internal::status = 0;
         Internal::child_quit = 0;
         Internal::abnormal_quit = 0;
         signal(SIGCHLD, Internal::sigchld_handle);

         if (can_hide)
         {
            if (pipe(fds) < 0)
               return;

            fork_fd = fds[0];
            for (unsigned i = 0; i < 2; i++)
               fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK);

            setVisible(false);
            general.hide();
            video.hide();
            audio.hide();
            input.hide();
         }

         if ((Internal::child_pid = fork()))
         {
            if (can_hide)
            {
               close(fds[1]);
               forked_timer.setEnabled();
            }
         }
         else
         {
            if (can_hide)
            {
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
      void fork_ssnes(const string& path, const char **cmd)
      {
         print_cmd(path, cmd);

         HANDLE reader, writer;
         SECURITY_ATTRIBUTES saAttr;
         ZeroMemory(&saAttr, sizeof(saAttr));
         saAttr.nLength = sizeof(saAttr);
         saAttr.bInheritHandle = TRUE;
         saAttr.lpSecurityDescriptor = NULL;
         CreatePipe(&reader, &writer, &saAttr, 0);
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
         siStartInfo.hStdInput = NULL;
         siStartInfo.dwFlags |= forked_async ? 0 : STARTF_USESTDHANDLES;

         WCHAR wcmdline[1024];
         if (MultiByteToWideChar(CP_UTF8, 0, cmdline, -1, wcmdline, 1024) == 0)
         {
            if (reader)
               CloseHandle(reader);
            if (writer)
               CloseHandle(writer);
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

               CloseHandle(writer);
               writer = NULL;

               fork_file = reader;
               forked_pinfo = piProcInfo;

               InitializeCriticalSection(&Internal::crit);
               Internal::thread_alive = true;
               forked_read_thread = CreateThread(NULL, 0, Internal::read_thread, reader, 0, NULL);
               forked_timer.setEnabled();
            }
            else
               setStatusText("Failed to start SSNES");
         }
         else
            fork_file = NULL;

         if (writer)
            CloseHandle(writer);
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
         Item general;
         Separator sep;
         Item video;
         Item audio;
         Item input;

         Separator sep2;

         Menu controllers;
         Menu port_1;
         Menu port_2;

         RadioItem gamepad_1;
         RadioItem mouse_1;
         RadioItem none_1;

         RadioItem gamepad_2;
         RadioItem multitap_2;
         RadioItem mouse_2;
         RadioItem scope_2;
         RadioItem justifier_2;
         RadioItem justifiers_2;
         RadioItem none_2;
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
         updater_menu.setText("SSNES");
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

         settings.general.setText("General");
         settings.video.setText("Video");
         settings.audio.setText("Audio");
         settings.input.setText("Input");

         help.about.setText("About");

         file_menu.append(file.ext_rom);
         file_menu.append(file.log);
         file_menu.append(file.sep);
         file_menu.append(file.quit);
         settings_menu.append(settings.general);
         settings_menu.append(settings.sep);
         settings_menu.append(settings.video);
         settings_menu.append(settings.audio);
         settings_menu.append(settings.input);
         settings_menu.append(settings.sep2);

         settings.controllers.setText("Controllers");
         settings.port_1.setText("Port 1");
         settings.port_2.setText("Port 2");

         settings.gamepad_1.setText("Gamepad");
         settings.mouse_1.setText("Mouse");
         settings.none_1.setText("None");

         settings.gamepad_2.setText("Gamepad");
         settings.multitap_2.setText("Multitap");
         settings.mouse_2.setText("Mouse");
         settings.scope_2.setText("SuperScope");
         settings.justifier_2.setText("Justifier");
         settings.justifiers_2.setText("Two Justifiers");
         settings.none_2.setText("None");

         settings_menu.append(settings.controllers);
         settings.controllers.append(settings.port_1);
         settings.controllers.append(settings.port_2);
         settings.port_1.append(settings.gamepad_1);
         settings.port_1.append(settings.mouse_1);
         settings.port_1.append(settings.none_1);

         settings.port_2.append(settings.gamepad_2);
         settings.port_2.append(settings.multitap_2);
         settings.port_2.append(settings.mouse_2);
         settings.port_2.append(settings.scope_2);
         settings.port_2.append(settings.justifier_2);
         settings.port_2.append(settings.justifiers_2);
         settings.port_2.append(settings.none_2);

         RadioItem::group(settings.gamepad_1, settings.mouse_1, settings.none_1);
         RadioItem::group(settings.gamepad_2, settings.multitap_2, settings.mouse_2, settings.scope_2, settings.justifier_2, settings.justifiers_2, settings.none_2);

#ifdef _WIN32
         updater_elems.update.setText("Update SSNES");
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
         help.about.onTick = [this]() { MessageWindow::information(*this, "SSNES/Phoenix\nHans-Kristian Arntzen (Themaister) (C) - 2011\nThis is free software released under GNU GPLv3\nPhoenix (C) byuu - 2011"); };

         settings.general.onTick = [this]() { general.show(); };
         settings.video.onTick = [this]() { video.show(); };
         settings.audio.onTick = [this]() { audio.show(); };
         settings.input.onTick = [this]() { input.show(); };
         file.ext_rom.onTick = [this]() { ext_rom.show(); };

#ifdef _WIN32
         updater_elems.update.onTick = [this]() { updater.show(); };
         updater.libsnes_path_cb = [this](const nall::string &path_)
         {
            auto path = path_;
            path.transform("\\", "/");
            configs.cli.set("libsnes_path", path);
            libsnes.setPath(path);
         };

         // Might use config later.
         updater.ssnes_path_cb = [this]() -> nall::string { return "ssnes"; };
#endif
      }
};

int main()
{
   MainWindow win;
   OS::main();
}
