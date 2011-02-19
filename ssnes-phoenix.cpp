#include <phoenix.hpp>

using namespace nall;
using namespace phoenix;

class MainWindow : public Window
{
   public:
      MainWindow()
      {
         setTitle("SSNES || Phoenix");
         setBackgroundColor(64, 64, 64);
         setGeometry({128, 128, 320, 160});

         init_menu();
         onClose = []() { OS::quit(); };

         init_main_frame();

         vbox.setMargin(5);
         append(vbox);
         setMenuVisible();
         setStatusVisible();
      }

      void show()
      {
         setVisible();
      }

      void hide()
      {
         setVisible(false);
      }



   private:
      VerticalLayout vbox;
      Menu file_menu, settings_menu, help_menu;

      Button start_btn;

      HorizontalLayout rom_layout;
      Label rom_name;
      TextEdit rom_edit;

      HorizontalLayout config_layout;
      Label config_name;
      TextEdit config_edit;
      Button config_open;

      HorizontalLayout ssnes_layout;
      Label ssnes_name;
      TextEdit ssnes_edit;
      Button ssnes_open;

      HorizontalLayout libsnes_layout;
      Label libsnes_name;
      TextEdit libsnes_edit;
      Button libsnes_open;

      void init_main_frame()
      {
         rom_layout.append(rom_name, 100, 30);
         rom_layout.append(rom_edit, 0, 30);
         rom_name.setText("ROM path:");

         config_layout.append(config_name, 100, 30);
         config_layout.append(config_edit, 0, 30);
         config_layout.append(config_open, 60, 30);
         config_name.setText("SSNES config file:");
         config_open.setText("Open ...");

         ssnes_layout.append(ssnes_name, 100, 30);
         ssnes_layout.append(ssnes_edit, 0, 30);
         ssnes_layout.append(ssnes_open, 60, 30);
         ssnes_name.setText("SSNES path:");
         ssnes_open.setText("Find ...");

         libsnes_layout.append(libsnes_name, 100, 30);
         libsnes_layout.append(libsnes_edit, 0, 30);
         libsnes_layout.append(libsnes_open, 60, 30);
         libsnes_name.setText("libsnes path:");
         libsnes_open.setText("Find ...");

         start_btn.setText("Start SSNES");
         vbox.append(rom_layout, 0, 0);
         vbox.append(config_layout, 0, 0);
         vbox.append(ssnes_layout, 0, 0);
         vbox.append(libsnes_layout, 0, 0);
         vbox.append(start_btn, 0, 0, 20);
      }

      struct
      {
         Menu open;
         MenuItem open_normal, open_sgb, open_bsx, open_bsx_slot;
         MenuSeparator open_sep;
         MenuItem netplay_open;
         MenuSeparator sep;
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

         file.open.setText("Open ROM");
         file.open_normal.setText("Normal");
         file.open_sgb.setText("Super GameBoy");
         file.open_bsx.setText("BSX Satellaview");
         file.open_bsx_slot.setText("BSX Satellaview (slotted)");
         file.open.append(file.open_normal);
         file.open.append(file.open_sep);
         file.open.append(file.open_sgb);
         file.open.append(file.open_bsx);
         file.open.append(file.open_bsx_slot);

         file.netplay_open.setText("Open ROM with Netplay");

         file.quit.setText("Quit");

         settings.general.setText("General");
         settings.video.setText("Video");
         settings.audio.setText("Audio");
         settings.input.setText("Input");


         help.about.setText("About");

         file_menu.append(file.open);
         file_menu.append(file.netplay_open);
         file_menu.append(file.sep);
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
      }
};

int main()
{
   MainWindow win;
   win.show();
   OS::main();
}
