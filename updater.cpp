#include "updater.hpp"
#include <nall/thread.hpp>
#include <nall/file.hpp>
#include <nall/zip.hpp>
#include <nall/http.hpp>

using namespace nall;

Updater::Updater()
{
   setTitle("SSNES || Update");
   onClose = [this]{ this->hide(); };

   timer.onTimeout = {&Updater::timer_event, this};
   timer.setInterval(50);

#if 0
   server_label.setText("URL:");
   server_hbox.append(server_label, 100, 0, 10);
   server_url.setText(base_url);"http://themaister.net/ssnes-dl/latest");
   server_hbox.append(server_url, 300, 0);

   vbox.append(server_hbox);
#endif

   download.setEnabled(false);
   cancel_download.setEnabled(false);

   version_download.setText("Check version");
   dl_layout.append(version_download, 0, 0);
   download.setText("Download");
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
   opts_64bit.setText("x64");
   opts_slim.setText("Slim");
   opts_full.setText("Full");
   opts_redist.setText("Redist");
   RadioBox::group(opts_32bit, opts_64bit);
   RadioBox::group(opts_slim, opts_full, opts_redist);

   opts_arch.setText("CPU:");
   opts_layout.append(opts_arch, 0, 0);
   opts_layout.append(opts_32bit, 0, 0);
   opts_layout.append(opts_64bit, 0, 0, 30);
   opts_build.setText("Build:");
   opts_layout.append(opts_build, 0, 0);
   opts_layout.append(opts_slim, 0, 0);
   opts_layout.append(opts_full, 0, 0);
   opts_layout.append(opts_redist, 0, 0);
   vbox.append(opts_layout, 3);

   opts_32bit.setChecked();
   opts_slim.setChecked();

   latest_label.setText("Latest release: N/A");
   vbox.append(latest_label, ~0, 0);

   vbox.setMargin(5);

   auto minimum = vbox.minimumGeometry();
   setGeometry({128, 128, minimum.width, minimum.height});


   version_download.onTick = [this] {
      transfer.version_only = true;
      start_download(latest_file);
   };

   download.onTick = [this] {
      transfer.version_only = false;
      
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

   cancel_download.onTick = [this] {
      scoped_lock(transfer.lock);
      transfer.cancelled = true;
   };

   append(vbox);
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
   download.setEnabled(false);
   cancel_download.setEnabled(true);

   transfer.file_path = path;
   spawn_thread(&Updater::download_thread, this, path);

   timer.setEnabled(true);
}

void Updater::cancel()
{
   hide();
   progress_label.setText("N/A");
   scoped_lock(transfer.lock);
   transfer.cancelled = true;
}

void Updater::show()
{
   setVisible();
}

void Updater::hide()
{
   setVisible(false);
}

bool Updater::extract_zip(const nall::string &path)
{
   nall::zip z;
   if (!z.open(path))
   {
      MessageWindow::critical(*this, "Failed opening ZIP!");
      return false;
   }

   foreach (file, z.file)
   {
      uint8_t *data;
      unsigned size;
      if (!z.extract(file, data, size))
      {
         MessageWindow::critical(*this, "Failed extracting file!");
         return false;
      }

      if (!nall::file::write(file.name, data, size))
      {
         delete [] data;
         return false;
      }

      delete [] data;
   }

   return true;
}

void Updater::timer_event()
{
   scoped_lock(transfer.lock);

   if (transfer.finished)
   {
      if (transfer.success)
      {
         progress.setPosition(100);

         if (transfer.version_only)
         {
            if (transfer.data.size() == 0)
               return;

            transfer.data.back() = 0; // Remove EOF.
            transfer.version = transfer.data.data();

            string latest("Latest release: ", transfer.version);
            latest_label.setText(latest);

            download.setEnabled(true);
            version_download.setEnabled(false);
         }
         else
         {
            bool valid = false;
            if (nall::file::write(transfer.file_path,
                  (const uint8_t *)transfer.data.data(),
                  transfer.data.size()))
            {
               valid = true;
            }
            else
               MessageWindow::critical(*this, "Failed saving archive to disk!");

            if (valid && extract_zip(transfer.file_path))
               MessageWindow::information(*this, "Extracted archive! :)");
            else if (valid)
               MessageWindow::critical(*this, "Failed opening ZIP!");
         }

         download.setEnabled(true);
      }
      else
         MessageWindow::warning(*this, "Download was not completed!");

      timer.setEnabled(false);
      cancel_download.setEnabled(false);
   }
   else if (transfer.cancelled)
   {
      timer.setEnabled(false);
      download.setEnabled(true);
      cancel_download.setEnabled(false);
   }

   update_progress();
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
   if ((ret = dl.connect(base_host)))
   {
      //print("Connected!\n");
      ret = dl.download({base_folder, path});
      //print("Finished? ", ret, "\n");
   }
   else
      print("Failed to connect!\n");

   scoped_lock(transfer.lock);
   transfer.success = ret;
   transfer.finished = true;
}

void Updater::update(const char *content, unsigned size)
{
   scoped_lock(transfer.lock);
   transfer.data.insert(transfer.data.end(), content, content + size);
   transfer.now = transfer.data.size();
}

bool Updater::progress_update(unsigned now, unsigned total)
{
   scoped_lock(transfer.lock);

   transfer.now = now;
   transfer.total = total;

   return !transfer.cancelled;
}

