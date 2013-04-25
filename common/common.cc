#include "common.hh"

#include <elle/assert.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/print.hh>
#include <elle/system/platform.hh>

#include <boost/preprocessor/cat.hpp>

#include <stdexcept>
#include <unordered_map>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define COMMON_DEFAULT_INFINIT_HOME ".infinit"
#define COMMON_DEFAULT_META_PROTOCOL "http"
#define COMMON_DEFAULT_META_HOST "meta.api.development.infinit.io"
#define COMMON_DEFAULT_META_PORT 12345
#define COMMON_DEFAULT_TROPHONIUS_PROTOCOL "http"
#define COMMON_DEFAULT_TROPHONIUS_HOST "trophonius.api.development.infinit.io"
#define COMMON_DEFAULT_TROPHONIUS_PORT 23456
#define COMMON_DEFAULT_RESOURCES_ROOT_URL "http://download.development.infinit.io"
#define COMMON_DEFAULT_LONGINUS_HOST "longinus.api.development.infinit.io"
#define COMMON_DEFAULT_LONGINUS_PORT 9999

#define COMMON_PRODUCTION_INFINIT_HOME ".infinit"
#define COMMON_PRODUCTION_META_PROTOCOL "http"
#define COMMON_PRODUCTION_META_HOST "v1.meta.api.production.infinit.io"
#define COMMON_PRODUCTION_META_PORT 12345
#define COMMON_PRODUCTION_TROPHONIUS_PROTOCOL "http"
#define COMMON_PRODUCTION_TROPHONIUS_HOST "v1.trophonius.api.production.infinit.io"
#define COMMON_PRODUCTION_TROPHONIUS_PORT 23456
#define COMMON_PRODUCTION_RESOURCES_ROOT_URL "http://download.production.infinit.io"
#define COMMON_PRODUCTION_LONGINUS_HOST "v1.longinus.api.production.infinit.io"
#define COMMON_PRODUCTION_LONGINUS_PORT 9999

#ifdef INFINIT_PRODUCTION_BUILD
# define VAR_PREFIX COMMON_PRODUCTION
#else
# define VAR_PREFIX COMMON_DEFAULT
#endif

# define COMMON_INFINIT_HOME \
  BOOST_PP_CAT(VAR_PREFIX, _INFINIT_HOME) \
/**/
# define COMMON_META_PROTOCOL \
  BOOST_PP_CAT(VAR_PREFIX, _META_PROTOCOL) \
/**/
# define COMMON_META_HOST \
  BOOST_PP_CAT(VAR_PREFIX, _META_HOST) \
/**/
# define COMMON_META_PORT \
  BOOST_PP_CAT(VAR_PREFIX, _META_PORT) \
/**/
# define COMMON_TROPHONIUS_PROTOCOL \
  BOOST_PP_CAT(VAR_PREFIX, _TROPHONIUS_PROTOCOL) \
/**/
# define COMMON_TROPHONIUS_HOST \
  BOOST_PP_CAT(VAR_PREFIX, _TROPHONIUS_HOST) \
/**/
# define COMMON_TROPHONIUS_PORT \
  BOOST_PP_CAT(VAR_PREFIX, _TROPHONIUS_PORT) \
/**/
# define COMMON_RESOURCES_ROOT_URL \
  BOOST_PP_CAT(VAR_PREFIX, _RESOURCES_ROOT_URL) \
/**/
# define COMMON_LONGINUS_HOST \
  BOOST_PP_CAT(VAR_PREFIX, _LONGINUS_HOST) \
/**/
# define COMMON_LONGINUS_PORT \
  BOOST_PP_CAT(VAR_PREFIX, _LONGINUS_PORT) \
/**/


namespace path = elle::os::path;

namespace
{

  std::string
  _home_directory()
  {
    struct passwd* pw = ::getpwuid(getuid());
    if (pw != nullptr && pw->pw_dir != nullptr)
      return std::string{pw->pw_dir};
    else
      return elle::os::getenv("HOME", "/tmp");
  }

  std::string
  _download_directory()
  {
    std::string download_dir = elle::os::getenv("INFINIT_DOWNLOAD_DIR", "");
    if (download_dir.length() > 0 && path::exists(download_dir) && path::is_directory(download_dir))
      return download_dir;

    std::string home_dir = _home_directory();
    std::string probable_download_dir = path::join(home_dir, "/Downloads");

    if (path::exists(probable_download_dir) && path::is_directory(probable_download_dir))
      return probable_download_dir;

    return home_dir;
  }

