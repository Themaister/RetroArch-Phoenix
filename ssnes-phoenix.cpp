#include <phoenix.hpp>
#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "config_file.hpp"
#include <utility>
#include <functional>
#include "settings.hpp"

using namespace nall;
using namespace phoenix;

#ifndef _WIN32
namespace Internal
{
   extern "C"
   {
      static void sigchld_handle(int);
   }

   static void sigchld_handle(int)
   {
      //print("SSNES child died!\n");
      while (waitpid(-1, NULL, WNOHANG) > 0);
   }
}
#endif


class LogWindow : public ToggleWindow
{
   public:
      LogWindow() : ToggleWindow("SSNES || Log window")
      {
         label.setText("SSNES output:");
         layout.append(label, 0, WIDGET_HEIGHT);
         layout.append(box, 0, 0);
         box.setEditable(false);
         layout.setMargin(5);
         append(layout);
      }

      void push(const char *text) { log.append(text); box.setText(log); if(OS::pendingEvents()) OS::processEvents(); }
      void clear() { log = ""; box.setText(log); }

   private:
      VerticalLayout layout;
      TextEdit box;
      Label label;
      string log;
};

class MainWindow : public Window
{
   public:
      MainWindow() : general(configs.gui, configs.cli), video(configs.cli), audio(configs.cli), input(configs.cli), ext_rom(configs.gui)
      {
         setTitle("SSNES || Phoenix");
         //setBackgroundColor(64, 64, 64);
         setGeometry({128, 128, 680, 350});

         init_menu();
         onClose = []() { OS::quit(); };

         init_main_frame();

         vbox.setMargin(5);
         append(vbox);
         setMenuVisible();
         setStatusVisible();

         init_config();
         setVisible();
      }

   private:
      VerticalLayout vbox;
      Menu file_menu, settings_menu, help_menu;

      Button start_btn;

      General general;
      Video video;
      Audio audio;
      Input input;
      ExtROM ext_rom;

      string m_cli_path;

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

            hlayout[0].append(enable_label, 100, WIDGET_HEIGHT);
            hlayout[0].append(enable, 80, WIDGET_HEIGHT, 0);
            hlayout[0].append(server, 70, 20);
            hlayout[0].append(client, 70, 20);

            hlayout[1].append(host_label, 80, WIDGET_HEIGHT, 20);
            hlayout[1].append(host, 200, WIDGET_HEIGHT, 20);
            hlayout[1].append(port_label, 120, WIDGET_HEIGHT, 20);
            hlayout[1].append(port, 100, WIDGET_HEIGHT);
            hlayout[2].append(frames_label, 80, WIDGET_HEIGHT, 20);
            hlayout[2].append(frames, 60, WIDGET_HEIGHT);
         }

         HorizontalLayout hlayout[3];
         RadioBox server, client;

         Label port_label, host_label, frames_label;
         TextEdit port;
         TextEdit host;
         TextEdit frames;
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
         TextEdit edit;
         Button button;
         Button clear;
         string filter;

         ConfigFile *conf;
         string config_key;

         function<void (const string&)> cb;

         entry(bool preapply = true) : conf(NULL), cb(&entry::dummy)
         {
            button.setText("Open ...");
            clear.setText("Clear");

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

               string file = OS::fileLoad(Window::None, start_path, filter);
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


         void setFilter(const string& _filter) { filter = _filter; }
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
               hlayout.append(label, 150, WIDGET_HEIGHT);
               hlayout.append(edit, 0, WIDGET_HEIGHT);
               hlayout.append(clear, 80, WIDGET_HEIGHT);
               hlayout.append(button, 100, WIDGET_HEIGHT);
            }
      } rom, config, ssnes, libsnes;

      struct enable_entry : entry
      {
         enable_entry() : entry(false)
         {
            enable_tick.setText("Enable");
            hlayout.append(label, 150, WIDGET_HEIGHT);
            hlayout.append(edit, 0, WIDGET_HEIGHT, 5);
            hlayout.append(enable_tick, 80, WIDGET_HEIGHT);
            hlayout.append(clear, 80, WIDGET_HEIGHT);
            hlayout.append(button, 100, WIDGET_HEIGHT);
         }

         bool is_enabled() { return enable_tick.checked(); }

         CheckBox enable_tick;
      } movie_play;

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

            label.setText("ROM type:");
            hlayout.append(label, 150, WIDGET_HEIGHT);
            hlayout.append(box, 200, WIDGET_HEIGHT);
         }

         enum rom_type type()
         {
            static const enum rom_type types[] = {Normal, SGB, Sufami, BSX, BSX_Slot};
            return types[box.selection()];
         }

         HorizontalLayout& layout() { return hlayout; }

         private:
            ComboBox box;
            Label label;
            HorizontalLayout hlayout;
      } rom_type;


