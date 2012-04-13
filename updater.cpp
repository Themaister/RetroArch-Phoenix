#ifdef _WIN32

#include <nall/http.hpp>
#include <nall/thread.hpp>
#include <nall/file.hpp>
#include <nall/zip.hpp>
#include "updater.hpp"
#include <stdio.h>

using namespace nall;

Updater::Updater()
{
   // After an update, killit! :D
   delete_old_exe();

   setTitle("SSNES || Updater");
   onClose = [this]{ this->hide(); };

   timer.onTimeout = {&Updater::timer_event, this};
   timer.setInterval(50);
   timer.setEnabled(false);

   version_download.setText("Check version");
   dl_layout.append(version_download, 0, 0);
   download.setText("Download SSNES");
   dl_layout.append(download, 0, 0);
   cancel_download.setText("Cancel");
   dl_layout.append(cancel_download, 0, 0);
   vbox.append(dl_layout, 5);

   progress.setPosition(0);
   progress_layout.append(progress, 300, 0, 10);
   progress_label.setText("N/A");
   progress_layout.append(progress_label, 100, 0, 3);
   vbox.append(progress_layout);

   opts_32bit.setText("x86");
   opts_64bit.setText("x86_64");
   opts_slim.setText("Slim");
   opts_full.setText("Full");
   opts_redist.setText("Redist");
   RadioBox::group(opts_32bit, opts_64bit);
   RadioBox::group(opts_slim, opts_full, opts_redist);

   opts_arch.setText("CPU:");
   opts_layout.append(opts_arch, 0, 0, 5);
   opts_layout.append(opts_32bit, 0, 0);
   opts_layout.append(opts_64bit, 0, 0, 30);
   opts_build.setText("Build:");
   opts_layout.append(opts_build, 0, 0, 5);
   opts_layout.append(opts_slim, 0, 0);
   opts_layout.append(opts_full, 0, 0);
   opts_layout.append(opts_redist, 0, 0);
   vbox.append(opts_layout, 3);

#ifdef _WIN64
   opts_64bit.setChecked();
#else
   opts_32bit.setChecked();
#endif

   update_ssnes_version();
   opts_full.setChecked();

   latest_label.setText("Latest release: N/A");
   current_label.setText("Downloaded release: N/A");
   vbox.append(latest_label, ~0, 0);
   vbox.append(current_label, ~0, 0, 20);

   libretro_download.setText("Download core");
   libretro_use.setText("Use core");
   libretro_download.onTick = [this] {
      if (!libretro_listview.selected())
      {
         MessageWindow::warning(*this, "Select core to download first.");
         return;
      }

      initiate_download();
   };

   libretro_use.onTick = [this] {
      if (!libretro_listview.selected())
      {
         MessageWindow::warning(*this, "Select core to use first.");
         return;
      }

      const auto &elem = libretro_current[libretro_listview.selection()];
      string path = {basedir(), elem.basename, ".dll"};
      if (!nall::file::exists(path))
      {
         auto response = MessageWindow::information(*this,
               "This core is not downloaded yet.\n"
               "Would you like to download it now?",
               MessageWindow::Buttons::YesNo);

         if (response == MessageWindow::Response::Yes)
            initiate_download();
      }
      else
         libretro_path_cb(path);
   };

   libretro_buttons.append(libretro_download, 0, 0);
   libretro_buttons.append(libretro_use, 0, 0);
   vbox.append(libretro_buttons, 5);

   libretro_listview.setHeaderText("System", "Core", "Version", "Architecture", "Library", "Downloaded");
   libretro_listview.setHeaderVisible();
   libretro_listview.autoSizeColumns();
   vbox.append(libretro_listview, 550, 250);

   vbox.setMargin(5);

   auto minimum = vbox.minimumGeometry();
   setGeometry({128, 128, minimum.width, minimum.height});


   version_download.onTick = [this] {
      transfer.version_only = true;
      transfer.libretro = false;
      start_download(latest_file());
   };

   download.onTick = [this] {

      transfer.version_only = false;
      transfer.libretro = false;
      
      nall::string path;
      path.append(opts_redist.checked() ? "SSNES-win" : "ssnes-win");
      path.append(opts_32bit.checked() ? "32-" : "64-");
      if (opts_redist.checked())
         path.append("libs.zip");
      else
      {
         path.append(transfer.version, "-");
         path.append(opts_slim.checked() ? "slim.zip" : "full.zip");
      }

      start_download(path);
   };

   libretro_listview.onActivate = {&Updater::initiate_download, this};

   cancel_download.onTick = [this] {
      nall::scoped_lock lock(transfer.lock);
      transfer.cancelled = true;
   };

   opts_32bit.onTick = {&Updater::update_listview, this};
   opts_64bit.onTick = {&Updater::update_listview, this};

   append(vbox);

   disable_downloads();
   cancel_download.setEnabled(false);
}

