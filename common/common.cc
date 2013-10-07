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

// XXX Metrics are always targetting the sandboxes.
#define COMMON_METRICS_INVESTORS_GOOGLE_TID "UA-31957100-3"

#define COMMON_METRICS_GOOGLE_TID "UA-31957100-5"

#define COMMON_METRICS_KISSMETRICS_TID \
   "0a79eca82697f0f7f0e6d5183daf8f1ebb81b39e"

#define COMMON_METRICS_KISSMETRICS_USER_TID \
   "63c77a385e9bf07550da9135b721963ac24d9e12"

#define COMMON_METRICS_KISSMETRICS_NETWORK_TID \
   "86b2aa67570c82874a96e73af61027496c8fb9e0"

#define COMMON_METRICS_KISSMETRICS_TRANSACTION_TID \
    "d953090a46a854e355d69335ae30c375498a05b6"

#define COMMON_METRICS_INFINIT_TID \
    ""

/// By default the token is empty to ensure that no metrics are sent.
/// See metrics/services/Mixpanel.cc
#define COMMON_METRICS_MIXPANEL_TRANSACTION_TID \
    ""

#define COMMON_DEFAULT_INFINIT_HOME ".infinit"
#define COMMON_DEFAULT_META_PROTOCOL "http"
#define COMMON_DEFAULT_META_HOST "meta.api.development.infinit.io"
#define COMMON_DEFAULT_META_PORT 80
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
#define COMMON_DEFAULT_APERTUS_HOST "apertus.api.development.infinit.io"
#define COMMON_DEFAULT_APERTUS_PORT 9899
#define COMMON_DEFAULT_METRICS_HOST "metrics.api.development.infinit.io"
#define COMMON_DEFAULT_METRICS_PORT 80

#define COMMON_PRODUCTION_INFINIT_HOME ".infinit"
#define COMMON_PRODUCTION_META_PROTOCOL "http"
#define COMMON_PRODUCTION_META_HOST "v2.meta.api.production.infinit.io"
#define COMMON_PRODUCTION_META_PORT 80
#define COMMON_PRODUCTION_TROPHONIUS_PROTOCOL "http"
#define COMMON_PRODUCTION_TROPHONIUS_HOST "v2.trophonius.api.production.infinit.io"
#define COMMON_PRODUCTION_TROPHONIUS_PORT 23456
#define COMMON_PRODUCTION_RESOURCES_ROOT_URL "http://download.production.infinit.io"
#define COMMON_PRODUCTION_LONGINUS_HOST "v2.longinus.api.production.infinit.io"
#define COMMON_PRODUCTION_LONGINUS_PORT 9999
#define COMMON_PRODUCTION_HEARTBEAT_HOST "heartbeat.production.infinit.io"
#define COMMON_PRODUCTION_HEARTBEAT_PORT 9898
#define COMMON_PRODUCTION_STUN_HOST "punch.api.development.infinit.io"
#define COMMON_PRODUCTION_STUN_PORT 3478
#define COMMON_PRODUCTION_APERTUS_HOST "apertus.api.production.infinit.io"
#define COMMON_PRODUCTION_APERTUS_PORT 9899
#define COMMON_PRODUCTION_METRICS_HOST "v2.metrics.api.production.infinit.io"
#define COMMON_PRODUCTION_METRICS_PORT 80

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
# define COMMON_APERTUS_HOST \
  BOOST_PP_CAT(VAR_PREFIX, _APERTUS_HOST) \
/**/
# define COMMON_APERTUS_PORT \
  BOOST_PP_CAT(VAR_PREFIX, _APERTUS_PORT) \
/**/
# define COMMON_METRICS_HOST \
  BOOST_PP_CAT(VAR_PREFIX, _METRICS_HOST) \