#ifdef _WIN32
      static string gui_config_path()
      {
         // Insane hack. Goddamn, win32 file handling sucks ... :(
         // After we have changed directoy in the file browser, this changes ...
         // On startup this will be correct ...
         static char dir[256];
         if (!dir[0])
            GetCurrentDirectoryA(sizeof(dir), dir);

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
            static char dir[256];
            if (!dir[0])
               GetCurrentDirectoryA(sizeof(dir), dir);

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
            mkdir(dir, 0644);

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
               cli_path = path;
               cli_path.append("/ssnes/ssnes.cfg");
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
         rom.setFilter("Super Magicom, Super Famicom (*.smc,*.sfc)");
         movie_play.setFilter("BSNES Movie (*.bsv)");
         config.setFilter("Config file (*.cfg)");
         libsnes.setFilter("Dynamic library (" DYNAMIC_EXTENSION ")");
#ifdef _WIN32
         ssnes.setFilter("Executable file (*.exe)");
#endif

         rom.setLabel("Normal ROM path:");
         movie_play.setLabel("BSV movie:");
         config.setLabel("SSNES config file:");
         ssnes.setLabel("SSNES path:");
         libsnes.setLabel("libsnes path:");

         start_btn.setText("Start SSNES");
         vbox.append(rom.layout(), 0, 0, 3);
         vbox.append(rom_type.layout(), 0, 0, 5);
         vbox.append(movie_play.layout(), 0, 0, 10);
         vbox.append(config.layout(), 0, 0, 3);
         vbox.append(ssnes.layout(), 0, 0, 3);
         vbox.append(libsnes.layout(), 0, 0, 3);
         vbox.append(start_btn, 0, WIDGET_HEIGHT, 15);
         vbox.append(net.hlayout[0], 0, 0);
         vbox.append(net.hlayout[1], 0, 0, 0);
         vbox.append(net.hlayout[2], 0, 0, 20);

         start_btn.onTick = [this]() { this->start_ssnes(); };
      }

      void show_error(const string& err)
      {
         MessageWindow::warning(Window::None, err);
      }

      bool append_rom(string& rom_path, linear_vector<const char*>& vec_cmd)
      {
         // Need static since we're referencing a const char*. If destructor hits we're screwed.
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
               rom_path = rom.getPath();
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
         linear_vector<const char*> vec_cmd;
         string ssnes_path = ssnes.getPath();
         if (ssnes_path.length() == 0) ssnes_path = "ssnes";
         string rom_path;
         string config_path = config.getPath();
         string host;
         string port;
         string frames;
         string movie_path;

         vec_cmd.append("ssnes");

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
            //print(string({"Port:", port}));
            vec_cmd.append(port);

            vec_cmd.append("-F");
            frames = net.frames.text();
            //print(string({"Frames:", frames}));
            vec_cmd.append(frames);
         }

         if (movie_play.is_enabled())
         {
            vec_cmd.append("-P");
            movie_path = movie_play.getPath();
            vec_cmd.append(movie_path);
         }

         if (settings.mouse_1.checked())
         {
            vec_cmd.append("-m");
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


         //foreach(i, vec_cmd) if (i) print(i, "\n");
         //print("\n");

         vec_cmd.append(NULL);
         configs.gui.write();
         configs.cli.write();
         fork_ssnes(ssnes_path, &vec_cmd[0]);
      }

