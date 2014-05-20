#include <sys/types.h>
#include <unistd.h>

#include <stdexcept>
#include <unordered_map>

#include <boost/preprocessor/cat.hpp>

#include <elle/assert.hh>
#include <elle/os/environ.hh>
#include <elle/os/path.hh>
#include <elle/print.hh>
#include <elle/system/home_directory.hh>
#include <elle/system/platform.hh>

#include <infinit/metrics/CompositeReporter.hh>
#include <infinit/metrics/reporters/InfinitReporter.hh>
#include <infinit/metrics/reporters/KeenReporter.hh>
#include <infinit/metrics/Reporter.hh>

#include <common/common.hh>
#include <version.hh>

#define COMMON_DEFAULT_INFINIT_HOME ".infinit"

#define COMMON_PRODUCTION_INFINIT_HOME ".infinit"

#ifdef INFINIT_PRODUCTION_BUILD
# define VAR_PREFIX COMMON_PRODUCTION
#else
# define VAR_PREFIX COMMON_DEFAULT
#endif

# define COMMON_INFINIT_HOME \
  BOOST_PP_CAT(VAR_PREFIX, _INFINIT_HOME) \
/**/

namespace path = elle::os::path;

namespace
{

  std::string
  _home_directory()
  {
    return elle::system::home_directory().string();
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

    std::string
    passport_path(std::string const& user)
    {
      return path::join(infinit::user_directory(user), user + ".ppt");
    }

    /// Returns the path of the file containing the computer device uuid.
    std::string
    device_id_path()
    {
      return path::join(home(), "device.uuid");
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
      static bool no_cache = !elle::os::getenv("INFINIT_NO_DIR_CACHE", "").empty();
      if (no_cache)
        download_dir = _download_directory();
      return download_dir;
    }

  } //!system
}

namespace
{
  static
  std::string
  _server_host_name(std::string const& server_name, bool production)
  {
    std::string res;
    if (production)
    {
      res = elle::sprintf(
        "%s.%s.%s.api.production.infinit.io",
        server_name,
        INFINIT_VERSION_MINOR,
        INFINIT_VERSION_MAJOR
      );
    }
    else
    {
      res = "development.infinit.io";
    }
    return res;
  }
}

common::infinit::Configuration::Configuration(bool production)
{
  bool env_production = !::elle::os::getenv("INFINIT_PRODUCTION", "").empty();
  bool env_development = !::elle::os::getenv("INFINIT_DEVELOPMENT", "").empty();
  if (env_production && env_development)
  {
    ELLE_ABORT("define one and only one of "
               "INFINIT_PRODUCTION and INFINIT_DEVELOPMENT");
  }
  else if (!env_production && !env_development)
  {
    if (production)
      env_production = true;
    else
      env_development = true;
  }
  else if (env_development)
  {
    env_production = false;
  }

  // Meta
  this->_meta_protocol = elle::os::getenv("INFINIT_META_PROTOCOL", "https");
  this->_meta_host = elle::os::getenv(
    "INFINIT_META_HOST", _server_host_name("meta", env_production));
  this->_meta_port = boost::lexical_cast<int>(
    elle::os::getenv("INFINIT_META_PORT", "443"));

  // Trophonius
  this->_trophonius_host = elle::os::getenv(
    "INFINIT_TROPHONIUS_HOST",
    _server_host_name("trophonius", env_production));
  this->_trophonius_port = boost::lexical_cast<int>(
    elle::os::getenv("INFINIT_TROPHONIUS_PORT",
                     env_production ? "443" : "444"));

  // Metrics
  this->_metrics_infinit_enabled = boost::lexical_cast<bool>(
    elle::os::getenv("INFINIT_METRICS_INFINIT", env_production ? "1" : "0"));
  this->_metrics_infinit_host =
    elle::os::getenv("INFINIT_METRICS_INFINIT_HOST",
                     _server_host_name("metrics", env_production));
  this->_metrics_infinit_port = boost::lexical_cast<int>(
    elle::os::getenv(
      "INFINIT_METRICS_INFINIT_PORT", "80"));
  this->_metrics_keen_enabled = boost::lexical_cast<bool>(
    elle::os::getenv("INFINIT_METRICS_KEEN", "1"));
  this->_metrics_keen_project =
    elle::os::getenv(
      "INFINIT_METRICS_KEEN_PROJECT", env_production ?
      "532c5a9c00111c0da2000023" :
      "53307f5ace5e436303000014");
  this->_metrics_keen_key =
    elle::os::getenv(
      "INFINIT_METRICS_KEEN_KEY", env_production ?
      "19562aa3aed59df3f0a0bb746975d4b61a1789b52b6ee42ffcdd88fbe9fec7bd6f8e6cf4256fee1a08a842edc8212b98b57d3c28b6df94fd1520834390d0796ad2efbf59ee1fca268bdc4c6d03fa438102ae22c7c6e318d98fbe07becfb83ec65b2e844c57bb3db2da1d36903c4ef791" :
      "d9440867211d34efa94b2dc72673c46b02d3110dbc3271ee83fec6fd97d9be1839a3c02a913cd7091ee310e93c62f95799679ee4ec66707d8742c3649dd756ae32c69828778b2a77ea39121f0d407a49577553c71ad87fd3c38bdf1e9322201e0155fdc21269c6c47834e9907470204f");
}

std::unique_ptr< ::infinit::metrics::Reporter>
common::metrics(common::infinit::Configuration const& config)
{
  auto res = elle::make_unique< ::infinit::metrics::CompositeReporter>();
  if (config.metrics_infinit_enabled())
    res->add_reporter(elle::make_unique< ::infinit::metrics::InfinitReporter>(
                        config.metrics_infinit_host(),
                        config.metrics_infinit_port()));
  if (config.metrics_keen_enabled())
    res->add_reporter(elle::make_unique< ::infinit::metrics::KeenReporter>(
                        config.metrics_keen_project(),
                        config.metrics_keen_key()));
  return std::unique_ptr< ::infinit::metrics::Reporter>(
    std::move(res));
}