/**/
# define COMMON_METRICS_PORT \
  BOOST_PP_CAT(VAR_PREFIX, _METRICS_PORT) \
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
      {"8access",     "bin/8access"},
      {"8group",      "bin/8group"},
      {"8infinit",    "bin/8infinit"},
      {"8watchdog",   "bin/8watchdog"},
      {"8transfer",   "bin/8transfer"},
      {"8progress",   "bin/8progress"},
      {"gdbmacro.py", "bin/gdbmacro.py"},
      {"heartbeat-server", "bin/heartbeat-server"},
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
    transactions_directory(std::string const& user_id)
    {
      return path::join(user_directory(user_id), "transaction");
    }

    std::string
    transaction_snapshots_directory(std::string const& user_id)
    {
      return path::join(transactions_directory(user_id), ".snapshot");
    }

    std::string
    frete_snapshot_path(std::string const& user_id,
                        std::string const& transaction_id)
    {
      return path::join(transactions_directory(user_id), transaction_id + ".frete");
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
  }


  namespace metrics
  {
    std::string const&
    google_fallback_path()
    {
      static std::string const fb_path = path::join(
        common::infinit::home(), "google_analytics.fallback");
      return fb_path;
    }

    std::string const&
    mixpanel_fallback_path()
    {
      static std::string const fb_path = path::join(
        common::infinit::home(), "mixpanel_analytics.fallback");
      return fb_path;
    }

    std::string const&
    infinit_metrics_fallback_path()
    {
      static std::string const fb_path = path::join(
        common::infinit::home(), "infinit_analytics.fallback");
      return fb_path;
    }

    std::string const&
    fallback_path()
    {
      static std::string const fb_path = path::join(
        common::infinit::home(), "analytics.fallback");
      return fb_path;
    }

    ::metrics::Service::Info const&
    google_info_investors()
    {
      static ::metrics::Service::Info google = {
        "google",
        "www.google-analytics.com",
        80,
        path::join(common::infinit::home(), "ga.id"),
        elle::os::getenv("INFINIT_METRICS_INVESTORS_GOOGLE_TID",
                         COMMON_METRICS_INVESTORS_GOOGLE_TID),
      };
      return google;
    }

    ::metrics::Service::Info const&
    google_info(::metrics::Kind const kind)
    {
      static ::metrics::Service::Info google = {
        "google",
        "www.google-analytics.com",
        80,
        path::join(common::infinit::home(), "ga.id"),
        elle::os::getenv("INFINIT_METRICS_GOOGLE_TID",
                         COMMON_METRICS_GOOGLE_TID),
      };
      return google;
    }

    ::metrics::Service::Info const&
    kissmetrics_info(::metrics::Kind const kind)
    {
      static ::metrics::Service::Info const all = {
        "kissmetrics",
        "trk.kissmetrics.com",
        80,
        path::join(common::infinit::home(), "km.id"),
        elle::os::getenv("INFINIT_METRICS_KISSMETRICS_TID",
                         COMMON_METRICS_KISSMETRICS_TID),
      };
      static ::metrics::Service::Info const user = {
        "kissmetrics",
        "trk.kissmetrics.com",
        80,
        path::join(common::infinit::home(), "km.id"),
        elle::os::getenv("INFINIT_METRICS_KISSMETRICS_USER_TID",
                         COMMON_METRICS_KISSMETRICS_USER_TID),
      };
      static ::metrics::Service::Info const network = {
        "kissmetrics",
        "trk.kissmetrics.com",
        80,
        path::join(common::infinit::home(), "km.id"),
        elle::os::getenv("INFINIT_METRICS_KISSMETRICS_NETWORK_TID",
                         COMMON_METRICS_KISSMETRICS_NETWORK_TID),
      };
      static ::metrics::Service::Info const transaction = {
        "kissmetrics",
        "trk.kissmetrics.com",
        80,
        path::join(common::infinit::home(), "km.id"),
        elle::os::getenv("INFINIT_METRICS_KISSMETRICS_TRANSACTION_TID",
                         COMMON_METRICS_KISSMETRICS_TRANSACTION_TID),
      };
      switch (kind)
      {
        case ::metrics::Kind::all: return all;
        case ::metrics::Kind::user: return user;
        case ::metrics::Kind::network: return network;
        case ::metrics::Kind::transaction: return transaction;
      }
      elle::unreachable();
    }

    ::metrics::Service::Info const&
    mixpanel_info(::metrics::Kind const kind)
    {
      static ::metrics::Service::Info const transaction = {
        "mixpanel",
        "api.mixpanel.com",
        80,
        path::join(common::infinit::home(), "mp.id"),
        elle::os::getenv("INFINIT_METRICS_MIXPANEL_TRANSACTION_TID",
                         COMMON_METRICS_MIXPANEL_TRANSACTION_TID),
      };
      switch (kind)
      {
        case ::metrics::Kind::all: break;
        case ::metrics::Kind::network: break;
        case ::metrics::Kind::transaction: return transaction;
        case ::metrics::Kind::user: break;
      }
      elle::unreachable();
    }

    ::metrics::Service::Info const&
    infinit_metrics_info(::metrics::Kind const kind)
    {
      static ::metrics::Service::Info const transaction = {
        "infinit",
        elle::os::getenv("INFINIT_METRICS_HOST",
                         COMMON_METRICS_HOST),
        std::stoi(elle::os::getenv("INFINIT_METRICS_PORT",
                                   std::to_string(COMMON_METRICS_PORT))),
        path::join(common::infinit::home(), "im.id"),
        elle::os::getenv("INFINIT_METRICS_INFINIT_TID",
                         COMMON_METRICS_INFINIT_TID),
      };
      switch (kind)
      {
        case ::metrics::Kind::all: break;
        case ::metrics::Kind::network: break;
        case ::metrics::Kind::transaction: return transaction;
        case ::metrics::Kind::user: break;
      }
      elle::unreachable();
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

  namespace apertus
  {
    std::string
    host()
    {
      static std::string const host_string = elle::os::getenv(
        "INFINIT_APERTUS_HOST",
        COMMON_APERTUS_HOST
      );
      return host_string;
    }

    int
    port()
    {
      static std::string const port_string = elle::os::getenv(
        "INFINIT_APERTUS_PORT",
        std::to_string(COMMON_APERTUS_PORT)
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
