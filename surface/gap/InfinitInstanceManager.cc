#include <signal.h>
#include <stdlib.h>

#include <boost/filesystem.hpp>

#include <elle/Exception.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/system/signal.hh>

#include <common/common.hh>
#include <surface/gap/InfinitInstanceManager.hh>
#include <surface/gap/binary_config.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.InfinitInstanceManager");

namespace surface
{
  namespace gap
  {
    InfinitInstance::InfinitInstance(std::string const& user_id,
                                     std::string const& network_id,
                                     lune::Identity const& identity,
                                     nucleus::proton::Address const& root_addr):
      network_id(network_id),
      mount_point(),
      network(network_id),
      identity(identity),
      passport(),
      storage(this->network, common::infinit::network_shelter(user_id,
                                                              network_id)),
      hole(),
      etoile(),
      scheduler(),
      keep_alive(scheduler, "Keep alive", [] ()
                 {
                   while (true)
                   {
                     auto* current = reactor::Scheduler::scheduler()->current();
                     current->sleep(boost::posix_time::seconds(60));
                   }
                 }),
      thread(std::bind(&reactor::Scheduler::run, std::ref(scheduler)))
    {
      this->scheduler.mt_run<void>(
        elle::sprintf("initalizer for %s", network_id),
        [&] ()
        {
          elle::serialize::from_file(common::infinit::passport_path(user_id))
            >> this->passport;

          this->hole.reset(new hole::implementations::slug::Slug(
                             storage,
                             passport,
                             Infinit::authority(),
                             reactor::network::Protocol::tcp));
          this->etoile.reset(new etoile::Etoile(this->identity.pair(),
                                                this->hole.get(),
                                                root_addr));
        });
    }

    InfinitInstanceManager::InfinitInstanceManager(std::string const& user_id)
      : _user_id{user_id}
    {
      ELLE_TRACE_METHOD(user_id);
    }

    InfinitInstanceManager::~InfinitInstanceManager()
    {
      ELLE_TRACE_METHOD("");

      this->clear();
    }

    void
    InfinitInstanceManager::clear()
    {
      ELLE_TRACE_METHOD("");

      auto it = begin(this->_instances);

      while(it != end(this->_instances))
      {
        this->stop(it->first);
        it = begin(this->_instances);
      }

      ELLE_ASSERT(this->_instances.empty());
    }

    void
    InfinitInstanceManager::launch(std::string const& network_id,
                                   lune::Identity const& identity,
                                   nucleus::proton::Address const& root_addr)
    {
      ELLE_TRACE_SCOPE("%s: launch network %s", *this, network_id);

      if (this->_instances.find(network_id) != this->_instances.end())
        throw elle::Exception{"Network " + network_id + " already launched"};

      std::unique_ptr<InfinitInstance> instance(
        new InfinitInstance(this->_user_id, network_id, identity, root_addr));
      this->_instances.insert(std::make_pair(network_id, std::move(instance)));
    }

    void
    InfinitInstanceManager::stop(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);

      ELLE_ASSERT(this->_instances.find(network_id) != this->_instances.end());
      this->_instances.erase(network_id);
    }

    bool
    InfinitInstanceManager::exists(std::string const& network_id) const
    {
      return this->_instances.find(network_id) != this->_instances.end();
    }

    InfinitInstance const&
    InfinitInstanceManager::_instance(std::string const& network_id) const
    {
      auto it = this->_instances.find(network_id);
      if (it == this->_instances.end())
        throw elle::Exception{"Cannot find any network " + network_id};
      ELLE_ASSERT_NEQ(it->second, nullptr);
      return *(it->second);
    }

    InfinitInstance const*
    InfinitInstanceManager::_instance_for_file(std::string const& path)
    {
      ELLE_TRACE_METHOD(path);

      for (auto const& pair : this->_instances)
      {
        std::string mount_point = pair.second->mount_point;

        if (path.compare(0, mount_point.length(), path) != 0)
          return pair.second.get();
      }
      return nullptr;
    }

    /*----------.
    | Printable |
    `----------*/
    void
    InfinitInstanceManager::print(std::ostream& stream) const
    {
      stream << "InstanceManager(" << this->_user_id << ")";
    }
  }
}
