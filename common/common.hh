#ifndef  COMMON_COMMON_HH
# define COMMON_COMMON_HH

# include <string>

# define COMMON_DEFAULT_INFINIT_HOME ".infinit"
# define COMMON_DEFAULT_META_PROTOCOL "http"
# define COMMON_DEFAULT_META_HOST "meta.api.development.infinit.io"
# define COMMON_DEFAULT_META_PORT 12345
# define COMMON_DEFAULT_TROPHONIUS_PROTOCOL "http"
# define COMMON_DEFAULT_TROPHONIUS_HOST "trophonius.api.development.infinit.io"
# define COMMON_DEFAULT_TROPHONIUS_PORT 23456
# define COMMON_DEFAULT_RESOURCES_ROOT_URL "http://download.development.infinit.io"
# define COMMON_DEFAULT_LONGINUS_HOST "longinus.api.development.infinit.io"
# define COMMON_DEFAULT_LONGINUS_PORT 9999

# define COMMON_PRODUCTION_INFINIT_HOME ".infinit"
# define COMMON_PRODUCTION_META_PROTOCOL "http"
# define COMMON_PRODUCTION_META_HOST "v014.meta.api.production.infinit.io"
# define COMMON_PRODUCTION_META_PORT 12345
# define COMMON_PRODUCTION_TROPHONIUS_PROTOCOL "http"
# define COMMON_PRODUCTION_TROPHONIUS_HOST "v014.trophonius.api.production.infinit.io"
# define COMMON_PRODUCTION_TROPHONIUS_PORT 23456
# define COMMON_PRODUCTION_RESOURCES_ROOT_URL "http://download.production.infinit.io"
# define COMMON_PRODUCTION_LONGINUS_HOST "v014.longinus.api.production.infinit.io"
# define COMMON_PRODUCTION_LONGINUS_PORT 9999

namespace common
{

  /// All infinit related generic variables
  namespace infinit
  {

    /// Returns infinit home directory.
    /// Defaults to ~/.infinit on unices but could be changed by exporting
    /// INFINIT_HOME variable.
    std::string const&
    home();

    /// @brief Returns binary path from its name.
    ///
    /// if the environment variable INFINIT_BINARY_DIR is present, it will
    /// return ${INFINIT_BINARY_DIR}/name. Otherwise, if INFINIT_BUILD_DIR is
    /// set, it will return ${INFINIT_BUILD_DIR}/bin/binary_name.  If none of
    /// them has been set, it throws an exception.  Raises an exception when
    /// ensure is true and the path does not refer to a valid binary.
    std::string const&
    binary_path(std::string const& name,
                bool ensure = true);

    /// Returns the default path of every director.
    std::string
    networks_directory(std::string const& user_id);

    /// Returns network directory path.
    std::string
    network_directory(std::string const& user_id,
                      std::string const& network_id);

    /// Returns passport path.
    std::string
    passport_path(std::string const& user);

    /// Returns log file path.
    std::string
    log_path(std::string const& user_id,
             std::string const& network_id);

    // Returns the path to the shelter.
    std::string
    network_shelter(std::string const& user_id,
                    std::string const& network_id);

    /// Returns portal file path.
    std::string
    portal_path(std::string const& user_id,
                std::string const& network_id);

    /// Returns user directory path.
    std::string
    user_directory(std::string const& user_id);

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

  /// URLs to access meta server
  namespace meta
  {

    /// Returns the protocol used by meta (http or https)
    /// Can be overriden by INFINIT_META_PROTOCOL
    std::string const&
    protocol();

    /// Returns the host of the meta server.
    /// Can be overriden by INFINIT_META_HOST.
    std::string const&
    host();

    /// Returns the port of the meta server
    /// Can be overriden by INFINIT_META_PORT.
    uint16_t
    port();

    /// Returns the url to the meta server. If INFINIT_META_URL is defined, its
    /// value will be returned.
    std::string const&
    url();
  }

  namespace trophonius
  {
    /// Returns the protocol used by trophonius (http or https)
    /// Can be overriden by COMMON_INFINIT_TROPHONIUS_PROTOCOL
    std::string const&
    protocol();

    /// Returns the host of the trophonius server.
    /// Can be overriden by COMMON_INFINIT_TROPHONIUS_HOST.
    std::string const&
    host();

    /// Returns the port of the trophonius server
    /// Can be overriden by COMMON_INFINIT_TROPHONIUS_PORT.
    uint16_t
    port();

    /// Returns the url to the trophonius server. If INFINIT_META_URL is defined, its
    /// value will be returned.
    std::string const&
    url();
  }

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

  // XXX The watchdog use a local socket for now, but it will be redesigned
  namespace watchdog
  {

    /// The name of the local socket.
    std::string
    server_name(std::string const& user_id);

    /// The path to the lock file.
    std::string
    lock_path(std::string const& user_id);

    /// The path to the id file.
    std::string
    id_path(std::string const& user_id);

    /// The path to the log file.
    std::string
    log_path(std::string const& user_id);
  }

  namespace metrics
  {
    struct Info
    {
      std::string server;
      uint16_t port;
      std::string id_path;
    };

    Info const&
    google_info();

    Info const&
    km_info();

    /// Path to the file storing fallbacked metrics.
    std::string const&
    fallback_path();

  }

} // !common

#endif
