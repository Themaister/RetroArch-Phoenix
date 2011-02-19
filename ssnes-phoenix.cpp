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
         setGeometry({128, 128, 320, 240});

         init_menu();
         
         onClose = []() { OS::quit(); };

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
      Menu file_menu, help_menu;
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
         MenuItem about;
      } help;

      void init_menu()
      {
         file_menu.setText("File");
         help_menu.setText("Help");
         append(file_menu);
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
         file.quit.onTick = []() { OS::quit(); };
         help.about.setText("About");
         help.about.onTick = []() { MessageWindow::information(Window::None, "SSNES/Phoenix\nHans-Kristian Arntzen (Themaister) (C) - 2011\nThis is free software released under GNU GPLv3\nPhoenix (C) byuu - 2011"); };

         file_menu.append(file.open);
         file_menu.append(file.netplay_open);
         file_menu.append(file.sep);
         file_menu.append(file.quit);
         help_menu.append(help.about);
      }
};

int main()
{
   MainWindow win;
   win.show();
   OS::main();
}
