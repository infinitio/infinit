#include "common.hh"

#include <elle/assert.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/print.hh>
#include <elle/system/platform.hh>
#include <elle/serialize/extract.hh>

// XXX.
#include <cryptography/PublicKey.hh>

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
#define COMMON_DEFAULT_META_REPOSITORY_CERTIFICATE "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"
#define COMMON_DEFAULT_META_K "AAAAAAEAAAAAAgAAvP1nLhoX9h/9fcKY363+nqambhqrhNOGLPin5iffdUUFqE5lnDtmGjDnLji0vnDS9W8IjzcUPevoyJRpSmy5kpcpOIcH8GcAbjeO7+9bwXQ11KONruR7o8RzI8fKDD4baaQQyORJw/8S7fQLlUhcWWuTUSLTEHBvYtw76supCt7WG4Yhb5bVQU80r0bxe66epDpoWHS8is2BM4e5+dmCTJ1ToyKXVzHgmo2F5h+DkfroKIcNe6lItN0v5zFnpQq87JgZt6mc4WkKiLGUWMm797wg0EbAttDXwsKJPjqsGboNOGLwwKRaP8S6zUHPPwpyTuhuz33ME6t5FsbNvoDNucrFmJN9Bm3uAK6kJ3yIwe98ybMkbDknKwzaOs9JdXND2qhi2tTMipGrDX+fxsNli67Y5d4ibExK1H8Pd3S29PyF8d4ASTI8HCRDjzkKNTJrpzoP4txEW9QGLtpO5TEngm3VCiGqiojHiLIL6NPuJR68gHsgj/yfomzipayQmZR2oo0s1BTM8lTuiWntWB0jkJfwjnO3+we8ZNp6kgj0i/0v8XbikjWZxARxQBt0Rk0ZjLaWPqU8+RYYlL2s1ltUhwSIyp8fjCQtuiA12B4WWMKldCp+aIzhDN8GsCQDxNKprqdWpQtpGrfMYXHnhxjV7riuwv9UHsWb4gGy0GL5YHMAAAMAAAABAAE="
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
#define COMMON_PRODUCTION_META_REPOSITORY_CERTIFICATE COMMON_DEFAULT_META_CERTIFICATE_REPOSITORY

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
# define COMMON_META_REPOSITORY_CERTIFICATE \
  BOOST_PP_CAT(VAR_PREFIX, _META_REPOSITORY_CERTIFICATE) \
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

    /*-----.
    | User |
    `-----*/

    std::string
    user_directory(std::string const& user,
                   std::string const& home)
    {
      return path::join(home, "users", user);
    }

    std::string
    passport_path(std::string const& user,
                  std::string const& home)
    {
      return path::join(infinit::user_directory(user, home), "passport");
    }

    std::string
    identity_path(std::string const& user,
                  std::string const& home)
    {
      return path::join(user_directory(user, home), "identity");
    }

    std::string
    tokpass_path(std::string const& user,
                 std::string const& home)
    {
      return path::join(user_directory(user, home), "tokpass");
    }

    /*--------.
    | Netowrk |
    `--------*/

    std::string
    networks_directory(std::string const& user,
                       std::string const& home)
    {
      return path::join(user_directory(user, home), "networks");
    }


    std::string
    network_directory(std::string const& user,
                      std::string const& network,
                      std::string const& home)
    {
      return path::join(
        networks_directory(user, home),
        network
      );
    }

    std::string
    descriptor_path(std::string const& user,
                    std::string const& network,
                    std::string const& home)
    {
      return path::join(
        network_directory(user, network, home),
        "descriptor");
    }

    std::string
    network_shelter(std::string const& user,
                    std::string const& network,
                    std::string const& home)
    {
      return path::join(
        network_directory(user, network, home), "shelter");
    }

    std::string
    portal_path(std::string const& user,
                std::string const& network,
                std::string const& home)
    {
      return path::join(
        network_directory(user, network, home), "portal");
    }

    std::string
    log_path(std::string const& user,
             std::string const& network,
             std::string const& home)
    {
      return path::join(
        networks_directory(user, home), network + ".log");
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
    repository_certificate()
    {
      static ::infinit::Certificate certificate(
        elle::serialize::from_string<
          elle::serialize::InputBase64Archive>(
            COMMON_META_REPOSITORY_CERTIFICATE));

      return (certificate);
    }

    ::infinit::cryptography::PublicKey const&
    K()
    {
      static ::infinit::cryptography::PublicKey K(
        elle::serialize::from_string<
         elle::serialize::InputBase64Archive>(COMMON_DEFAULT_META_K));
      return K;
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
