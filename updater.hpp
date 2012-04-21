#ifndef __RetroArch_UPDATER_HPP
#define __RetroArch_UPDATER_HPP

#include <phoenix.hpp>
#include <vector>
#include <nall/thread.hpp>
#include <nall/function.hpp>

using namespace phoenix;

class Updater : public Window
{
   public:
      Updater();

      void show();
      void hide();
      void cancel();

      void update(const char *content, unsigned size);
      bool progress_update(unsigned now, unsigned total);

      nall::function<void (const nall::string &path)> libretro_path_cb;
      nall::function<nall::string()> retroarch_path_cb;

   private:
      Timer timer;
      void timer_event();

      void start_download(const nall::string &path);
      void download_thread(const nall::string &path);
      void update_progress();
      bool extract_zip(const nall::string &path);
      void move_self_exe();
      void delete_old_exe();
      static unsigned current_redist_version();
      struct
      {
         bool finished;
         bool success;
         bool cancelled;
         std::vector<char> data;
         nall::mutex lock;

         nall::string version;
         nall::string retroarch_version;
         unsigned redist_version;

         bool version_only;

         nall::string file_path;

         unsigned now, total;

         bool libretro;
         nall::string libretro_path;
      } transfer;

#if 0
      Label server_label;
      LineEdit server_url;
      HorizontalLayout server_hbox;
#endif

      ProgressBar progress;
      Label progress_label;
      HorizontalLayout progress_layout;

      Button version_download;
      Button download;
      Button cancel_download;
      HorizontalLayout dl_layout;

      Label latest_label;
      Label current_label;

      Label opts_arch, opts_build;
      RadioBox opts_32bit, opts_64bit;
      RadioBox opts_slim, opts_full, opts_redist;
      HorizontalLayout opts_layout;

      HorizontalLayout libretro_buttons;
      Button libretro_download;
      Button libretro_use;

      ListView libretro_listview;
      struct libretro_desc
      {
         nall::string system;
         nall::string core;
         nall::string version;
         nall::string arch;
         nall::string basename;
         bool downloaded;
      };
      nall::linear_vector<libretro_desc> libretro_list;
      nall::linear_vector<libretro_desc> libretro_current;
      static libretro_desc line2desc(const nall::string &line);
      void update_listview();
      void update_retroarch_version();

      void initiate_download();
      void enable_downloads();
      void disable_downloads();
      void end_transfer_list();
      bool end_file_transfer();

      VerticalLayout vbox;

      static const char *base_host()   { return "themaister.net"; }
      static const char *base_folder() { return "/retroarch-dl/"; }
      static const char *latest_file() { return "latest"; }

      static nall::string basedir();
};

#endif

