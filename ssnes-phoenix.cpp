#include <phoenix.hpp>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "config_file.hpp"
#include <utility>
#include <functional>
#include "settings.hpp"

using namespace nall;
using namespace phoenix;


class LogWindow : public ToggleWindow
{
   public:
      LogWindow() : ToggleWindow("SSNES || Log window")
      {
         label.setText("SSNES output:");
         layout.append(label, 0, 30);
         layout.append(box, 0, 0);
         box.setEditable(false);
         layout.setMargin(5);
         append(layout);
      }

      void push(const char *text) { log.append(text); box.setText(log); while(OS::pending()) OS::process(); }
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
      MainWindow() : general(configs.cli), video(configs.cli), audio(configs.cli)
      {
         setTitle("SSNES || Phoenix");
         //setBackgroundColor(64, 64, 64);
         setGeometry({128, 128, 600, 240});

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


      struct
      {
         ConfigFile gui, cli;
      } configs;

      struct logger : public LogWindow
      {
         logger()
         {
            label.setText("Show log window:");
            box.append(label, 100, 30);
            box.append(check, 20, 30);
            check.onTick = [this]() { if (check.checked()) this->show(); else this->hide(); };
         }

         HorizontalLayout box;
         Label label;
         CheckBox check;

         HorizontalLayout& layout() { return box; }
      } log_win;

      struct entry
      {
         HorizontalLayout hlayout;
         Label label;
         TextEdit edit;
         Button button;
         string filter;

         ConfigFile *conf;
         string config_key;

         function<void (const string&)> cb;

         entry() : conf(NULL), cb(&entry::dummy)
         {
            button.setText("Open ...");

            hlayout.append(label, 100, 30);
            hlayout.append(edit, 0, 30);
            hlayout.append(button, 60, 30);

            button.onTick = [this]() {
               string start_path;
               const char *path = std::getenv("HOME");
               if (path)
                  start_path = path;

               string file = OS::fileLoad(Window::None, start_path, filter);
               if (file.length() > 0)
                  edit.setText(file);

               if (conf)
                  conf->set(config_key, getPath());
               cb(getPath());
            };
         }


         void setFilter(const string& _filter) { filter = _filter; }
         void setLabel(const string& name) { label.setText(name); }
         void setPath(const string& name) { edit.setText(name); }
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
      } rom, config, ssnes, libsnes;

      void init_config()
      {
         string gui_path, cli_path;
         string tmp;
         const char *path = std::getenv("XDG_CONFIG_HOME");
         if (path)
         {
            gui_path = path;
            gui_path.append("/ssnes/phoenix.cfg");
         }
         else
            gui_path = "/etc/ssnes_phoenix.cfg";

         configs.gui = gui_path;

         if (configs.gui.get("ssnes_path", tmp)) ssnes.setPath(tmp);
         ssnes.setConfig(configs.gui, "ssnes_path");
         if (configs.gui.get("last_rom", tmp)) rom.setPath(tmp);
         rom.setConfig(configs.gui, "last_rom");
         if (configs.gui.get("config_path", tmp)) config.setPath(tmp);
         config.setConfig(configs.gui, "config_path", {&MainWindow::reload_cli_config, this});
         libsnes.setConfig(configs.cli, "libsnes_path");

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
         configs.cli = ConfigFile(cli_path);

         init_cli_config();
      }

      void reload_cli_config(const string& path)
      {
         configs.cli = ConfigFile(path);
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
      }

      void init_main_frame()
      {
         rom.setFilter("Super Magicom, Super Famicom (*.smc,*.sfc)");
         config.setFilter("Config file (*.cfg)");
#ifdef _WIN32
#define DYNAMIC_EXTENSION "dll"
#elif defined(__APPLE__)
#define DYNAMIC_EXTENSION "dylib"
#else
#define DYNAMIC_EXTENSION "so"
#endif
         libsnes.setFilter("Dynamic library (*." DYNAMIC_EXTENSION ")");
#ifdef _WIN32
         ssnes.setFilter("Executable file (*.exe)");
#endif

         rom.setLabel("ROM path:");
         config.setLabel("SSNES config file:");
         ssnes.setLabel("SSNES path:");
         libsnes.setLabel("libsnes path:");

         start_btn.setText("Start SSNES");
         vbox.append(rom.layout(), 0, 0, 3);
         vbox.append(config.layout(), 0, 0, 3);
         vbox.append(ssnes.layout(), 0, 0, 3);
         vbox.append(libsnes.layout(), 0, 0, 3);
         vbox.append(start_btn, 0, 0, 15);
         vbox.append(log_win.layout(), 0, 0);

         start_btn.onTick = [this]() { start_ssnes(); };
      }

      void show_error(const string& err)
      {
         MessageWindow::warning(Window::None, err);
      }

      void start_ssnes()
      {
         linear_vector<const char*> vec_cmd;
         string ssnes_path = ssnes.getPath();
         if (ssnes_path.length() == 0) ssnes_path = "ssnes";
         string rom_path = rom.getPath();
         string config_path = config.getPath();

         vec_cmd.append("ssnes");

         if (rom_path.length() == 0)
         {
            show_error("No ROM selected :(");
            return;
         }

         vec_cmd.append(rom_path);

         if (config_path.length() > 0)
         {
            vec_cmd.append("-c");
            vec_cmd.append(config_path);
         }

         vec_cmd.append("-v");

         vec_cmd.append(NULL);

         configs.gui.write();
         configs.cli.write();
         fork_ssnes(ssnes_path, &vec_cmd[0]);
      }

#ifndef _WIN32
      void fork_ssnes(const string& path, const char **cmd)
      {
         // I think we had to do this with GTK+ at least :v
         while (OS::pending())
            OS::process();

         setVisible(false);
         general.hide();
         video.hide();
         audio.hide();
         input.hide();

         // Gotta love Unix :)
         int fds[2];
         pipe(fds);
         close(2);
         dup(fds[1]);

         if (fork())
         {
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
         }
         else
         {
            if (execvp(path, const_cast<char**>(cmd)) < 0)
               exit(255);
         }

         close(fds[0]);
         close(fds[1]);

         setVisible();
      }
#else
      // This will be hell on Earth :v
      void fork_ssnes(const string& cmd)
      {
         setStatusText("This isn't implemented yet. :(");
      }
#endif

      struct
      {
         MenuItem quit;
      } file;

      struct
      {
         MenuItem general;
         MenuSeparator sep;
         MenuItem video;
         MenuItem audio;
         MenuItem input;
      } settings;

      struct
      {
         MenuItem about;
      } help;

      void init_menu()
      {
         file_menu.setText("File");
         settings_menu.setText("Settings");
         help_menu.setText("Help");
         append(file_menu);
         append(settings_menu);
         append(help_menu);

         file.quit.setText("Quit");

         settings.general.setText("General");
         settings.video.setText("Video");
         settings.audio.setText("Audio");
         settings.input.setText("Input");

         help.about.setText("About");

         file_menu.append(file.quit);
         settings_menu.append(settings.general);
         settings_menu.append(settings.sep);
         settings_menu.append(settings.video);
         settings_menu.append(settings.audio);
         settings_menu.append(settings.input);
         help_menu.append(help.about);

         init_menu_callbacks();
      }

      void init_menu_callbacks()
      {
         file.quit.onTick = []() { OS::quit(); };
         help.about.onTick = []() { MessageWindow::information(Window::None, "SSNES/Phoenix\nHans-Kristian Arntzen (Themaister) (C) - 2011\nThis is free software released under GNU GPLv3\nPhoenix (C) byuu - 2011"); };

         settings.general.onTick = [this]() { general.show(); };
         settings.video.onTick = [this]() { video.show(); };
         settings.audio.onTick = [this]() { audio.show(); };
         settings.input.onTick = [this]() { input.show(); };
      }
};

int main()
{
   MainWindow win;
   OS::main();
}
