#ifndef  COMMON_COMMON_HH
# define COMMON_COMMON_HH

# include <memory>
# include <stdint.h>
# include <string>

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
      Configuration(bool production);

      ELLE_ATTRIBUTE_R(bool, production);

      /// Meta configuration
      ELLE_ATTRIBUTE_R(std::string, meta_protocol);
      ELLE_ATTRIBUTE_R(std::string, meta_host);
      ELLE_ATTRIBUTE_R(int, meta_port);

      /// Trophonius configuration
      ELLE_ATTRIBUTE_R(std::string, trophonius_host);
      ELLE_ATTRIBUTE_R(int, trophonius_port);

      /// Metrics configuration
      ELLE_ATTRIBUTE_R(bool, metrics_infinit_enabled);
      ELLE_ATTRIBUTE_R(std::string, metrics_infinit_host);
      ELLE_ATTRIBUTE_R(int, metrics_infinit_port);
      ELLE_ATTRIBUTE_R(bool, metrics_keen_enabled);
      ELLE_ATTRIBUTE_R(std::string, metrics_keen_project);
      ELLE_ATTRIBUTE_R(std::string, metrics_keen_key);
    };

    /// Returns infinit home directory.
    /// Defaults to ~/.infinit on unices but could be changed by exporting
    /// INFINIT_HOME variable.
    std::string const&
    home();

    /// Returns passport path.
    std::string
    passport_path(std::string const& user);

    /// Returns the path of the file containing the computer device uuid.
    std::string
    device_id_path();

    /// Returns user directory path.
    std::string
    user_directory(std::string const& user_id);

    /// Returns user directory path.
    std::string
    transactions_directory(std::string const& user_id);

    /// Returns user diaries directory path.
    std::string
    transaction_snapshots_directory(std::string const& user_id);

    /// Returns frete diary for a specific transaction.
    std::string
    frete_snapshot_path(std::string const& user_id,
                        std::string const& transaction_id);

    /// The path to the identity file.
    std::string
    identity_path(std::string const& user_id);
  }

  /// System and Operating System related stuffs
  namespace system
  {

    /// Returns the user home directory.
    std::string const&
    home_directory();

    /// Returns download directory at {HOME}/Downloads if exists else home directory.
    std::string const&
    download_directory();

    /// Returns the platform name (linux, macosx or windows)
    std::string const&
    platform();

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
