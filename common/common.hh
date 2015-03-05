#ifndef COMMON_COMMON_HH
# define COMMON_COMMON_HH

# include <memory>
# include <stdint.h>
# include <string>
# include <vector>

# include <boost/filesystem/path.hpp>
# include <boost/optional.hpp>
# include <boost/uuid/uuid.hpp>

# include <elle/attribute.hh>

namespace infinit
{
  namespace metrics
  {
    class Reporter;
  }
}

namespace common
{
  /// All infinit related generic variables
  namespace infinit
  {
    /// Create configuration.
    class Configuration
    {
    public:
      Configuration(
        bool production,
        bool enable_mirroring = true,
        uint64_t max_mirror_size = 0,
        boost::optional<std::string> download_dir = {},
        boost::optional<std::string> persistent_config_dir = {},
        boost::optional<std::string> non_persistent_config_dir = {});
      Configuration(
        std::string const& meta_protocol,
        std::string const& meta_host,
        uint16_t meta_port,
        std::vector<unsigned char> trophonius_fingerprint,
        boost::optional<boost::uuids::uuid const&> device_id = {},
        boost::optional<std::string> download_dir = {},
        boost::optional<std::string> home_dir = {});
      Configuration() = default;

      ELLE_ATTRIBUTE_R(bool, production);
      ELLE_ATTRIBUTE_R(bool, enable_mirroring);
      ELLE_ATTRIBUTE_R(uint64_t, max_mirror_size);

      /// Meta configuration.
      ELLE_ATTRIBUTE_R(std::string, meta_protocol);
      ELLE_ATTRIBUTE_R(std::string, meta_host);
      ELLE_ATTRIBUTE_R(int, meta_port);

      /// Trophonius configuration.
      ELLE_ATTRIBUTE_R(std::vector<unsigned char>, trophonius_fingerprint);

      /// Metrics configuration.
      ELLE_ATTRIBUTE_R(bool, metrics_infinit_enabled);
      ELLE_ATTRIBUTE_R(std::string, metrics_infinit_host);
      ELLE_ATTRIBUTE_R(int, metrics_infinit_port);

      /// Device configuration.
      ELLE_ATTRIBUTE_R(boost::uuids::uuid, device_id);

      /// Directory configuration.
      /// The persistent storage is that which should be backed up if possible.
      /// An example of this would be where the device ID is stored.
      ELLE_ATTRIBUTE_R(std::string, persistent_config_dir);
      /// The non-persistent storage is that which should not be backed up.
      /// An example of this would be where the transaction snapshots are.
      ELLE_ATTRIBUTE_R(std::string, non_persistent_config_dir);
      /// The download directory is where files will be downloaded for the user.
      ELLE_ATTRIBUTE_R(std::string, download_dir);

      /// Returns passport path.
      std::string
      passport_path() const;

      /// Returns the path of the file containing the computer device uuid.
      std::string
      device_id_path() const;

      /// Return the path to the configuration file.
      std::string
      configuration_path() const;

      /// Returns the path of the file showing that Infinit has been launched
      /// before.
      std::string
      first_launch_path() const;

      /// Returns the persistent user directory path.
      std::string
      persistent_user_directory(std::string const& user_id) const;

      /// Returns the non-persistent user directory path.
      std::string
      non_persistent_user_directory(std::string const& user_id) const;

      /// Returns user directory path.
      std::string
      transactions_directory(std::string const& user_id) const;

      /// The path to the identity file.
      std::string
      identity_path(std::string const& user_id) const;

      std::unique_ptr< ::infinit::metrics::Reporter>
      metrics() const;

    private:
      boost::uuids::uuid
      _get_device_id();
    };
  }
}

#endif