void Updater::initiate_download()
{
   transfer.version_only = false;
   transfer.libretro = true;

   const auto &elem = libretro_current[libretro_listview.selection()];
   transfer.libretro_path = {basedir(), elem.basename, ".dll"};

   if (elem.downloaded)
   {
      auto response = MessageWindow::information(*this,
            "This core is already downloaded.\nWould you like to use it?",
            MessageWindow::Buttons::YesNo);

      if (response == MessageWindow::Response::Yes)
      {
         if (response == MessageWindow::Response::Yes && libretro_path_cb)
            libretro_path_cb(transfer.libretro_path);
      }
      else
      {
         response = MessageWindow::information(*this,
               "Would you like to redownload it (in case there was a hotfix)?",
               MessageWindow::Buttons::YesNo);

         if (response == MessageWindow::Response::Yes)
            start_download({elem.basename, ".zip"});
      }
   }
   else
      start_download({elem.basename, ".zip"});
}

void Updater::start_download(const string &path)
{
   transfer.finished = false;
   transfer.success = false;
   transfer.cancelled = false;
   transfer.now = 0;
   transfer.total = 0;
   transfer.data.clear();

   progress.setPosition(0);
   disable_downloads();
   cancel_download.setEnabled(true);

   transfer.file_path = path;
   spawn_thread(&Updater::download_thread, this, path);

   timer.setEnabled(true);
}

void Updater::cancel()
{
   hide();
   progress_label.setText("N/A");
   nall::scoped_lock lock(transfer.lock);
   transfer.cancelled = true;
}

void Updater::show()
{
   update_ssnes_version();
   setVisible();
}

void Updater::hide()
{
   setVisible(false);
}

// This is awkward as shit. We can't overwrite ourselves (ssnes-phoenix.exe),
// but we can rename ourselves and overwrite ... :D
void Updater::move_self_exe()
{
   char path[MAX_PATH];
   GetModuleFileName(GetModuleHandle(0), path, sizeof(path));
   nall::string new_path(path, ".old-deleteme");
   MoveFile(path, new_path);
}

void Updater::delete_old_exe()
{
   char path[MAX_PATH];
   GetModuleFileName(GetModuleHandle(0), path, sizeof(path));
   nall::string backup_path(path, ".old-deleteme");
   DeleteFile(backup_path);
}

bool Updater::extract_zip(const nall::string &path)
{
   nall::zip z;
   if (!z.open({basedir(), path}))
   {
      MessageWindow::critical(*this, "Failed opening ZIP!");
      return false;
   }

   foreach (file, z.file)
   {
      // Don't overwrite config files.
      if (file.name.endswith(".cfg") && nall::file::exists({basedir(), file.name}))
         continue;

      uint8_t *data;
      unsigned size;
      if (!z.extract(file, data, size))
         continue;

      if (file.name == "ssnes-phoenix.exe") // Oh snap, we have to do magic trickery! :D
         move_self_exe();

      if (!nall::file::write({basedir(), file.name}, data, size))
      {
         delete [] data;
         continue;
      }

      delete [] data;
   }

   z.close();

   nall::string rmpath(basedir(), path);
#ifdef _WIN32
   DeleteFile(rmpath);
#else
   ::unlink(rmpath);
#endif

   return true;
}

