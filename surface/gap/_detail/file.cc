#include "../State.hh"

#include <common/common.hh>

#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/system/Process.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    namespace fs = boost::filesystem;

    FileInfos const&
    State::file_infos(std::string const& path)
    {
      std::string abspath = elle::os::path::absolute(path, true);
      ELLE_DEBUG("Get file infos of %s", abspath);
      auto it = this->_files_infos.find(abspath);
      if (it != this->_files_infos.end())
        return *(it->second);


      InfinitInstance const* instance =
        this->infinit_instance_manager().network_instance_for_file(abspath);

      if (instance == nullptr)
        throw Exception{
            gap_error, "Cannot find network for path '" + abspath + "'"
        };

      std::unique_ptr<FileInfos> infos;
      // Prepare infos ptr
      {
        std::string relpath;
        if (abspath == instance->mount_point)
          relpath = "";
        else
          relpath = abspath.substr(instance->mount_point.size() + 1);

        infos.reset(new FileInfos{
          instance->mount_point,
          instance->network_id,
          abspath,
          relpath,
          std::map<std::string, int>{},
        });
      }

      std::string const& access_binary = common::infinit::binary_path("8access");

      std::list<std::string> arguments{
                                    "--user",
                                    this->_me._id,
                                    "--type",
                                    "user",
                                    "--network",
                                    this->network(infos->network_id)._id,
                                    "--path",
                                    ("/" + infos->relative_path),
                                    "--consult"
                                };

      elle::system::ProcessConfig pc;
      {
        std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");

        if (!log_file.empty())
        {
          log_file += ".access.log";

          pc.inherit_current_environment();
          pc.setenv("ELLE_LOG_FILE", log_file);
        }
      }
      elle::system::Process p{std::move(pc), access_binary, arguments};
      ELLE_DEBUG("LAUNCH: %s %s", access_binary, boost::algorithm::join(arguments, " "));
      if (p.wait_status() != 0) // .8 sec
        throw Exception(gap_internal_error, "8access binary failed");
      std::stringstream ss;
      ss << p.read();

      std::string line;
      while(std::getline(ss, line))
        {
          if (!line.size())
            continue;
          std::stringstream line_stream{line};
          std::string public_key;
          int permissions;
          line_stream >> public_key /* eat "User" */
                      >> public_key
                      >> permissions;
          int gap_perm = (int) gap_none;
          if (permissions & nucleus::neutron::permissions::read)
            gap_perm |= gap_read;
          if (permissions & nucleus::neutron::permissions::write)
            gap_perm |= gap_write;
          std::string const& user_id = this->user_from_public_key(public_key)._id;
          infos->accesses[user_id] = gap_perm;
        }

      this->_files_infos[abspath] = infos.get();
      return *(infos.release());
    }

    void
    State::deprecated_set_permissions(std::string const& user_id,
                                      std::string const& abspath,
                                      nucleus::neutron::Permissions permissions,
                                      bool recursive)
    {
      FileInfos const& infos = this->file_infos(abspath);
      Network const& network = this->network(infos.network_id);

      this->_wait_portal(infos.network_id);

      std::string const& access_binary = common::infinit::binary_path("8access");

      std::list<std::string> arguments{
                                    "--user",
                                    this->_me._id,
                                    "--type",
                                    "user",
                                    "--grant",
                                    "--network",
                                    network._id,
                                    "--path",
                                    ("/" + infos.relative_path),
                                    "--identity",
                                    this->user(user_id).public_key
                                };

      if (permissions & nucleus::neutron::permissions::read)
        arguments.push_back("--read");
      if (permissions & nucleus::neutron::permissions::write)
        arguments.push_back("--write");

      ELLE_DEBUG("LAUNCH: %s %s", access_binary, boost::algorithm::join(arguments, " "));

      if (permissions & gap_exec)
        {
          ELLE_WARN("XXX: setting executable permissions not yet implemented");
        }

      elle::system::ProcessConfig pc;
      {
        std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");

        if (!log_file.empty())
        {
          log_file += ".access.log";

          pc.inherit_current_environment();
          pc.setenv("ELLE_LOG_FILE", log_file);
        }
      }
      elle::system::Process p{std::move(pc), access_binary, arguments};
      if (p.wait_status() != 0)
        throw Exception(gap_internal_error, "8access binary failed");

      if (recursive && elle::os::path::is_directory(abspath))
        {
          boost::filesystem::directory_iterator it{abspath},
                                                end{};
          for (; it != end; ++it)
            {
              this->deprecated_set_permissions(user_id,
                                               it->path().string(),
                                               permissions,
                                               true);
            }
        }
    }

    void
    State::set_permissions(std::string const& user_id,
                           std::string const& network_id,
                           nucleus::neutron::Permissions permissions)
    {
      ELLE_TRACE("setting permissions");

      // TODO: Do this only on the current device for sender and recipient.
      if (this->_wait_portal(network_id) == false)
          throw Exception{gap_error, "Couldn't find portal to infinit instance"};

      std::string const& access_binary = common::infinit::binary_path("8access");

      std::list<std::string> arguments{
                                  "--user",
                                  this->_me._id,
                                  "--type",
                                  "user",
                                  "--grant",
                                  "--network",
                                  network_id,
                                  "--path",
                                  "/",
                                  "--identity",
                                  this->user(user_id).public_key
                                };

      if (permissions & nucleus::neutron::permissions::read)
        arguments.push_back("--read");
      if (permissions & nucleus::neutron::permissions::write)
        arguments.push_back("--write");

      ELLE_DEBUG("LAUNCH: %s %s", access_binary, boost::algorithm::join(arguments, " "));

      if (permissions & gap_exec)
      {
        ELLE_WARN("XXX: setting executable permissions not yet implemented");
      }

      elle::system::ProcessConfig pc;
      {
        std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");

        if (!log_file.empty())
        {
          log_file += ".access.log";

          pc.inherit_current_environment();
          pc.setenv("ELLE_LOG_FILE", log_file);
        }
      }
      elle::system::Process p{std::move(pc), access_binary, arguments};
      if (p.wait_status() != 0)
        throw Exception(gap_internal_error, "8access binary failed");
    }

    size_t
    State::file_size(std::string const& path)
    {
      if (!fs::exists(path))
        return 0;

      if (!fs::is_directory(path))
        return fs::file_size(path);

      size_t size = 0;
      fs::directory_iterator end_itr;

      for (fs::directory_iterator itr(path);
           itr != end_itr;
           ++itr)
        {
          if (fs::is_directory(itr->status()))
            {
              size += this->file_size(itr->path().string());
            }
          else
            {
              size += fs::file_size(itr->path());
            }
        }
      return size;
    }


  }
}
