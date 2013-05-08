#include "InfinitInstanceManager.hh"

#include <common/common.hh>

#include <elle/Exception.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
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

    InfinitInstanceManager::~InfinitInstanceManager()
    {
      this->clear();
    }

    void
    InfinitInstanceManager::clear()
    {
      auto it = begin(this->_instances);

      while(it != end(this->_instances))
      {
        this->stop(it->first);
        it = begin(this->_instances);
      }

      ELLE_ASSERT(this->_instances.empty());
    }

    void
    InfinitInstanceManager::wait_portal(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);

      ELLE_DEBUG("retrieving portal path");
      auto portal_path = common::infinit::portal_path(this->_user_id, network_id);

      ELLE_DEBUG("portal path is %s", portal_path);
      if (!this->exists(network_id))
        this->launch(network_id);

      for (int i = 0; i < 45; ++i)
      {
        ELLE_DEBUG("Waiting for portal.");
        if (elle::os::path::exists(portal_path))
          return;

        if (!this->exists(network_id))
          throw Exception{gap_error, "Infinit instance has crashed"};
        ::sleep(1);
      }

      throw Exception{
        elle::sprintf("Unable to find portal for %s", network_id)};
    }


    void
    InfinitInstanceManager::launch(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);
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

      std::string command;
      std::list<std::string> args;

      auto pc = elle::system::process_config(elle::system::normal_config);
      if (elle::os::getenv("INFINIT_DEBUG_WITH_VALGRIND", "") == "1")
      {
        pc.pipe_file(
            elle::system::ProcessChannelStream::out,
            "/tmp/infinit.out", "a+"
        );
        pc.pipe_file(
            elle::system::ProcessChannelStream::err,
            "/tmp/infinit.out", "a+"
        );
        command = "/opt/local/bin/valgrind";
        args = {
          "--dsymutil=yes", "--max-stackframe=751633440",
          common::infinit::binary_path("8infinit"),
          "-n", network_id,
          "-u", _user_id
        };
      }
      else
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
        command = common::infinit::binary_path("8infinit");
        args = {
          "-n", network_id,
          "-u", _user_id
        };
      }
      ELLE_DEBUG("%s %s %s %s %s", common::infinit::binary_path("8infinit"),
                 "-n", network_id, "-u",
                 _user_id);
      auto process = elle::make_unique<elle::system::Process>(
        std::move(pc),
        command,
        args
      );

      _instances[network_id].reset(new InfinitInstance{
          network_id,
          "",
          std::move(process),
      });
    }

    void
    InfinitInstanceManager::stop(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);
      try
      {
        auto const& instance = this->instance(network_id);
        ELLE_ASSERT(instance.process != nullptr);
        instance.process->interrupt(elle::system::ProcessTermination::dont_wait);
      }
      catch (elle::Exception const& e)
      {
        ELLE_WARN("no network found, no infinit to kill");
      }

      auto it_proc = this->_instances.find(network_id);
      if (it_proc != this->_instances.end())
      {
        ELLE_ASSERT(it_proc->second != nullptr);
        auto& proc = it_proc->second->process;

        if (proc->running())
        {
          proc->interrupt(elle::system::ProcessTermination::dont_wait);
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
          else
          {
            ELLE_DEBUG("status 0");
          }
        }
        _instances.erase(network_id);
      }
    }

    bool
    InfinitInstanceManager::exists(std::string const& network_id) const
    {
      ELLE_TRACE_METHOD(network_id);
      if (this->_instances.find(network_id) == this->_instances.end())
        return false;

      if (!this->instance(network_id).process->running())
      {
        ELLE_WARN("Found not running infinit instance (pid = %s): status = %s",
                   this->instance(network_id).process->id(),
                   this->instance(network_id).process->status());
        //XXX this->_instances.erase(network_id);
        return false;
      }
      return true;
    }

    InfinitInstance const&
    InfinitInstanceManager::instance(std::string const& network_id) const
    {
      ELLE_TRACE_METHOD(network_id);
      auto it = _instances.find(network_id);
      if (it == _instances.end())
        throw elle::Exception{"Cannot find any network " + network_id};
      ELLE_ASSERT(it->second != nullptr);
      return *(it->second);
    }

    InfinitInstance const*
    InfinitInstanceManager::instance_for_file(std::string const& path)
    {
      ELLE_TRACE_METHOD(path);
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
