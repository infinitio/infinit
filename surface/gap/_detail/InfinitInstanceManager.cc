#include "InfinitInstanceManager.hh"

#include <common/common.hh>

#include <elle/Exception.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/os/getenv.hh>
#include <elle/system/signal.hh>

#include <signal.h>

#include <stdlib.h>

ELLE_LOG_COMPONENT("infinit.surface.gap.InfinitInstanceManager");

namespace surface
{
  namespace gap
  {

    InfinitInstanceManager::InfinitInstanceManager(std::string const& user_id)
      : _user_id{user_id}
    {}

    void
    InfinitInstanceManager::launch_network(std::string const& network_id)
    {
      ELLE_TRACE_FUNCTION(network_id);
      if (_instances.find(network_id) != _instances.end())
      {
        ELLE_ASSERT(_instances[network_id] != nullptr);
        if (_instances[network_id]->process->running())
          throw elle::Exception{"Network " + network_id + " already launched"};
        else
          ELLE_DEBUG("Found staled infinit instance (%s): status = %s",
                     _instances[network_id]->process->id(),
                     _instances[network_id]->process->status());
      }
      auto pc = elle::system::process_config(elle::system::normal_config);
      {
        std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");

        if (!log_file.empty())
        {
          if (elle::os::in_env("INFINIT_LOG_FILE_PID"))
          {
            log_file += ".";
            log_file += std::to_string(::getpid());
          }
          log_file += ".infinit.log";

          pc.setenv("INFINIT_LOG_FILE", log_file);
        }
      }
      ELLE_DEBUG("%s %s %s %s %s", common::infinit::binary_path("8infinit"),
                 "-n", network_id, "-u",
                 _user_id);
      auto process = elle::make_unique<elle::system::Process>(
        std::move(pc),
        common::infinit::binary_path("8infinit"),
        std::list<std::string>{"-n", network_id, "-u", _user_id}
      );

      _instances[network_id].reset(new InfinitInstance{
          network_id,
          "",
          std::move(process),
      });
    }

    void
    InfinitInstanceManager::stop_network(std::string const& network_id)
    {
      ELLE_TRACE("stopping network");

      try
      {
        auto const& instance = this->network_instance(network_id);
        instance.process->interrupt();
        instance.process->wait();
      }
      catch (elle::Exception const& e)
      {
        ELLE_DEBUG("no network found, no infinit to kill");
      }

      auto it_proc = this->_instances.find(network_id);
      if (it_proc != this->_instances.end())
      {
        auto& proc = it_proc->second->process;

        if (proc->running())
        {
          proc->interrupt();
          proc->wait();
        }
        else
        {
          if (proc->status() != 0)
          {
            elle::system::Process::StatusCode status_code = proc->status();
            if (status_code < 0)
            {
              if (-status_code == SIGINT)
                ELLE_LOG("8infinit stopped with signal %s (%s)",
                         -status_code,
                         elle::system::strsignal(-status_code));
              else
                ELLE_ERR("8infinit stopped with signal %s (%s)",
                         -status_code,
                         elle::system::strsignal(-status_code));
            }
            else
              ELLE_ERR("8infinit exited with status %s", status_code);
          }
        }
        _instances.erase(network_id);
      }
    }

    bool
    InfinitInstanceManager::has_network(std::string const& network_id) const
    {
      if (this->_instances.find(network_id) == this->_instances.end())
        return false;

      if (!this->network_instance(network_id).process->running())
      {
        ELLE_WARN("Found not running infinit instance (pid = %s): status = %s",
                   this->network_instance(network_id).process->id(),
                   this->network_instance(network_id).process->status());
        //XXX this->_instances.erase(network_id);
        return false;
      }
      return true;
    }

    InfinitInstance const&
    InfinitInstanceManager::network_instance(std::string const& network_id) const
    {
      auto it = _instances.find(network_id);
      if (it == _instances.end())
        throw elle::Exception{"Cannot find any network " + network_id};
      ELLE_ASSERT(it->second != nullptr);
      return *(it->second);
    }

    InfinitInstance const*
    InfinitInstanceManager::network_instance_for_file(std::string const& path)
    {
      for (auto const& pair : _instances)
        {
          std::string mount_point = pair.second->mount_point;

          if (path.compare(0, mount_point.length(), path) != 0)
            return pair.second.get();
        }
      return nullptr;
    }

  }
}
