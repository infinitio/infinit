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

namespace path = elle::os::path;

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

  static
  void
  _set_path_with_optional(std::string& path_to_set,
                          std::string const& env_option,
                          std::string const& fallback,
                          boost::optional<std::string> optional = {})
  {
    std::string res;
    if (!env_option.empty())
    {
      res = env_option;
      if (res.empty() ||
          !boost::filesystem::exists(res) ||
          !boost::filesystem::is_directory(res))
      {
        ELLE_ABORT("unable to set path with env variable: %s", res);
      }
    }
    else
    {
      res = (!optional || optional.get().empty())
        ? fallback
        : optional.get();
      if (res.empty() ||
          !boost::filesystem::exists(res) ||
          !boost::filesystem::is_directory(res))
      {
        ELLE_ABORT("unable to set path (%s) or fallback (%s)", res, fallback);
      }
    }
    path_to_set = res;
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
    Configuration::Configuration(
      bool production,
      bool enable_mirroring,
      boost::optional<std::string> download_dir,
      boost::optional<std::string> persistent_config_dir,
      boost::optional<std::string> non_persistent_config_dir)
    {
      // File mirroring.
      bool mirror_enable =
        !::elle::os::getenv("INFINIT_ENABLE_MIRRORING", "").empty();
      bool mirror_disable =
        !::elle::os::getenv("INFINIT_DISABLE_MIRRORING", "").empty();
      if (mirror_enable && mirror_disable)
      {
        ELLE_ABORT("define only one of INFINIT_ENABLE_MIRRORING and "\
                   "INFINIT_DISABLE_MIRRORING");
      }
      if (mirror_enable)
        _enable_mirroring = true;
      else if (mirror_disable)
        _enable_mirroring = false;
      else
        _enable_mirroring = enable_mirroring;
      // Download directory.
      _set_path_with_optional(
        this->_download_dir,
        elle::os::getenv("INFINIT_DOWNLOAD_DIR", ""),
        path::join(elle::system::home_directory().string(), "Downloads"),
        download_dir);

      if (!elle::os::getenv("INFINIT_HOME", "").empty())
      {
        boost::filesystem::path default_path(
          elle::os::getenv("INFINIT_HOME", ""));
        if (!boost::filesystem::exists(default_path))
        {
          boost::filesystem::create_directories(default_path);
        }
        else if (!boost::filesystem::is_directory(default_path))
        {
          ELLE_ABORT("unable to set path using INFINIT_HOME (%s) "\
                     "file exists and is not directory", default_path);
        }
        this->_persistent_config_dir = default_path.string();
        this->_non_persistent_config_dir = default_path.string();
      }
      else
      {
        // Persistent config directory.
        _set_path_with_optional(
          this->_persistent_config_dir,
          elle::os::getenv("INFINIT_PERSISTENT_DIR", ""),
          path::join(elle::system::home_directory().string(), ".infinit"),
          persistent_config_dir);

        // Non-persistent config directory.
        _set_path_with_optional(
          this->_non_persistent_config_dir,
          elle::os::getenv("INFINIT_NON_PERSISTENT_DIR", ""),
          path::join(elle::system::home_directory().string(), ".infinit"),
          non_persistent_config_dir);
      }

      bool env_production =
        !::elle::os::getenv("INFINIT_PRODUCTION", "").empty();
      bool env_development =
        !::elle::os::getenv("INFINIT_DEVELOPMENT", "").empty();
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
      this->_device_id = this->_get_device_id();

      // Trophonius
      this->_trophonius_fingerprint = default_trophonius_fingerprint;
    }

    Configuration::Configuration(
      std::string const& meta_protocol,
      std::string const& meta_host,
      uint16_t meta_port,
      std::vector<unsigned char> trophonius_fingerprint)
        : _enable_mirroring(true)
        , _meta_protocol(meta_protocol)
        , _meta_host(meta_host)
        , _meta_port(meta_port)
        , _trophonius_fingerprint(trophonius_fingerprint)
        , _metrics_infinit_enabled(false)
        , _metrics_infinit_host()
        , _metrics_infinit_port()
        , _persistent_config_dir()
        , _non_persistent_config_dir()
        , _download_dir()
    {
      this->_device_id = this->_get_device_id();

      // Download directory.
      _set_path_with_optional(
        this->_download_dir,
        elle::os::getenv("INFINIT_DOWNLOAD_DIR", ""),
        path::join(elle::system::home_directory().string(), "Downloads"));

      if (!elle::os::getenv("INFINIT_HOME", "").empty())
      {
        boost::filesystem::path default_path(
          elle::os::getenv("INFINIT_HOME", ""));
        if (!boost::filesystem::exists(default_path))
        {
          boost::filesystem::create_directories(default_path);
        }
        else if (!boost::filesystem::is_directory(default_path))
        {
          ELLE_ABORT("unable to set path using INFINIT_HOME (%s) "\
                     "file exists and is not directory", default_path);
        }
        this->_persistent_config_dir = default_path.string();
        this->_non_persistent_config_dir = default_path.string();
      }
      else
      {
        // Persistent config directory.
        _set_path_with_optional(
          this->_persistent_config_dir,
          elle::os::getenv("INFINIT_PERSISTENT_DIR", ""),
          path::join(elle::system::home_directory().string(), ".infinit"));

        // Non-persistent config directory.
        _set_path_with_optional(
          this->_non_persistent_config_dir,
          elle::os::getenv("INFINIT_NON_PERSISTENT_DIR", ""),
          path::join(elle::system::home_directory().string(), ".infinit"));
      }
    }

    boost::uuids::uuid
    Configuration::_get_device_id()
    {
      auto device_uuid = boost::uuids::nil_generator()();
      bool force_regenerate
        = !elle::os::getenv("INFINIT_FORCE_NEW_DEVICE_ID", "").empty();
      if (!force_regenerate
          && boost::filesystem::exists(this->device_id_path()))
      {
        ELLE_TRACE("%s: get device uuid from file", *this);
        boost::filesystem::ifstream file(this->device_id_path());
        std::string struuid;
        file >> struuid;
        device_uuid = boost::uuids::string_generator()(struuid);
      }
      else
      {
        ELLE_TRACE("%s: create device uuid", *this);
        boost::filesystem::create_directories(
          boost::filesystem::path(this->device_id_path())
          .parent_path());
        device_uuid = boost::uuids::random_generator()();
        std::ofstream file(this->device_id_path());
        if (!file.good())
          ELLE_ERR("%s: Failed to create device uuid file at %s", *this,
                   this->device_id_path());
        file << device_uuid << std::endl;
      }
      return device_uuid;
    }

    std::string
    Configuration::passport_path() const
    {
      return path::join(this->persistent_config_dir(), "passport");
    }

    std::string
    Configuration::device_id_path() const
    {
      return path::join(this->persistent_config_dir(), "device.uuid");
    }

    std::string
    Configuration::configuration_path() const
    {
      return path::join(this->non_persistent_config_dir(), "configuration");
    }

    std::string
    Configuration::first_launch_path() const
    {
      return path::join(this->persistent_config_dir(), "first_launch");
    }

    std::string
    Configuration::persistent_user_directory(std::string const& user_id) const
    {
      auto users_path = path::join(this->persistent_config_dir(), "users");
      return path::join(users_path, user_id);
    }

    std::string
    Configuration::non_persistent_user_directory(std::string const& user_id) const
    {
      auto users_path = path::join(this->non_persistent_config_dir(), "users");
      return path::join(users_path, user_id);
    }

    std::string
    Configuration::transactions_directory(std::string const& user_id) const
    {
      return path::join(this->non_persistent_user_directory(user_id),
                        "transactions");
    }

    std::string
    Configuration::identity_path(std::string const& user_id) const
    {
      return path::join(this->persistent_user_directory(user_id),
                        "identity");
    }

    std::unique_ptr< ::infinit::metrics::Reporter>
    Configuration::metrics() const
    {
      using namespace ::infinit::metrics;
      auto res = elle::make_unique<CompositeReporter>();
      if (this->metrics_infinit_enabled())
        res->add_reporter(elle::make_unique<InfinitReporter>(
                            this->metrics_infinit_host(),
                            this->metrics_infinit_port()));
      return std::unique_ptr<Reporter>(
        std::move(res));
    }
  }
}