Updater::libretro_desc Updater::line2desc(const nall::string &line)
{
   nall::lstring list;
   list.split(",", line);

   static const nall::string na = "<Invalid>";
   if (list.size() < 5)
      return {na, na, na, na, na};

   return { // This is starting to look like JavaScript ... :D
      list[0].trim(" ", " "),
      list[1].trim(" ", " "),
      list[2].trim(" ", " "),
      list[3].trim(" ", " "),
      list[4].trim(" ", " "),
   };
}

void Updater::end_transfer_list()
{
   if (transfer.data.size() == 0)
      return;

   transfer.data.push_back('\0');

   nall::lstring list;
   list.split("\n", transfer.data.data());
   if (list.size() == 0)
      return;

   transfer.version = list[0];
   list.remove(0);
   transfer.redist_version = strtoul(list[0], 0, 0);
   list.remove(0);

   string latest("Latest release: ", transfer.version);
   latest_label.setText({"Latest release: ", transfer.version});

   version_download.setEnabled(false);

   foreach (elem_, list)
   {
      if (elem_.length() > 0)
      {
         const auto &elem = line2desc(elem_);
         libretro_list.append(elem);
      }
   }

   update_listview();

   if (transfer.version == transfer.ssnes_version)
      MessageWindow::information(*this,
            "SSNES is up to date!");
}

void Updater::update_listview()
{
   libretro_listview.reset();
   libretro_current.reset();

   bool x86 = opts_32bit.checked();
   foreach (elem, libretro_list)
   {
      if (opts_32bit.checked() && elem.arch == "x86")
         libretro_current.append(elem);
      else if (opts_64bit.checked() && elem.arch == "x86_64")
         libretro_current.append(elem);
   }

   auto dir = basedir();
   foreach (elem, libretro_current)
   {
      elem.downloaded = nall::file::exists({dir, elem.basename, ".dll"});
      libretro_listview.append(elem.system, elem.core, elem.version, elem.arch, nall::string(elem.basename, ".dll"), nall::string(elem.downloaded ? "Yes" : "No"));
   }

   libretro_listview.autoSizeColumns();
}

bool Updater::end_file_transfer()
{
   bool valid = false;
   if (nall::file::write({basedir(), transfer.file_path},
            (const uint8_t *)transfer.data.data(),
            transfer.data.size()))
   {
      valid = true;
   }
   else
      MessageWindow::critical(*this, "Failed saving archive to disk!");

   if (valid && extract_zip(transfer.file_path))
   {
      if (transfer.libretro)
      {
         auto response = MessageWindow::information(*this,
               {"Extracted core to ", transfer.libretro_path,
               ".\nDo you want to use this core?"}, MessageWindow::Buttons::YesNo);

         if (response == MessageWindow::Response::Yes && libretro_path_cb)
            libretro_path_cb(transfer.libretro_path);

         update_listview();
      }
      else
      {
         MessageWindow::information(*this, "Extracted archive!");
         update_ssnes_version();
      }
   }
   else if (valid)
   {
      MessageWindow::critical(*this, "Failed opening ZIP!");
      return false;
   }

   return true;
}

void Updater::timer_event()
{
   nall::scoped_lock lock(transfer.lock);

   if (transfer.finished)
   {
      timer.setEnabled(false);
      if (transfer.success)
      {
         progress.setPosition(100);

         if (transfer.version_only)
            end_transfer_list();
         else
         {
            bool success = end_file_transfer();

            if (success)
            {
               if (opts_full.checked() && (current_redist_version() != transfer.redist_version))
               {
                  auto response = MessageWindow::information(*this,
                        {"You downloaded full build, but redist is outdated.\n",
                        "Current: ", current_redist_version(), "\n",
                        "Available: ", transfer.redist_version, "\n",
                        "Do you want to download it now?"},
                        MessageWindow::Buttons::YesNo);

                  if (response == MessageWindow::Response::Yes)
                  {
                     transfer.libretro = false;
                     transfer.version_only = false;
                     nall::string arch(opts_32bit.checked() ? "32-" : "64-");
                     start_download({"SSNES-win", arch, "libs.zip"});
                     return;
                  }
               }

               if (!transfer.libretro && !opts_redist.checked())
               {
                  MessageWindow::information(*this,
                        "SSNES-Phoenix is updated. Restart the program to complete the update.");
               }
            }
         }
      }
      else
         MessageWindow::warning(*this, "Download was not completed!");

      cancel_download.setEnabled(false);

      if (transfer.version.length() > 0)
         enable_downloads();
   }
   else if (transfer.cancelled)
   {
      timer.setEnabled(false);
      enable_downloads();
      cancel_download.setEnabled(false);
   }

   update_progress();
}

