#ifndef  COMMON_COMMON_HH
# define COMMON_COMMON_HH

# include <memory>
# include <stdint.h>
# include <string>

# include <boost/uuid/uuid.hpp>
# include <boost/optional.hpp>

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
      /// Create a configuration with the default download directory:
      /// HOME/Downloads.
      Configuration(bool production,
                    boost::optional<std::string> download_dir =
                      boost::optional<std::string>{});

      ELLE_ATTRIBUTE_R(bool, production);

      /// Meta configuration
      ELLE_ATTRIBUTE_R(std::string, meta_protocol);
      ELLE_ATTRIBUTE_R(std::string, meta_host);
      ELLE_ATTRIBUTE_R(int, meta_port);

      /// Trophonius configuration
      ELLE_ATTRIBUTE_R(std::vector<unsigned char>, trophonius_fingerprint);

      /// Metrics configuration
      ELLE_ATTRIBUTE_R(bool, metrics_infinit_enabled);
      ELLE_ATTRIBUTE_R(std::string, metrics_infinit_host);
      ELLE_ATTRIBUTE_R(int, metrics_infinit_port);

      /// Device configuration
      ELLE_ATTRIBUTE_R(boost::uuids::uuid, device_id);

      /// Download directory
      ELLE_ATTRIBUTE_R(std::string, download_dir);
    };

    /// Returns infinit home directory.
    /// Defaults to ~/.infinit on unices but could be changed by exporting
    /// INFINIT_HOME variable.
    std::string const&
    home();

    /// Returns passport path.
    std::string
    passport_path(std::string const& user);

    /// Returns passport path.
    std::string
    passport_path(std::string const& home,
                  std::string const& user);

    /// Returns the path of the file containing the computer device uuid.
    std::string
    device_id_path();

    /// Returns the path of the file containing the computer device uuid.
    std::string
    device_id_path(std::string const& home);

    /// Return the path to the configuration file.
    std::string
    configuration_path();

    /// Return the path to the configuration file.
    std::string
    configuration_path(std::string const& home);

    /// Returns the path of the file showing that Infinit has been launched
    /// before.
    std::string
    first_launch_path();

    /// Returns the path of the file showing that Infinit has been launched
    /// before.
    std::string
    first_launch_path(std::string const& home);

    /// Returns user directory path.
    std::string
    user_directory(std::string const& user_id);

    /// Returns user directory path.
    std::string
    user_directory(std::string const& home,
                   std::string const& user_id);

    /// Returns user directory path.
    std::string
    transactions_directory(std::string const& user_id);

    /// Returns user directory path.
    std::string
    transactions_directory(std::string const& home,
                           std::string const& user_id);

    /// Returns user diaries directory path.
    std::string
    transaction_snapshots_directory(std::string const& user_id);

    /// Returns user diaries directory path.
    std::string
    transaction_snapshots_directory(std::string const& home,
                                    std::string const& user_id);

    /// Returns frete diary for a specific transaction.
    std::string
    frete_snapshot_path(std::string const& user_id,
                        std::string const& transaction_id);

    /// The path to the identity file.
    std::string
    identity_path(std::string const& user_id);

    /// The path to the identity file.
    std::string
    identity_path(std::string const& home,
                  std::string const& user_id);
  }

  /// System and Operating System related stuffs
  namespace system
  {

    /// Returns the user home directory.
    std::string const&
    home_directory();

    /// Returns the architecture in bits.
    unsigned int
    architecture();

  }

  std::unique_ptr< ::infinit::metrics::Reporter>
  metrics(infinit::Configuration const& config);

  /// All resources URIs
  namespace resources
  {

    /// Returns the root url for download infinit resources.  It will use
    /// current platform and architecture when not provided. If
    /// INFINIT_RESOURCES_ROOT_URL is defined, it will be used to build base
    /// url.
    std::string
    base_url(char const* platform = nullptr,
             unsigned int architecture = 0);

    /// Returns the manifest url (Behave as resources_url()).
    std::string
    manifest_url(char const* platform = nullptr,
                 unsigned int architecture = 0);

  }

} // !common

#endif
