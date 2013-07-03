#include <signal.h>
#include <stdlib.h>

#include <boost/filesystem.hpp>

#include <HoleFactory.hh>

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
#include <surface/gap/_detail/TransferOperations.hh>
#include <surface/gap/InfinitInstanceManager.hh>
#include <surface/gap/binary_config.hh>

#include <boost/algorithm/string.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.InfinitInstanceManager");

namespace surface
{
  namespace gap
  {
    InfinitInstance::InfinitInstance(std::string const& user_id,
                                     std::string const& token,
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
        elle::sprintf("initializer for %s", network_id),
        [&] ()
        {
          elle::serialize::from_file(common::infinit::passport_path(user_id))
            >> this->passport;

          this->hole = infinit::hole_factory(this->descriptor,
                                             storage,
                                             passport,
                                             Infinit::authority(),
                                             {});

          this->etoile.reset(
            new etoile::Etoile(this->identity.pair(),
                               this->hole.get(),
                               this->descriptor.meta().root()));
        });
    }

    InfinitInstanceManager::InfinitInstanceManager(std::string const& user_id,
                                                   std::string const& token)
      : _user_id{user_id}
      , _token{token}
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
          this->_user_id, this->_token, network_id, identity, descriptor_digest));

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

    void
    InfinitInstanceManager::upload_files(std::string const& network_id,
                                         std::unordered_set<std::string> items)
    {
      ELLE_TRACE_SCOPE("%s: uploading  %s into network %s",
                       *this, items, network_id);

      auto& instance = this->_instance(network_id);

      instance.scheduler.mt_run<void>(
        elle::sprintf("upload files for %s", network_id),
        [&] ()
        {
          auto& etoile = *instance.etoile;

          nucleus::neutron::Subject subject;
          subject.Create(instance.descriptor.meta().administrator_K());

          operation_detail::to::send(etoile, instance.descriptor, subject, items);
        });
    }

    void
    InfinitInstanceManager::download_files(std::string const& network_id,
                                           nucleus::neutron::Subject const& subject,
                                           std::string const& destination)
    {
      ELLE_TRACE_SCOPE("%s: download files from network %s into %s",
                       *this, network_id, destination);

      auto& instance = this->_instance(network_id);

      instance.scheduler.mt_run<void>(
        elle::sprintf("download files for %s", network_id),
        [&] ()
        {
          auto& etoile = *instance.etoile;

          operation_detail::from::receive(etoile, instance.descriptor, subject, destination);
        });
    }

    int
    InfinitInstanceManager::connect_try(std::string const& network_id,
                                        std::vector<std::string> const& addresses)
    {
      ELLE_TRACE_SCOPE("%s: connecting infinit of network %s to %s",
                       *this, network_id, addresses);

      auto& instance = this->_instance(network_id);

      return instance.scheduler.mt_run<int>(
        elle::sprintf("connecting nodes for %s", network_id),
        [&] () -> int
        {
          auto& hole = dynamic_cast<hole::implementations::slug::Slug&>(*instance.hole);

          typedef std::unique_ptr<reactor::VThread<bool>> VThreadBoolPtr;
          std::vector<std::pair<VThreadBoolPtr, std::string>> v;

          auto slug_connect = [&] (std::string const& endpoint)
            {
              std::vector<std::string> result;
              boost::split(result, endpoint, boost::is_any_of(":"));

              auto const &ip = result[0];
              auto const &port = result[1];
              ELLE_DEBUG("slug_connect(%s, %s)", ip, port)
              hole.portal_connect(ip, std::stoi(port));

              ELLE_DEBUG("slug_wait(%s, %s)", ip, port)
              if (!hole.portal_wait(ip, std::stoi(port)))
                throw elle::Exception(elle::sprintf("slug_wait(%s, %s) failed",
                                                    ip, port));
            };

          int i = 0;
          for (auto const& address: addresses)
          {
            try
            {
              slug_connect(address);
              ++i;
              ELLE_LOG("%s: connection to %s succeed", *this, address);
            }
            catch (elle::Exception const& e)
            {
              ELLE_WARN("%s: connection to %s failed", *this, address);
            }
          }

          ELLE_TRACE("%s: finish connecting to %d node%s",
                     *this, i, i > 1 ? "s" : "");
          return i;
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
