#include "common.hh"

#include <elle/assert.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/print.hh>
#include <elle/system/platform.hh>
#include <elle/serialize/extract.hh>

#include <infinit/Certificate.hh>

#include <boost/preprocessor/cat.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define COMMON_DEFAULT_INFINIT_HOME ".infinit"
#define COMMON_DEFAULT_META_PROTOCOL "http"
#define COMMON_DEFAULT_META_HOST "meta.api.development.infinit.io"
#define COMMON_DEFAULT_META_PORT 12345
#define COMMON_DEFAULT_META_CERTIFICATE "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"
#define COMMON_DEFAULT_TROPHONIUS_PROTOCOL "http"
#define COMMON_DEFAULT_TROPHONIUS_HOST "trophonius.api.development.infinit.io"
#define COMMON_DEFAULT_TROPHONIUS_PORT 23456
#define COMMON_DEFAULT_RESOURCES_ROOT_URL "http://download.development.infinit.io"
#define COMMON_DEFAULT_LONGINUS_HOST "longinus.api.development.infinit.io"
#define COMMON_DEFAULT_LONGINUS_PORT 9999
#define COMMON_DEFAULT_HEARTBEAT_HOST "heartbeat.development.infinit.io"
#define COMMON_DEFAULT_HEARTBEAT_PORT 9898
#define COMMON_DEFAULT_STUN_HOST "punch.api.development.infinit.io"
#define COMMON_DEFAULT_STUN_PORT 3478

#define COMMON_PRODUCTION_INFINIT_HOME ".infinit"
#define COMMON_PRODUCTION_META_PROTOCOL "http"
#define COMMON_PRODUCTION_META_HOST "v1.meta.api.production.infinit.io"
#define COMMON_PRODUCTION_META_PORT 12345
#define COMMON_PRODUCTION_META_CERTIFICATE COMMON_DEFAULT_META_CERTIFICATE

#define COMMON_PRODUCTION_TROPHONIUS_PROTOCOL "http"
#define COMMON_PRODUCTION_TROPHONIUS_HOST "v1.trophonius.api.production.infinit.io"
#define COMMON_PRODUCTION_TROPHONIUS_PORT 23456
#define COMMON_PRODUCTION_RESOURCES_ROOT_URL "http://download.production.infinit.io"
#define COMMON_PRODUCTION_LONGINUS_HOST "v1.longinus.api.production.infinit.io"
#define COMMON_PRODUCTION_LONGINUS_PORT 9999
#define COMMON_PRODUCTION_HEARTBEAT_HOST "heartbeat.production.infinit.io"
#define COMMON_PRODUCTION_HEARTBEAT_PORT 9898
#define COMMON_PRODUCTION_STUN_HOST "punch.api.development.infinit.io"
#define COMMON_PRODUCTION_STUN_PORT 3478

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
# define COMMON_META_CERTIFICATE \
  BOOST_PP_CAT(VAR_PREFIX, _META_CERTIFICATE) \
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
# define COMMON_HEARTBEAT_HOST \
  BOOST_PP_CAT(VAR_PREFIX, _HEARTBEAT_HOST) \
/**/
# define COMMON_HEARTBEAT_PORT \
  BOOST_PP_CAT(VAR_PREFIX, _HEARTBEAT_PORT) \
/**/
# define COMMON_STUN_HOST \
  BOOST_PP_CAT(VAR_PREFIX, _STUN_HOST) \
/**/
# define COMMON_STUN_PORT \
  BOOST_PP_CAT(VAR_PREFIX, _STUN_PORT) \
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
      {"gdbmacro.py",  "bin/gdbmacro.py"},
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
    descriptor_path(std::string const& user_id,
                    std::string const& network_id)
    {
      return path::join(
        network_directory(user_id, network_id),
        network_id + ".dsc");
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
      return path::join(user_directory(user_id), user_id + ".idy");
    }

    std::string
    tokpass_path(std::string const& user_id)
    {
      return path::join(user_directory(user_id), "tokpass");
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

    std::string const&
    token_path()
    {
      static std::string const token_path =
        elle::os::getenv("INFINIT_TOKEN_PATH", "");

      return token_path;
    }

    std::string
    token()
    {
      {
        static std::string const token = elle::os::getenv("INFINIT_TOKEN", "");
        if (!token.empty())
          return token;
      }

      std::string const& _token_path = token_path();
      std::string token;
      if (!_token_path.empty())
      {
        std::ifstream token_file{_token_path};
        std::getline(token_file, token);
      }
      return token;
    }

    ::infinit::Certificate const&
    certificate()
    {
      static ::infinit::Certificate certificate(
        elle::serialize::from_string<
          elle::serialize::InputBase64Archive>(COMMON_META_CERTIFICATE));

      return (certificate);
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
                         path::join(common::infinit::home(), "ga.id"),
                         elle::os::getenv("INFINIT_METRICS_GOOGLE_TID",
                                          "UA-31957100-3")};
      return google;
    }

    Info const&
    km_info()
    {
      static Info km{
        "trk.kissmetrics.com",
        80,
        path::join(common::infinit::home(), "km.id"),
        elle::os::getenv("INFINIT_METRICS_KISSMETRICS_TID",
                         "0a79eca82697f0f7f0e6d5183daf8f1ebb81b39e")};
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

  namespace heartbeat
  {
    std::string const&
    host()
    {
      static std::string const host_string = elle::os::getenv(
        "INFINIT_HEARTBEAT_HOST",
        COMMON_HEARTBEAT_HOST
      );
      return host_string;
    }

    int
    port()
    {
      static std::string const port_string = elle::os::getenv(
        "INFINIT_HEARTBEAT_PORT",
        std::to_string(COMMON_HEARTBEAT_PORT)
      );
      return std::stoi(port_string);
    }
  }

  namespace stun
  {
    std::string const&
    host()
    {
      static std::string const host_string = elle::os::getenv(
        "INFINIT_STUN_HOST",
        COMMON_STUN_HOST
      );
      return host_string;
    }

    int
    port()
    {
      static std::string const port_string = elle::os::getenv(
        "INFINIT_STUN_PORT",
        std::to_string(COMMON_STUN_PORT)
      );
      return std::stoi(port_string);
    }
  } /* stun */
}
