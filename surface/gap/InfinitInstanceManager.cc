#include <signal.h>
#include <stdlib.h>

#include <boost/filesystem.hpp>

#include <elle/Exception.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/system/signal.hh>

#include <etoile/Etoile.hh>
#include <etoile/wall/Access.hh>
#include <etoile/wall/Group.hh>
#include <etoile/wall/Object.hh>
#include <etoile/wall/Path.hh>
#include <etoile/path/Chemin.hh>

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
                                     std::string const& descriptor):
      network_id(network_id),
      mount_point(),
      network(network_id),
      identity(identity),
      passport(),
      descriptor(
        elle::serialize::from_string<elle::serialize::InputBase64Archive>(
          descriptor)),
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
          this->etoile.reset(
            new etoile::Etoile(this->identity.pair(),
                               this->hole.get(),
                               this->descriptor.meta().root()));
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
                                   std::string const& descriptor_digest)
    {
      ELLE_TRACE_SCOPE("%s: launch network %s", *this, network_id);

      if (this->_instances.find(network_id) != this->_instances.end())
        throw elle::Exception{"Network " + network_id + " already launched"};

      std::unique_ptr<InfinitInstance> instance(
        new InfinitInstance(
          this->_user_id, network_id, identity, descriptor_digest));

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

    void
    InfinitInstanceManager::add_user(
      std::string const& network_id,
      nucleus::neutron::Group::Identity const& group,
      nucleus::neutron::Subject const& subject)
    {
      ELLE_TRACE_SCOPE("%s: add user %s into network %s",
                       *this, subject, network_id);

      auto& instance = this->_instance(network_id);

      instance.scheduler.mt_run<void>(
        elle::sprintf("add_user for %s", network_id),
        [&] ()
        {
          auto& etoile = *instance.etoile;
          auto identifier = etoile::wall::Group::Load(etoile, group);

          elle::Finally discard{[&] ()
            {
              etoile::wall::Group::Discard(etoile, identifier);
            }
          };

          etoile::wall::Group::Add(etoile, identifier, subject);
          etoile::wall::Group::Store(etoile, identifier);

          discard.abort();
        });
    }

    void
    InfinitInstanceManager::grant_permissions(
      std::string const& network_id,
      nucleus::neutron::Subject const& subject)
    {
      ELLE_TRACE_SCOPE("%s: grant permissions to user %s into network %s",
                       *this, subject, network_id);

      auto& instance = this->_instance(network_id);

      instance.scheduler.mt_run<void>(
        elle::sprintf("grant permissions for %s", network_id),
        [&] ()
        {
          auto& etoile = *instance.etoile;

          etoile::path::Chemin chemin = etoile::wall::Path::resolve(etoile, "/");
          auto identifier = etoile::wall::Object::load(etoile, chemin);

          elle::Finally discard{[&] ()
            {
              etoile::wall::Object::discard(etoile, identifier);
            }
          };

          etoile::wall::Access::Grant(etoile,
                                      identifier,
                                      subject,
                                      nucleus::neutron::permissions::write);

          etoile::wall::Object::store(etoile, identifier);

          discard.abort();
        });
    }


    InfinitInstance&
    InfinitInstanceManager::_instance(std::string const& network_id)
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
