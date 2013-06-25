#include "binary_config.hh"

#include <common/common.hh>

#include <boost/filesystem.hpp>

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;

    static
    void
    _common_config(elle::system::ProcessConfig& config)
    {
      config.setenv("ELLE_LOG_PID", "1");
      config.setenv("ELLE_LOG_TID", "1");
      config.setenv("ELLE_LOG_TIME", "1");
      config.setenv("INFINIT_NO_FORK", "1");
      config.setenv("ELLE_LOG_LEVEL",
                    "TRACE,"
                    "elle*:LOG,"
                    "infinit.cryptography*:LOG,"
                    "infinit.protocol*:DEBUG,"
                    "reactor*:LOG,"
                    "reactor.network*:TRACE");
    }

    /// @brief Return a valid config process for an infinit binary.
    elle::system::ProcessConfig
    binary_config(std::string const& name,
                  std::string const& user_id,
                  std::string const& network_id)
    {
      using namespace elle::system;
      auto config = process_config(elle::system::normal_config);
      std::string path = (
        fs::path(common::infinit::network_directory(user_id, network_id)) /
        (name + ".log")).string();
      config.pipe_file(ProcessChannelStream::out, path);
      config.pipe_file(ProcessChannelStream::err, path);
      _common_config(config);
      return config;
    }

    elle::system::ProcessConfig
    binary_check_output_config(std::string const& name,
                               std::string const& user_id,
                               std::string const& network_id)
    {
      using namespace elle::system;
      auto config = process_config(elle::system::pipe_stdout_config);
      std::string path = (
        fs::path(common::infinit::network_directory(user_id, network_id)) /
        (name + ".log")).string();
      config.pipe_file(ProcessChannelStream::err, path);
      _common_config(config);
      return config;
    }
  }
}