unsigned Updater::current_redist_version()
{
   nall::string path = {basedir(), "ssnes-redist-version"};
   uint8_t *data;
   unsigned size;
   if (nall::file::read(path, data, size))
   {
      std::vector<char> buf(data, data + size);
      buf.push_back('\0');
      delete [] data;
      return strtoul(buf.data(), 0, 0);
   }
   else
      return 0;
}

void Updater::update_progress()
{
   string text;
   if (transfer.now > 1000000)
   {
      text.append(nall::fp(transfer.now / 1000000.0, 3));
      text.append(" MB");
   }
   else if (transfer.now > 1000)
   {
      text.append(nall::fp(transfer.now / 1000.0, 3));
      text.append(" kB");
   }
   else
      text.append(transfer.now, " B");

   if (transfer.total)
      progress.setPosition(transfer.now * 100 / transfer.total);

   progress_label.setText(text);
}

void Updater::download_thread(const nall::string &path)
{
   nall::http dl;
   dl.write_cb = {&Updater::update, this};
   dl.progress_cb = {&Updater::progress_update, this};

   bool ret;
   if ((ret = dl.connect(base_host())))
   {
      ret = dl.download({base_folder(), path});
      dl.disconnect();
   }

   nall::scoped_lock lock(transfer.lock);
   transfer.success = ret;
   transfer.finished = true;
}

void Updater::update(const char *content, unsigned size)
{
   nall::scoped_lock lock(transfer.lock);
   transfer.data.insert(transfer.data.end(), content, content + size);
   transfer.now = transfer.data.size();
}

bool Updater::progress_update(unsigned now, unsigned total)
{
   nall::scoped_lock lock(transfer.lock);

   transfer.now = now;
   transfer.total = total;

   return !transfer.cancelled;
}

nall::string Updater::basedir()
{
   // Windows is completely batshit retarded and the relative path might "change" by going into the file manager, so we have to manually figure out the full path. :)
#ifdef _WIN32
   char dir_path[MAX_PATH];
   GetModuleFileName(GetModuleHandle(0), dir_path, sizeof(dir_path));
   char *split = strrchr(dir_path, '\\');
   if (!split) split = strrchr(dir_path, '/');
   if (split) split[1] = '\0';
   return dir_path;
   //MessageWindow::information(*this, path);
#else
   return nall::string();
#endif
}

void Updater::enable_downloads()
{
   download.setEnabled(true);
   libretro_listview.setEnabled(true);
}

void Updater::disable_downloads()
{
   download.setEnabled(false);
   libretro_listview.setEnabled(false);
}

void Updater::update_ssnes_version()
{
   if (!ssnes_path_cb)
      return;

   FILE *file = popen(nall::string(ssnes_path_cb(), " --help"), "r");
   if (!file)
   {
      current_label.setText("Downloaded release: N/A");
      return;
   }

   nall::string version = "N/A";

   char buffer[1024] = {0};
   fgets(buffer, sizeof(buffer), file);
   fgets(buffer, sizeof(buffer), file);

   // Second line of --help has "-- v13.37 --". :D
   const char *start = strstr(buffer, "-- v");
   if (start)
   {
      start += 4;
      char *end = strchr(start, ' ');
      if (end)
         *end = '\0';

      version = start;
   }

   current_label.setText({"Downloaded release: ", version});
   fclose(file);
   transfer.ssnes_version = version;
}

#endif

