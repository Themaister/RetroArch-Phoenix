#ifndef __SSNES_UPDATER_HPP
#define __SSNES_UPDATER_HPP

#include <phoenix.hpp>
#include <vector>
#include <nall/thread.hpp>

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

   private:
      Timer timer;
      void timer_event();

      void start_download(const nall::string &path);
      void download_thread(const nall::string &path);
      void update_progress();
      bool extract_zip(const nall::string &path);
      struct
      {
         bool finished;
         bool success;
         bool cancelled;
         std::vector<char> data;
         nall::mutex lock;

         nall::string version;
         bool version_only;

         nall::string file_path;

         unsigned now, total;
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

      Label opts_arch, opts_build;
      RadioBox opts_32bit, opts_64bit;
      RadioBox opts_slim, opts_full, opts_redist;
      HorizontalLayout opts_layout;

      Label libsnes_label;
      Label libsnes_dlhint;
      ListView libsnes_listview;
      struct libsnes_desc
      {
         nall::string system;
         nall::string core;
         nall::string version;
         nall::string arch;
         nall::string basename;
      };
      nall::linear_vector<libsnes_desc> libsnes_list;
      nall::linear_vector<libsnes_desc> libsnes_current;
      static libsnes_desc line2desc(const nall::string &line);
      void update_listview();

      void enable_downloads();
      void disable_downloads();
      void end_transfer_list();
      void end_file_transfer();

      VerticalLayout vbox;

      static const char *base_host()   { return "themaister.net"; }
      static const char *base_folder() { return "/ssnes-dl/"; }
      static const char *latest_file() { return "latest"; }

      static nall::string basedir();
};

#endif