#ifndef _WIN32
      void fork_ssnes(const string& path, const char **cmd)
      {
         // I think we had to do this with GTK+ at least :v
         if (OS::pendingEvents()) OS::processEvents();

         bool can_hide = !general.getAsyncFork();

         // Gotta love Unix :)
         int fds[2];

         if (can_hide)
         {
            setVisible(false);
            general.hide();
            video.hide();
            audio.hide();
            input.hide();

            pipe(fds);
            signal(SIGCHLD, SIG_DFL);
         }
         else
            signal(SIGCHLD, Internal::sigchld_handle);

         if (fork())
         {
            if (can_hide)
            {
               close(fds[1]);
               log_win.clear();
               char line[1024] = {0};
               ssize_t ret;
               while((ret = read(fds[0], line, sizeof(line) - 1)) > 0)
               {
                  line[ret] = '\0';
                  log_win.push(line);
               }

               int status = 0;
               wait(&status);
               status = WEXITSTATUS(status);
               if (status == 255)
                  setStatusText({"Could not find SSNES in given path ", path});
               else if (status != 0)
                  setStatusText({"Failed to open ROM!"});
               else
                  setStatusText("SSNES exited successfully.");
               close(fds[0]);
            }
         }
         else
         {
            if (can_hide)
            {
               // Redirect stderr to GUI reader.
               close(2);
               dup(fds[1]);
            }
            if (execvp(path, const_cast<char**>(cmd)) < 0)
               exit(255);
         }

         if (can_hide)
            setVisible();
      }
#else
      // This will be hell on Earth :v
      void fork_ssnes(const string& path, const char **cmd)
      {
         if (OS::pendingEvents())
            OS::processEvents();

         bool can_hide = !general.getAsyncFork();

         if (can_hide)
         {
            setVisible(false);
            general.hide();
            video.hide();
            audio.hide();
            input.hide();
         }

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
            //print("Appending: ", *cmd, "\n"); 
            cmdline.append(*cmd++); 
            cmdline.append("\" "); 
         }

         //print(cmdline);
         //print("\n");

         PROCESS_INFORMATION piProcInfo;
         STARTUPINFO siStartInfo;
         BOOL bSuccess = FALSE;
         ZeroMemory(&piProcInfo, sizeof(piProcInfo));

         ZeroMemory(&siStartInfo, sizeof(siStartInfo));
         siStartInfo.cb = sizeof(siStartInfo);
         siStartInfo.hStdError = writer;
         siStartInfo.hStdOutput = NULL;
         siStartInfo.hStdInput = NULL;
         siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

         WCHAR wcmdline[1024] = {0};
         MultiByteToWideChar(CP_UTF8, 0, cmdline, cmdline.length(), wcmdline, 1023);

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

         if (can_hide)
         {
            if (bSuccess)
            {
               CloseHandle(writer);
               writer = NULL;
               log_win.clear();
               char buf[1024];
               DWORD dwRead;
               while (ReadFile(reader, buf, sizeof(buf) - 1, &dwRead, NULL) == TRUE && dwRead > 0)
               {
                  buf[dwRead] = '\0';
                  log_win.push(buf);
               }
               setStatusText("SSNES returned successfully!");
            }
            else
            {
               setStatusText("Failed to start SSNES");
            }
         }

         if (reader)
            CloseHandle(reader);
         if (writer)
            CloseHandle(writer);

         if (can_hide)
            setVisible();
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

         RadioItem gamepad_2;
         RadioItem multitap_2;
         RadioItem mouse_2;
         RadioItem scope_2;
         RadioItem justifier_2;
         RadioItem justifiers_2;
      } settings;

      struct
      {
         Item about;
      } help;

      void init_menu()
      {
         file_menu.setText("File");
         settings_menu.setText("Settings");
         help_menu.setText("Help");
         append(file_menu);
         append(settings_menu);
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

         settings.gamepad_2.setText("Gamepad");
         settings.multitap_2.setText("Multitap");
         settings.mouse_2.setText("Mouse");
         settings.scope_2.setText("SuperScope");
         settings.justifier_2.setText("Justifier");
         settings.justifiers_2.setText("Two Justifiers");

         settings_menu.append(settings.controllers);
         settings.controllers.append(settings.port_1);
         settings.controllers.append(settings.port_2);
         settings.port_1.append(settings.gamepad_1);
         settings.port_1.append(settings.mouse_1);

         settings.port_2.append(settings.gamepad_2);
         settings.port_2.append(settings.multitap_2);
         settings.port_2.append(settings.mouse_2);
         settings.port_2.append(settings.scope_2);
         settings.port_2.append(settings.justifier_2);
         settings.port_2.append(settings.justifiers_2);

         RadioItem::group(settings.gamepad_1, settings.mouse_1);
         RadioItem::group(settings.gamepad_2, settings.multitap_2, settings.mouse_2, settings.scope_2, settings.justifier_2, settings.justifiers_2);

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
      }
};

int main()
{
   MainWindow win;
   OS::main();
}
