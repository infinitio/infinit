#include <sys/types.h>
#include <unistd.h>

#include <stdexcept>
#include <unordered_map>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <elle/assert.hh>
#include <elle/os/environ.hh>
#include <elle/os/path.hh>
#include <elle/print.hh>
#include <elle/system/home_directory.hh>
#include <elle/system/platform.hh>

#include <infinit/metrics/CompositeReporter.hh>
#include <infinit/metrics/reporters/InfinitReporter.hh>
#include <infinit/metrics/Reporter.hh>

#include <common/common.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("common")

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
      static bool no_cache = !elle::os::getenv("INFINIT_NO_DIR_CACHE", "").empty();
      if (no_cache)
        infinit_dir = _infinit_home();
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
    configuration_path()
    {
      return path::join(home(), "configuration");
    }

    std::string
    first_launch_path()
    {
      return path::join(home(), "first_launch");
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
      static bool no_cache = !elle::os::getenv("INFINIT_NO_DIR_CACHE", "").empty();
      if (no_cache)
        home_dir = _home_directory();
      return home_dir;
    }

    unsigned int
    architecture()
    {
      return sizeof(void*) * 8;
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
      res = "preprod.meta.production.infinit.io";
    }
    return res;
  }
}

static const std::vector<unsigned char> default_trophonius_fingerprint =
{
  0xCB, 0xC5, 0x12, 0xBB, 0x86, 0x4D, 0x6B, 0x1C, 0xBC, 0x02,
  0x3D, 0xD8, 0x44, 0x75, 0xC1, 0x8C, 0x6E, 0xfC, 0x3B, 0x65
};

common::infinit::Configuration::Configuration(bool production,
                                              boost::optional<std::string> download_dir)
{
  std::string download_directory = (!download_dir || download_dir.get().empty())
    ? path::join(elle::system::home_directory().string(), "Downloads")
    : download_dir.get();

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

  // Metrics
  this->_metrics_infinit_enabled = boost::lexical_cast<bool>(
    elle::os::getenv("INFINIT_METRICS_INFINIT", env_production ? "1" : "0"));
  this->_metrics_infinit_host =
    elle::os::getenv("INFINIT_METRICS_INFINIT_HOST",
                     _server_host_name("metrics", env_production));
  this->_metrics_infinit_port = boost::lexical_cast<int>(
    elle::os::getenv(
      "INFINIT_METRICS_INFINIT_PORT", "80"));

  // Device
  auto device_uuid = boost::uuids::nil_generator()();
  bool force_regenerate
    = !elle::os::getenv("INFINIT_FORCE_NEW_DEVICE_ID", "").empty();
  if (!force_regenerate
      && boost::filesystem::exists(common::infinit::device_id_path()))
  {
    ELLE_TRACE("%s: get device uuid from file", *this);
    boost::filesystem::ifstream file(common::infinit::device_id_path());
    std::string struuid;
    file >> struuid;
    device_uuid = boost::uuids::string_generator()(struuid);
  }
  else
  {
    ELLE_TRACE("%s: create device uuid", *this);
    boost::filesystem::create_directories(
      boost::filesystem::path(common::infinit::device_id_path())
      .parent_path());
    device_uuid = boost::uuids::random_generator()();
    std::ofstream file(common::infinit::device_id_path());
    if (!file.good())
      ELLE_ERR("%s: Failed to create device uuid file at %s", *this,
               common::infinit::device_id_path());
    file << device_uuid << std::endl;
  }
  this->_device_id = device_uuid;

  // Trophonius
  this->_trophonius_fingerprint = default_trophonius_fingerprint;

  // Download directory
  this->_download_dir = elle::os::getenv("INFINIT_DOWNLOAD_DIR",
                                         download_directory);
    if (this->download_dir().length() > 0 &&
        boost::filesystem::exists(this->download_dir()) &&
        boost::filesystem::is_directory(this->download_dir()))
    {
      ELLE_TRACE("%s: set download directory: %s", *this, this->download_dir());
    }
    else
    {
      ELLE_ERR("%s: failed to set download directory: %s",
                 *this, this->download_dir());
    }
}

std::unique_ptr< ::infinit::metrics::Reporter>
common::metrics(common::infinit::Configuration const& config)
{
  auto res = elle::make_unique< ::infinit::metrics::CompositeReporter>();
  if (config.metrics_infinit_enabled())
    res->add_reporter(elle::make_unique< ::infinit::metrics::InfinitReporter>(
                        config.metrics_infinit_host(),
                        config.metrics_infinit_port()));
  return std::unique_ptr< ::infinit::metrics::Reporter>(
    std::move(res));
}