  std::string
  _infinit_home()
  {
    return elle::os::getenv(
        "INFINIT_HOME",
        common::system::home_directory() + "/" + COMMON_INFINIT_HOME
    );
  }

  std::string
  _built_binary_relative_path(std::string const& name)
  {
    static std::unordered_map<std::string, std::string> paths{
      {"8access",   "bin/8access"},
      {"8group",    "bin/8group"},
      {"8infinit",  "bin/8infinit"},
      {"8watchdog", "bin/8watchdog"},
      {"8transfer", "bin/8transfer"},
      {"8progress", "bin/8progress"},
    };
    auto it = paths.find(name);
    if (it == paths.end())
      throw std::runtime_error("Built binary '" + name + "' not registered");
    return it->second;
  }

  uint16_t
  _meta_port()
  {
    return std::stoi(
      elle::os::getenv(
        "INFINIT_META_PORT",
        std::to_string(COMMON_META_PORT)
      )
    );
  }

  uint16_t
  _trophonius_port()
  {
    std::string port_string = elle::os::getenv(
        "INFINIT_TROPHONIUS_PORT",
        elle::sprint(COMMON_TROPHONIUS_PORT)
    );
    std::stringstream ss(port_string);
    uint16_t port;
    ss >> port;
    if (ss.fail())
      throw std::runtime_error{
          "Couldn't retreive the port from '" + port_string + "'"
      };
    return port;
  }
}

namespace common
{

  namespace infinit
  {

    std::string const&
    home()
    {
      static std::string infinit_dir = _infinit_home();
      return infinit_dir;
    }

    std::string const&
    binary_path(std::string const& name, bool ensure)
    {
      static std::unordered_map<std::string, std::string> binaries;

      auto it = binaries.find(name);
      if (it != binaries.end())
        return it->second;

      static std::string build_dir = elle::os::getenv("INFINIT_BUILD_DIR", "");
      static std::string bin_dir = elle::os::getenv("INFINIT_BINARY_DIR", "");
      std::string path;
      if (bin_dir.size())
          path = elle::os::path::join(bin_dir, name);
      else if (build_dir.size())
          path = elle::os::path::join(build_dir,
                                      _built_binary_relative_path(name));
      else
        throw std::runtime_error{
          "Neither INFINIT_BUILD_DIR nor INFINIT_BINARY_DIR has been set"
        };

      if (ensure && !elle::os::path::exists(path))
        throw std::runtime_error("Cannot find any binary at '" + path + "'");

      return (binaries[name] = path);
    }

    std::string
    networks_directory(std::string const& user_id)
    {
      return path::join(user_directory(user_id), "networks");
    }

    std::string
    network_directory(std::string const& user_id,
                      std::string const& network_id)
    {
      return path::join(
        networks_directory(user_id),
        network_id
      );
    }

    std::string
    network_shelter(std::string const& user_id,
                    std::string const& network_id)
    {
      return path::join(
        network_directory(user_id, network_id), "shelter");
    }

    std::string
    passport_path(std::string const& user)
    {
      return path::join(infinit::user_directory(user), user + ".ppt");
    }

    std::string
    log_path(std::string const& user_id,
             std::string const& network_id)
    {
      return path::join(
        networks_directory(user_id), network_id + ".log");
    }

    std::string
    portal_path(std::string const& user_id,
                std::string const& network_id)
    {
      return path::join(network_directory(user_id, network_id), "portal.phr");
    }

    std::string
    user_directory(std::string const& user_id)
    {
      return path::join(home(), "users", user_id);
    }

    std::string
    identity_path(std::string const& user_id)
    {
        return path::join(
          infinit::user_directory(user_id),
          "identity"
        );
    }

  } // !infinit


  namespace system
  {

    std::string const&
    home_directory()
    {
      static std::string home_dir = _home_directory();
      return home_dir;
    }

    std::string const&
    platform()
    {
      static std::string const platform_ =
#ifdef INFINIT_LINUX
        "linux";
#elif INFINIT_MACOSX
        "macosx";
#elif INFINIT_WINDOWS
        "windows";
#else
# error "this platform is not supported"
#endif
        return platform_;
    }

