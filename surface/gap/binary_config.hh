#ifndef SURFACE_GAP_BINARY_CONFIG_HH
# define SURFACE_GAP_BINARY_CONFIG_HH

# include <elle/system/Process.hh>

namespace surface
{
  namespace gap
  {
    /// @brief Return a valid config process for an infinit binary.
    elle::system::ProcessConfig
    binary_config(std::string const& name,
                  std::string const& user_id,
                  std::string const& network_id);

    elle::system::ProcessConfig
    binary_check_output_config(std::string const& name,
                               std::string const& user_id,
                               std::string const& network_id);
  }
}

#endif
