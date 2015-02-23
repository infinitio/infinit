#ifndef COMMON_COMMON_HH
# define COMMON_COMMON_HH

# include <memory>
# include <stdint.h>
# include <string>

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
      /// Create a configuration with the default download directory:
      /// HOME/Downloads.
      Configuration(bool production,
                    boost::filesystem::path home = boost::filesystem::path(),
                    boost::optional<std::string> download_dir =
                    boost::optional<std::string>{});
      ELLE_ATTRIBUTE_R(boost::filesystem::path, home);
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

    std::string
    infinit_default_home();

    // /// Returns infinit home directory.
    // /// Defaults to ~/.infinit on unices but could be changed by exporting
    // /// INFINIT_HOME variable.
    // std::string const&
    // home();

    /// Returns passport path.
    boost::filesystem::path
    passport_path(boost::filesystem::path const& home, std::string const& user);

    /// Returns the path of the file containing the computer device uuid.
    boost::filesystem::path
    device_id_path(boost::filesystem::path const& home);

    /// Return the path to the configuration file.
    boost::filesystem::path
    configuration_path(boost::filesystem::path const& home);

    /// Returns the path of the file showing that Infinit has been launched
    /// before.
    boost::filesystem::path
    first_launch_path(boost::filesystem::path const& home);

    /// Returns user directory path.
    boost::filesystem::path
    user_directory(boost::filesystem::path const& home,
                   std::string const& user_id);

    /// Returns user directory path.
    boost::filesystem::path
    transactions_directory(boost::filesystem::path const& home,
                           std::string const& user_id);

    /// Returns user diaries directory path.
    boost::filesystem::path
    transaction_snapshots_directory(boost::filesystem::path const& home,
                                    std::string const& user_id);

    /// Returns frete diary for a specific transaction.
    boost::filesystem::path
    frete_snapshot_path(boost::filesystem::path const& home,
                        std::string const& user_id,
                        std::string const& transaction_id);

    /// The path to the identity file.
    boost::filesystem::path
    identity_path(boost::filesystem::path const& home,
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

}

#endif