    unsigned int
    architecture()
    {
      return sizeof(void*) * 8;
    }

    std::string const&
    download_directory()
    {
      static std::string download_dir = _download_directory();
      return download_dir;
    }

  } //!system

  namespace meta
  {

    std::string const&
    protocol()
    {
      static std::string const protocol = elle::os::getenv(
          "INFINIT_META_PROTOCOL",
          COMMON_META_PROTOCOL
      );
      return protocol;
    }

    uint16_t
    port()
    {
      static uint16_t const port = _meta_port();
      return port;
    }

    std::string const&
    host()
    {
      static std::string const host = elle::os::getenv(
          "INFINIT_META_HOST",
          COMMON_META_HOST
      );
      return host;
    }

    std::string const&
    url()
    {
      static std::string const url = elle::os::getenv(
          "INFINIT_META_URL",
          protocol() + "://" + host()
            + ":" + elle::sprint(port())
      );
      return url;
    }

  } // !meta

  namespace trophonius
  {

    std::string const&
    protocol()
    {
      static std::string const protocol = elle::os::getenv(
          "INFINIT_TROPHONIUS_PROTOCOL",
          COMMON_TROPHONIUS_PROTOCOL
      );
      return protocol;
    }

    uint16_t
    port()
    {
      static uint16_t const port = _trophonius_port();
      return port;
    }

    std::string const&
    host()
    {
      static std::string const host = elle::os::getenv(
          "INFINIT_TROPHONIUS_HOST",
          COMMON_TROPHONIUS_HOST
      );
      return host;
    }

    std::string const&
    url()
    {
      static std::string const url = elle::os::getenv(
          "INFINIT_TROPHONIUS_URL",
          protocol() + "://" + host()
            + ":" + elle::sprint(port())
      );
      return url;
    }
  } // !trophonius

  namespace resources
  {

    std::string
    base_url(char const* platform_,
             unsigned int architecture_)
    {
      static std::string const base_url = elle::os::getenv(
          "INFINIT_RESOURCES_ROOT_URL",
          COMMON_RESOURCES_ROOT_URL
      );
      std::string platform = (
          platform_ != nullptr ?
          std::string(platform_) :
          ::common::system::platform()
      );
      std::string architecture = elle::sprint(
          architecture_ != 0 ?
          architecture_ :
          ::common::system::architecture()
      );
      return base_url + "/" + platform + architecture;
    }

    std::string
    manifest(char const* platform,
             unsigned int architecture)
    {
      return base_url(platform, architecture) + "/manifest.xml";
    }

  }


  //- scheduled for deletion --------------------------------------------------

  namespace watchdog
  {

    std::string
    server_name(std::string const& user_id)
    {
      return path::join(infinit::user_directory(user_id), "server.wtg");
    }

    std::string
    lock_path(std::string const& user_id)
    {
        return path::join(
          infinit::user_directory(user_id),
          "lock.wtg"
        );
    }

    std::string
    log_path(std::string const& user_id)
    {
        return path::join(
          infinit::user_directory(user_id),
          "log.wtg"
        );
    }

    std::string
    id_path(std::string const& user_id)
    {
        return path::join(
          infinit::user_directory(user_id),
          "id.wtg"
        );
    }
  }

  namespace metrics
  {
    std::string const&
    fallback_path()
    {
      static std::string const fb_path = path::join(common::infinit::home(),
                                                    "analytics.fallback");
      return fb_path;
    }

    Info const&
    google_info()
    {
      static Info google{"www.google-analytics.com",
                         80,
                         path::join(common::infinit::home(), "ga.id")};

      return google;
    }

    Info const&
    km_info()
    {
      static Info km{"trk.kissmetrics.com",
                     80,
                     path::join(common::infinit::home(), "km.id")};

      return km;
    }
  }

  namespace longinus
  {
    std::string
    host()
    {
      static std::string const host_string = elle::os::getenv(
        "INFINIT_LONGINUS_HOST",
        COMMON_LONGINUS_HOST
      );
      return host_string;
    }

    int
    port()
    {
      static std::string const port_string = elle::os::getenv(
        "INFINIT_LONGINUS_PORT",
        std::to_string(COMMON_LONGINUS_PORT)
      );
      return std::stoi(port_string);
    }
  }
}
