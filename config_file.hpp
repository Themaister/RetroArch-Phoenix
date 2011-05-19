#ifndef __CONFIG_FILE_HPP
#define __CONFIG_FILE_HPP

#include "config_file.h"
#include <phoenix.hpp>
#include <utility>
using namespace nall;

class ConfigFile
{
   public:
      ConfigFile(const string& _path = "") : path(_path)
      {
         conf = config_file_new(path);
         if (!conf)
            conf = config_file_new(NULL);
      }

      ConfigFile(const ConfigFile&) = delete;
      void operator=(const ConfigFile&) = delete;

      operator bool() { return conf; }

      ConfigFile& operator=(ConfigFile&& _in) 
      { 
         if (conf) 
         {
            if (path[0])
               config_file_write(conf, path);
            config_file_free(conf); 
            conf = _in.conf; 
            _in.conf = NULL;
            path = _in.path;
         }
         return *this;
      }

      bool get(const string& key, int& out) 
      { 
         if (!conf) return false;
         int val; 
         if (config_get_int(conf, key, &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      bool get(const string& key, char& out) 
      { 
         if (!conf) return false;
         char val; 
         if (config_get_char(conf, key, &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      bool get(const string& key, bool& out) 
      { 
         if (!conf) return false;
         bool val; 
         if (config_get_bool(conf, key, &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      bool get(const string& key, string& out) 
      { 
         if (!conf) return false;
         char *val;
         if (config_get_string(conf, key, &val))
         {
            out = val;
            std::free(val);
            return out.length() > 0;
         }
         return false;
      }

      bool get(const string& key, double& out) 
      { 
         if (!conf) return false;
         double val;
         if (config_get_double(conf, key, &val))
         {
            out = val;
            return true;
         }
         return false;
      }

      void set(const string& key, int val)
      {
         if (conf) config_set_int(conf, key, val);
      }

      void set(const string& key, double val)
      {
         if (conf) config_set_double(conf, key, val);
      }

      void set(const string& key, const string& val)
      {
         if (conf) config_set_string(conf, key, val);
      }

      void set(const string& key, char val)
      {
         if (conf) config_set_char(conf, key, val);
      }

      void set(const string& key, bool val)
      {
         if (conf) config_set_bool(conf, key, val);
      }

      void write() { if (conf && path[0]) config_file_write(conf, path); }

      ConfigFile(ConfigFile&& _in) { *this = std::move(_in); }

      ~ConfigFile() { if (conf) { if (path[0]) config_file_write(conf, path);  config_file_free(conf); } }


   private:
      config_file_t *conf;
      string path;
};


#endif
