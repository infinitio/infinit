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
}

namespace common
{

  namespace infinit
  {
    boost::filesystem::path
    passport_path(boost::filesystem::path const& home,
                  std::string const& user)
    {
      return infinit::user_directory(home, user) / (user + ".ppt");
    }

    /// Returns the path of the file containing the computer device uuid.
    boost::filesystem::path
    device_id_path(boost::filesystem::path const& home)
    {
      return home / "device.uuid";
    }

    boost::filesystem::path
    configuration_path(boost::filesystem::path const& home)
    {
      return home / "configuration";
    }

    boost::filesystem::path
    first_launch_path(boost::filesystem::path const& home)
    {
      return home / "first_launch";
    }

    boost::filesystem::path
    user_directory(boost::filesystem::path const& home,
                   std::string const& user_id)
    {
      return home / "users" / user_id;
    }

    boost::filesystem::path
    transactions_directory(boost::filesystem::path const& home,
                           std::string const& user_id)
    {
      return user_directory(home, user_id) / "transaction";
    }

    boost::filesystem::path
    transaction_snapshots_directory(boost::filesystem::path const& home,
                                    std::string const& user_id)
    {
      return transactions_directory(home, user_id) / ".snapshot";
    }

    boost::filesystem::path
    frete_snapshot_path(boost::filesystem::path const& home,
                        std::string const& user_id,
                        std::string const& transaction_id)
    {
      return
        transactions_directory(home, user_id) / (transaction_id + ".frete");
    }

    boost::filesystem::path
    identity_path(boost::filesystem::path const& home,
                  std::string const& user_id)
    {
      return infinit::user_directory(home, user_id) / "identity";
    }
  }


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

namespace common
{
  namespace infinit
  {
    std::string
    infinit_default_home()
    {
      static bool first = true;
      static std::string res;
      static bool no_cache =
        !elle::os::getenv("INFINIT_NO_DIR_CACHE", "").empty();
      if (first || no_cache)
      {
        first = false;
        res = elle::os::getenv(
          "INFINIT_HOME",
          common::system::home_directory() + "/.infinit"
          );
      }
      return res;
    }

    common::infinit::Configuration::Configuration(
      bool production,
      boost::filesystem::path home,
      boost::optional<std::string> download_dir)
      : _home(std::move(home))
    {
      if (this->_home.empty())
        this->_home = infinit_default_home();
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
      auto device_id_path = common::infinit::device_id_path(home);
      auto device_uuid = boost::uuids::nil_generator()();
      bool force_regenerate
        = !elle::os::getenv("INFINIT_FORCE_NEW_DEVICE_ID", "").empty();
      if (!force_regenerate && boost::filesystem::exists(device_id_path))
      {
        ELLE_TRACE("%s: get device uuid from file", *this);
        boost::filesystem::ifstream file(device_id_path);
        std::string struuid;
        file >> struuid;
        device_uuid = boost::uuids::string_generator()(struuid);
      }
      else
      {
        ELLE_TRACE("%s: create device uuid", *this);
        boost::filesystem::create_directories(
          boost::filesystem::path(device_id_path).parent_path());
        device_uuid = boost::uuids::random_generator()();
        boost::filesystem::ofstream file(device_id_path);
        if (!file.good())
          ELLE_ERR("%s: Failed to create device uuid file at %s",
                   *this, device_id_path);
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
