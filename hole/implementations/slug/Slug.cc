#include <elle/attribute.hh>
#include <elle/cast.hh>
#include <elle/container/map.hh>
#include <elle/container/vector.hh>
#include <elle/memory.hh>
#include <elle/utility/Move.hh>

#include <cryptography/random.hh>

#include <reactor/network/tcp-server.hh>
#include <reactor/network/udp-socket.hh>
#include <reactor/network/udt-server.hh>

#include <hole/Exception.hh>
#include <hole/implementations/slug/Host.hh>
#include <hole/implementations/slug/Slug.hh>

#include <nucleus/proton/Block.hh>

ELLE_LOG_COMPONENT("infinit.hole.slug.Slug");

namespace hole
{
  namespace implementations
  {
    namespace slug
    {

      /*-------------.
      | Construction |
      `-------------*/

      Slug::Slug(hole::storage::Storage& storage,
                 elle::Passport const& passport,
                 elle::Authority const& authority,
                 reactor::network::Protocol protocol,
                 std::vector<elle::network::Locus> const& members,
                 int port,
                 reactor::Duration connection_timeout,
                 std::unique_ptr<reactor::network::UDPSocket> socket):
        Hole(storage, passport, authority),
        _protocol(protocol),
        _members(members),
        _connection_timeout(connection_timeout),
        _state(State::detached),
        _port(port),
        _server(nullptr),
        _acceptor()
      {
        ELLE_TRACE_SCOPE("launch slug");
        auto& sched = *reactor::Scheduler::scheduler();
        try
        {
          ELLE_TRACE("try connecting to peers")
          {
            // FIXME: use builtin support for subcoroutines when available.
            std::vector<reactor::Thread*> connections;
            for (elle::network::Locus const& locus: this->members())
            {
              auto action = [&, locus] { this->_connect_try(locus); };
              auto thread = new reactor::Thread
                (sched, elle::sprintf("connect %s", locus), action);
              connections.push_back(thread);
            }
            for (reactor::Thread* t: connections)
            {
              sched.current()->wait(*t);
              delete t;
            }
          }
          // If the machine has been neither connected nor authenticated
          // to existing nodes...
          if (this->_state == State::detached)
          {
            ELLE_DEBUG("alone in the network");
            // Then, suppose that the current machine as the only one
            // in the network.  Thus, it can be implicitly considered
            // as authenticated in a network composed of itself alone.
            this->_state = State::attached;
          }

          if (socket)
          {
            // XXX: for now rebinding a socket is only available with UDT.
            ELLE_ASSERT_EQ(this->protocol(), reactor::network::Protocol::udt);
            this->_server = elle::make_unique<reactor::network::UDTServer>(
              *reactor::Scheduler::scheduler(), std::move(socket));
          }
          else
          {
            this->_server = reactor::network::Server::create(
              this->protocol(), sched);
          }

          // Finally, listen for incoming connections.
          ELLE_TRACE("serve on port %s", this->_port);
          {
            elle::network::Locus locus;
            elle::network::Host host(elle::network::Host::TypeAny);
            try
            {
              _server->listen(this->_port);
              // In case we asked for a random port to be picked up (by using 0)
              // or hole punching happened, retrieve the actual listening port.
              this->_port = this->_server->port();
              ELLE_ASSERT(this->_port != 0);
              ELLE_DEBUG("listening on port %s", this->_port);
              _acceptor.reset(new reactor::Thread(
                                *reactor::Scheduler::scheduler(),
                                "Slug accept",
                                [&] {this->_accept();}));
            }
            catch (reactor::Exception& e)
            {
              ELLE_ERR("unable to accept incoming connections: %s", e.what());
              // FIXME: what do ? For now, just go on without
              // listening. Useful when testing with several clients
              // on the same machine.
            }
          }
        }
        catch (...)
        {
          if (_acceptor)
            _acceptor->terminate_now();
          throw;
        }
      }

      Slug::~Slug()
      {
        for (auto host: Hosts(_hosts))
          this->_remove(host.second);

        // Stop serving; we may not be listening, since bind errors are
        // considered warnings (see constructor), in which case we have no
        // acceptor.
        if (_acceptor)
          _acceptor->terminate_now();
      }

      void
      Slug::_push(const nucleus::proton::Address& address,
                  const nucleus::proton::ImmutableBlock& block)
      {
        ELLE_TRACE_METHOD(address, block);

        // depending on the machine's state.
        switch (this->_state)
        {
          case State::attached:
          {
            // In this case, the current machine is connected and
            // has been authenticated as a valid node of the
            // network.  Therefore, the operation is carried out
            // both locally but also sent to every node in the
            // network.

            // Store the block locally.
            {
              block.validate(address);

              this->storage().store(address, block);
            }

            // Publish it onto the network.
            {
              for (auto neighbour: _hosts)
              {
                Host* host = neighbour.second;

                try
                {
                  host->push(address, block);
                }
                catch (std::exception const& e)
                {
                  ELLE_WARN("[%p] remote exception: %s",
                            this, e.what());
                  continue;
                }
              }
            }

            break;
          }
          default:
          {
            auto fmt = "the machine's state '%u' does not allow one "
              "to request operations on the storage layer";
            throw Exception(elle::sprintf(fmt, this->_state));
          }
        }
      }

      void
      Slug::_push(const nucleus::proton::Address& address,
                  const nucleus::proton::MutableBlock& block)
      {
        ELLE_TRACE_METHOD(address, block);

        switch (this->_state)
        {
          case State::attached:
          {
            // In this case, the current machine is connected and has been
            // authenticated as a valid node of the network.  Therefore, the
            // operation is carried out both locally but also sent to every node
            // in the network.

            // First, store the block locally.
            {
              // Validate the block, depending on its component.
              //
              // Indeed, the Object component requires as additional block for
              // being validated.
              switch (address.component())
              {
                case nucleus::neutron::ComponentObject:
                {
                  /* XXX[need to change the way validation works by relying
                     on a callback]
                     const nucleus::neutron::Object* object =
                     static_cast<const nucleus::neutron::Object*>(&block);
                     assert(dynamic_cast<const nucleus::neutron::Object*>(
                     &block) != nullptr);

                     // validate the object according to the presence of
                     // a referenced access block.
                     if (object->access() != nucleus::proton::Address::null())
                     {
                     // Load the access block.
                     std::unique_ptr<nucleus::proton::Block> addressBlock
                     (this->pull(object->access(),
                     nucleus::proton::Revision::Last));

                     // Get access block.
                     nucleus::neutron::Access * access =
                     dynamic_cast<nucleus::neutron::Access *>(addressBlock.get());

                     if (access == nullptr)
                     throw Exception("expected an access block");

                     // validate the object, providing the
                     object->validate(address, access);
                     }
                     else
                     {
                     // validate the object.
                     object->validate(address, nullptr);
                     }
                  */

                  break;
                }
                default:
                {
                  // Validate the block through the common interface.
                  block.validate(address);

                  break;
                }
                case nucleus::neutron::ComponentUnknown:
                {
                  throw Exception(elle::sprintf("unknown component '%u'",
                                                address.component()));
                }
              }

              this->storage().store(address, block);
            }

            // Publish it onto the network.
            {
              for (auto neighbour: this->_hosts)
              {
                Host* host = neighbour.second;

                try
                {
                  host->push(address, block);
                }
                catch (std::exception const& e)
                {
                  ELLE_WARN("[%p] remote exception: %s",
                            this, e.what());
                  continue;
                }
              }
            }

            break;
          }
          default:
          {
            auto fmt = "the machine's state '%u' does not allow one "
              "to request operations on the storage layer";
            throw Exception(elle::sprintf(fmt, this->_state));
          }
        }

#ifdef CACHE
        {
          ELLE_LOG_COMPONENT("infinit.hole.slug.cache");

          // Finally, now that the block has been accepted as a valid revision
          // of the mutable block, record it in the cache so that the machine
          // will no longer have to fetch the block from the other peers since
          // it already has the last revision and the other nodes will publish
          // any new revision.
          elle::String unique = address.unique();
          auto iterator = cache.find(unique);

          if (iterator == cache.end())
          {
            elle::String unique = address.unique();

            ELLE_TRACE("%s: register %s", *this, unique);

            elle::utility::Time current;

            if (current.Current() == elle::Status::Error)
              throw Exception("unable to retrieve the current time");

            auto result =
              cache.insert(std::pair<elle::String,
                           elle::utility::Time>(unique, current));

            if (result.second == false)
              throw Exception(
                elle::sprintf("unable to insert the address '%s' in the cache",
                              unique));
          }
          else
          {
            elle::utility::Time current;

            ELLE_TRACE("%s: update %s", *this, unique);

            if (current.Current() == elle::Status::Error)
              throw Exception("unable to retrieve the current time");

            iterator->second = current;
          }
        }
#endif
      }

      std::unique_ptr<nucleus::proton::Block>
      Slug::_pull(const nucleus::proton::Address& address)
      {
        ELLE_TRACE_METHOD(address);

        using nucleus::proton::ImmutableBlock;

        // Depending on the machine's state.
        switch (this->_state)
        {
          case State::attached:
          {
            // In this case, the current machine is connected and has been
            // authenticated as a valid node of the network.  Therefore, the
            // operation is carried out both locally but also sent to every node
            // in the network.

            std::unique_ptr<nucleus::proton::Block> block;

            if (!this->storage().exist(address))
            {
              ELLE_TRACE("the immutable block does not exist locally,"
                         " fetch %s from the peers", address);
              // Go through the neighbours and retrieve the block from them.
              bool found = false;
              for (auto neighbour: this->_hosts)
              {
                Host* host = neighbour.second;
                assert(host != nullptr);

                std::unique_ptr<nucleus::proton::ImmutableBlock> iblock;
                ELLE_TRACE("fetch %s from peer %s", address, *host);

                try
                {
                  iblock = elle::cast<ImmutableBlock>::runtime(
                    host->pull(address, nucleus::proton::Revision::Any));
                }
                catch (std::exception const& e)
                {
                  ELLE_WARN("%s: remote exception: %s",
                            this, e.what());
                  continue;
                }

                ELLE_TRACE("block fetched: %s", *iblock);

                // Validate the block.
                try
                {
                  this->storage().store(address, *iblock);

                  found = true;

                  // No need to continue since the block is immutable.
                  break;
                }
                catch (nucleus::Exception const& e)
                {
                  ELLE_WARN("%s: unable to validate the block '%s'",
                            this, address);
                  continue;
                }
              }

              // Check if none if the neighbour has the block.
              if (!found)
                throw Exception(
                  "unable to retrieve the block associated with "
                  "the given address from the other peers");
            }

            assert(this->storage().exist(address));

            ELLE_TRACE("load the local block at %s", address)
              block = this->storage().load(address);

            // Validate the block.
            block->validate(address);

            ELLE_TRACE("block loaded and validated: %s", *block);

            return block;
          }
          default:
          {
            auto fmt = "the machine's state '%u' does not allow one "
              "to request operations on the storage layer";
            throw Exception(elle::sprintf(fmt, this->_state));
          }
        }
      }

      std::unique_ptr<nucleus::proton::Block>
      Slug::_pull(const nucleus::proton::Address& address,
                  const nucleus::proton::Revision& revision)
      {
        ELLE_DEBUG_METHOD(address, revision);

        // Check the machine is connected and has been authenticated
        // as a valid node of the network.
        if (this->_state != State::attached)
          throw Exception(elle::sprintf(
                            "the machine's state '%u' does not allow one "
                            "to request operations on the storage layer",
                            this->_state));

        if (revision == nucleus::proton::Revision::Last)
          return _get_latest(address);
        else
          return _get_specific(address, revision);
      }

      template <typename T>
      using Ptr = std::unique_ptr<T>;

      Ptr<nucleus::proton::Block>
      Slug::_get_latest(const nucleus::proton::Address&    address)
      {
        ELLE_DEBUG_METHOD(address);

        // Contact all the hosts in order to retrieve the latest
        // revision of the block.
        //
        // This is required since no synchronisation mechanism is
        // present yet so the current machine may have missed some
        // revisions when disconnected.
        using nucleus::proton::MutableBlock;
        using nucleus::proton::Revision;
        using nucleus::neutron::Object;
        using nucleus::proton::Block;

#ifdef CACHE
        elle::String unique = address.unique();
        auto iterator = cache.find(unique);
        {
          ELLE_LOG_COMPONENT("infinit.hole.slug.cache");

          // First, check whether the address is already contained in the
          // cache. If so, the local block is used rather than issuing network
          // communication. The goal of this cache is to say 'the local revision
          // of the block is the latest, so use it instead of bothering everyone
          // else'. Note that this optimization works because all nodes are
          // supposed to be trustworthy (in the 'slug' implementation) and
          // therefore to send the newest revision of every modified block.

          if (iterator != cache.end())
            {
              elle::utility::Time deadline =
                iterator->second +
                elle::utility::Duration(elle::utility::Duration::UnitSeconds,
                                        CACHE_LIFESPAN);
              elle::utility::Time current;

              if (current.Current() == elle::Status::Error)
                throw Exception("unable to retrieve the current time");

              if (current < deadline)
                {
                  // In this case, the block is still valid and can therefore
                  // be used.
                  Ptr<nucleus::proton::Block> ptr;

                  // Make sure the block exists, otherwise, fall down to the
                  // usual case: retrieving the block from the network.
                  if (this->storage().exist(address))
                    {
                      ELLE_DEBUG("%s: cache hit on %s", *this, unique);

                      // Load the block.
                      ptr = this->storage().load(address);

                      return ptr;
                    }
                  else
                    {
                      ELLE_DEBUG("%s: cache hit but block %s not present locally", *this, unique);
                    }
                }
              else
                {
                  // Otherwise, the block must be discarded, as too old to
                  // be used.
                  //
                  // And the process must fall back to the original one which
                  // consists in retrieving the block from peers.

                  ELLE_DEBUG("%s: cache miss (expired) on %s", *this, unique);

                  cache.erase(iterator);
                }
            }
        }
#endif

        ELLE_DEBUG_SCOPE("%s: retrieving the block '%s' from the network",
                         this, address);

        for (auto neighbour: this->_hosts)
          {
            Host* host = neighbour.second;
            std::unique_ptr<MutableBlock> block;

            try
              {
                block =
                  elle::cast<MutableBlock>::runtime(
                    host->pull(address, Revision::Last));
              }
            catch (std::exception const& e)
              {
                ELLE_WARN("%s: remote exception: %s",
                          this, e.what());
                continue;
              }

            // Validate the block, depending on its component.
            // Indeed, the Object component requires as additional
            // block for being validated.
            switch (address.component())
              {
              case nucleus::neutron::ComponentObject:
                {
                  /* XXX[need to change the way validation works by relying
                         on a callback]
                  assert(dynamic_cast<Object const*>(block.get()) != nullptr);
                  Object const& object = *static_cast<Object*>(block.get());

                  // Validate the object according to the presence of
                  // a referenced access block.
                  if (object.access() != nucleus::proton::Address::null())
                    {
                      Ptr<Block> block(this->pull(object.access(), Revision::Last));
                      Ptr<nucleus::neutron::Access> access
                        (dynamic_cast<nucleus::neutron::Access*>(block.release()));
                      if (access == nullptr)
                        {
                          ELLE_WARN("%s: unable to load the access block for "
                                    "validating the object", this);
                          continue;
                        }

                      // validate the object, providing the
                      try
                        {
                          object.validate(address, access.get());
                        }
                      catch (nucleus::Exception const& e)
                        {
                          ELLE_WARN("%s: unable to validate the access block",
                                    this);
                          continue;
                        }
                    }
                  else
                    {
                      // Validate the object.
                      try
                        {
                          object.validate(address, nullptr);
                        }
                      catch (nucleus::Exception const& e)
                        {
                          ELLE_WARN("%s: unable to validate the access block",
                                    this);
                          continue;
                        }
                    }
                  */

                  break;
                }
              default:
                {
                  // validate the block through the common
                  // interface.
                  try
                    {
                      block->validate(address);
                    }
                  catch (nucleus::Exception const& e)
                    {
                      ELLE_WARN("%s: unable to validate the block",
                                this);
                      continue;
                    }

                  break;
                }
              case nucleus::neutron::ComponentUnknown:
                {
                  throw Exception(elle::sprintf("unknown component '%u'",
                                                         address.component()));
                }
              }

            // XXX It force conflict to be public. Can we change that ?
            ELLE_DEBUG("Check if the block derives the current block")
              if (this->storage().conflict(address, *block))
                {
                  ELLE_DEBUG("the block %p does not derive the local one",
                             block);
                  continue;
                }

            ELLE_DEBUG("storing the remote block %s locally", address)
              this->storage().store(address, *block);
          }

        // At this point, we may have retrieved one or more revisions
        // of the mutable block but we do not have any guarantee.

        if (!this->storage().exist(address))
          throw Exception("unable to retrieve the mutable block");

        Ptr<MutableBlock> block;

        // load the block.
        ELLE_DEBUG("loading the local block at %s", address);

        block = elle::cast<MutableBlock>::runtime(
          this->storage().load(address));

        ELLE_DEBUG("loaded block %s has revision %s",
                   block, block->revision());

        ELLE_DEBUG("validating the block")
        // Validate the block, depending on its component.
        // although every stored block has been checked, the block
        // may have been corrupt while on the hard disk.
        //
        // Note that the Object component requires as additional
        // block for being validated.
        switch (address.component())
          {
          case nucleus::neutron::ComponentObject:
            {
              /* XXX[need to change the way validation works by relying
                     on a callback]
              const Object* object =
                static_cast<const Object*>(block.get());
              assert(dynamic_cast<const Object*>(block.get()) != nullptr);

              // Validate the object according to the presence of
              // a referenced access block.
              if (object->access() != nucleus::proton::Address::null())
                {
                  // Load the access block.
                  Ptr<Block> block
                    (this->pull(object->access(), nucleus::proton::Revision::Last));
                  Ptr<nucleus::neutron::Access> access
                    (dynamic_cast<nucleus::neutron::Access*>(block.release()));
                  if (access == nullptr)
                    throw Exception("expected an access block");
                  // Validate the object, providing the
                  object->validate(address, access.get());
                }
              else
                {
                  // Validate the object.
                  object->validate(address, nullptr);
                }
              */

              break;
            }
          default:
            {
              // validate the block through the common interface.
              block->validate(address);

              break;
            }
          case nucleus::neutron::ComponentUnknown:
            {
              throw Exception(elle::sprintf("unknown component '%u'",
                                                     address.component()));
            }
          }

#ifdef CACHE
        {
          ELLE_LOG_COMPONENT("infinit.hole.slug.cache");

          // At this point, now that the block has been retrieved from
          // the network, the cache is updated by marking this mutable
          // block as up-to-date locally, meaning that the machine no
          // longer needs to fetch it from the other peers. Instead
          // the peers will notify everyone of the updated revisions.
          iterator = cache.find(unique);

          if (iterator == cache.end())
            {
              elle::String unique = address.unique();

              ELLE_DEBUG("%s: register %s", *this, unique);

              elle::utility::Time current;

              if (current.Current() == elle::Status::Error)
                throw Exception("unable to retrieve the current time");

              auto result =
                cache.insert(std::pair<elle::String,
                                       elle::utility::Time>(unique, current));

              if (result.second == false)
                throw Exception(
                  elle::sprintf("unable to insert the address '%s' in the cache",
                                unique));
            }
          else
            {
              elle::utility::Time current;

              ELLE_DEBUG("%s: update %s", *this, unique);

              if (current.Current() == elle::Status::Error)
                throw Exception("unable to retrieve the current time");

              iterator->second = current;
            }
        }
#endif

        return Ptr<Block>(block.release());
      }


      std::unique_ptr<nucleus::proton::Block>
      Slug::_get_specific(const nucleus::proton::Address& address,
                          nucleus::proton::Revision const& revision)
      {
        // Go through the neighbours and retrieve the
        // specific revision of the block.

        using nucleus::neutron::Object;
        using nucleus::proton::MutableBlock;

        Ptr<MutableBlock> block;

        // If the block does not exist, retrieve it from the peers.
        if (!this->storage().exist(address, revision))
          {
            bool found = false;
            for (auto neighbour: this->_hosts)
              {
                Host* host = neighbour.second;
                std::unique_ptr<MutableBlock> block;

                try
                  {
                    block =
                      elle::cast<MutableBlock>::runtime(
                        host->pull(address, revision));
                  }
                catch (std::exception const& e)
                  {
                    ELLE_WARN("%s: remote exception: %s",
                              this, e.what());
                    continue;
                  }

                // validate the block, depending on its
                // component.
                //
                // indeed, the Object component requires as
                // additional block for being validated.
                switch (address.component())
                  {
                  case nucleus::neutron::ComponentObject:
                    {
                      /* XXX[need to change the way validation works by relying
                             on a callback]
                      Object const&  object =
                        static_cast<Object const&>(derivable.block());
                      assert(dynamic_cast<Object const*>(
                               &(derivable.block())) != nullptr);

                      // validate the object according to the
                      // presence of a referenced access block.
                      if (object.access() != nucleus::proton::Address::null())
                        {
                          Ptr<nucleus::proton::Block> block
                            (this->pull(object.access(),
                                                   nucleus::proton::Revision::Last));
                          Ptr<nucleus::neutron::Access> access
                            (dynamic_cast<nucleus::neutron::Access*>(block.release()));
                          if (access == nullptr)
                            {
                              ELLE_WARN("%s: unable to retrieve the access "
                                        "block for validating the object",
                                        this);
                              continue;
                            }

                          // validate the object, providing the
                          try
                            {
                              object.validate(address, access.get());
                            }
                          catch (nucleus::Exception const& e)
                            {
                              ELLE_WARN("%s: unable to validate the object",
                                        this);
                              continue;
                            }
                        }
                      else
                        {
                          // validate the object.
                          try
                            {
                              object.validate(address, nullptr);
                            }
                          catch (nucleus::Exception const& e)
                            {
                              ELLE_WARN("%s: unable to validate the object",
                                        this);
                              continue;
                            }
                        }
                      */

                      break;
                    }
                  default:
                    {
                      // Validate the block through the common
                      // interface.
                      try
                        {
                          block->validate(address);
                        }
                      catch (nucleus::Exception const& e)
                        {
                          ELLE_WARN("%s: unable to validate the block",
                                    this);
                          continue;
                        }
                    }
                  case nucleus::neutron::ComponentUnknown:
                    {
                      throw Exception(elle::sprintf("unknown component '%u'",
                                                             address.component()));
                    }
                  }

                found = true;

                // stop since a block for this specific
                // revision has been retrieved.
                break;
              }

            // check if none if the neighbour has the block.
            if (!found)
              throw Exception("unable to retrieve the block associated with "
                                       "the given address from the other peers");
          }

        // Try to retrieve the block from the local storage.
        assert(this->storage().exist(address, revision));

        // Load the block.
        block = elle::cast<MutableBlock>::runtime(
          this->storage().load(address, revision));

        // validate the block, depending on its component.
        // although every stored block has been checked, the
        // block may have been corrupt while on the hard disk.
        //
        // note that the Object component requires as additional
        // block for being validated.
        switch (address.component())
          {
          case nucleus::neutron::ComponentObject:
            {
              /* XXX[need to change the way validation works by relying
                     on a callback]
              assert(dynamic_cast<const Object*>(block.get()) != nullptr);
              const Object* object = static_cast<const Object*>(block.get());

              // validate the object according to the presence of
              // a referenced access block.
              if (object->access() !=
                  nucleus::proton::Address::null())
                {
                  // Load the access block.
                  Ptr<nucleus::proton::Block> block
                    (this->pull(object->access(), nucleus::proton::Revision::Last));
                  Ptr<nucleus::neutron::Access> access
                    (dynamic_cast<nucleus::neutron::Access*>(block.release()));
                  if (access == nullptr)
                    throw Exception("expected an access block");

                  // Validate the object.
                  object->validate(address, access.get());
                }
              else
                {
                  // validate the object.
                  object->validate(address, nullptr);
                }
              */

              break;
            }
          default:
            {
              // validate the block through the common interface.
              block->validate(address);

              break;
            }
          case nucleus::neutron::ComponentUnknown:
            {
              throw Exception(elle::sprintf("unknown component '%u'",
                                                     address.component()));
            }
          }

        return std::move(block);
      }

      void
      Slug::_wipe(const nucleus::proton::Address& address)
      {
        ELLE_TRACE_METHOD(address);

        switch (this->_state)
        {
          case State::attached:
          {
            // In this case, the current machine is connected and has been
            // authenticated as a valid node of the network.  Therefore, the
            // operation is carried out both locally but also sent to every node
            // in the network.

            // Remove the block locally.
            {
              // Treat the request depending on the nature of the block which
              // the addres indicates.
              switch (address.family())
              {
                case nucleus::proton::Family::content_hash_block:
                {
                  // erase the immutable block.
                  this->storage().erase(address);

                  break;
                }
                case nucleus::proton::Family::public_key_block:
                case nucleus::proton::Family::owner_key_block:
                case nucleus::proton::Family::imprint_block:
                {
                  this->storage().erase(address);

                  break;
                }
                default:
                {
                  throw Exception(elle::sprintf("unknown block family %s",
                                                address.family()));
                }
              }
            }

#ifdef CACHE
            elle::String unique = address.unique();
            auto iterator = cache.find(unique);
            {
              // If the address is present in the cache, remove it.
              if (iterator != cache.end())
                cache.erase(iterator);
            }
#endif

            // Notify the other hosts of the removal.
            {
              for (auto neighbour: this->_hosts)
              {
                Host* host = neighbour.second;

                try
                {
                  host->wipe(address);
                }
                catch (std::exception const& e)
                {
                  ELLE_WARN("%s: remote exception: %s",
                            this, e.what());
                  continue;
                }
              }
            }

            break;
          }
          default:
          {
            throw Exception
              (elle::sprintf(
                "the machine's state '%s' does not allow one to "
                "request operations on the storage layer",
                this->_state));
          }
        }
      }

      /*------.
      | Hosts |
      `------*/

      std::vector<elle::network::Locus>
      Slug::loci()
      {
        std::vector<elle::network::Locus> res;
        for (auto host: _hosts)
          res.push_back(host.first);
        return std::move(res);
      }

      std::vector<Host*>
      Slug::hosts()
      {
        std::vector<Host*> res;
        for (auto host: _hosts)
          res.push_back(host.second);
        return std::move(res);
      }

      void
      Slug::_connect(elle::network::Locus const& locus)
      {
        ELLE_TRACE_SCOPE("try connecting to %s", locus);
        std::string hostname;
        locus.host.Convert(hostname);
        std::unique_ptr<reactor::network::Socket> socket(
          reactor::network::Socket::create(
            this->protocol(),
            *reactor::Scheduler::scheduler(),
            hostname, locus.port, _connection_timeout));
        _connect(std::move(socket), locus, true);
      }

      void
      Slug::_connect(std::unique_ptr<reactor::network::Socket> socket,
                        elle::network::Locus const& locus, bool opener)
      {
        // Beware: do not yield between the host creation and the
        // authentication, or we might face a race condition.
        Host* host = new Host(*this, locus, std::move(socket));
        // XXX: leak
        ELLE_TRACE("%s: authenticate to host: %s", *this, locus);
        auto loci = host->authenticate(this->passport());
        if (this->_state == State::detached)
          this->_state = State::attached;
        // XXX Propagation disabled.
        // for (auto locus: loci)
        //   if (_hosts.find(locus) == _hosts.end())
        //     _connect_try(locus);
        if (host->authenticated())
          // If the remote machine has authenticated, validate this host.
          this->_host_register(host);
      }

      void
      Slug::_host_register(Host* host)
      {
        ELLE_LOG("%s: add host: %s", *this, *host);
        // XXX: the next line is broken
        host->remote_passport(this->passport());
        this->_hosts[host->locus()] = host;
        this->_new_host.signal();
      }

      void
      Slug::_connect_try(elle::network::Locus const& locus)
      {
        try
          {
            _connect(locus);
          }
        catch (reactor::network::Exception& err)
          {
            ELLE_TRACE("ignore host %s: %s", locus, err.what());
          }
      }

      void
      Slug::_remove(Host* host)
      {
        elle::network::Locus locus(host->locus());
        assert(this->_hosts.find(locus) != this->_hosts.end());
        ELLE_LOG("%s: remove host: %s", *this, *host);
        this->_hosts.erase(locus);
        delete host;
      }

      /*-------.
      | Server |
      `-------*/

      void
      Slug::_accept()
      {
        reactor::Scope scope;

        while (true)
        {
          std::unique_ptr<reactor::network::Socket> socket(_server->accept());

          ELLE_TRACE_SCOPE("accept connection from %s",
                           socket->remote_locus());

#ifdef CACHE
          {
            // We need to clear cached blocks whenever a node joins the
            // network.
            //
            // Indeed, assuming the block B is in cache at revision 4
            // and considering the new node actually as a newer revision
            // of the block, say 5.
            //
            // By clearing the cache, the system will make sure to ask
            // the peers for the latest revision of the block. Without it,
            // the block revision 5 would still be used.
            //
            // @see hole::backends::fs::MutableBlock::derives()
            // XXX this should be done once the host is authenticated.
            ELLE_LOG_COMPONENT("infinit.hole.slug.cache");
            ELLE_TRACE("cleaning the cache");
            cache.clear();
          }
#endif

          // Depending on the machine's state.
          switch (this->_state)
          {
            case State::attached:
              {
                // FIXME: handling via loci is very wrong. IPs are
                // not uniques, and this reconstruction is lame and
                // non-injective.
                using elle::utility::move_on_copy;
                auto locus = socket->remote_locus();
                move_on_copy<std::unique_ptr<reactor::network::Socket>>
                  msocket(std::move(socket));
                auto auth_fn = [&, msocket, locus]
                {
                  this->_connect(std::move(msocket.value), locus, false);
                };
                scope.run_background(elle::sprintf("auth %s", locus), auth_fn);
                break;
              }
            default:
              {
                // FIXME: Why not listening only when we're attached ?
                ELLE_TRACE("not attached, ignore %s", socket->remote_locus());
                break;
              }
          }
        }
      }

      /*-------.
      | Portal |
      `-------*/

      void
      Slug::portal_connect(std::string const& host, int port)
      {
        ELLE_TRACE_FUNCTION(host, port);
        this->_server->accept(host, port);
      }

      bool
      Slug::portal_wait(std::string const& host, int port)
      {
        ELLE_TRACE_FUNCTION(host, port);

        elle::network::Locus locus{host, port};
        ELLE_TRACE("checking if the host '%s' is present and has been "
                   "authenticated", locus);

        // Wait for host to be in the list
        ELLE_DEBUG("active wait for %s", locus)
        {
          auto& sched = *reactor::Scheduler::scheduler();
          auto& thread = *sched.current();
          for (unsigned int max_tries = 10;
               max_tries != 0;
               max_tries--)
          {
            thread.wait(this->_new_host, boost::posix_time::seconds(1));
            ELLE_DEBUG("(%s) resume from wait. Hosts: %s ", locus, _hosts);
            auto it = _hosts.find(locus);
            if (it != end(_hosts))
            {
              // Check if this is the second host with the same passport
              elle::Passport const& pass = it->second->remote_passport();

              ELLE_DEBUG("passport: %s", pass);

              // We compare each passport with the one of the host.
              // If there is only one host with this passport, n shall be 1
              int n = 0;
              for (auto const& p: _hosts)
              {
                ELLE_DEBUG("value of n: %d", n);
                if (p.second->remote_passport() == pass)
                  n++;
                // Stop iteration if we know that the host is forbidden.
                if (n > 1)
                  goto error;
              }
              ELLE_DEBUG("%s found", locus);
              return true;
            }
            else
            {
              ELLE_DEBUG("not yet..");
            }
          }
        }
        error:
        ELLE_DEBUG("out of portal_wait(%s, %s) (%s)",
                   host, port, this->_hosts.size())
          this->_hosts.erase(locus);
        return false;
      }

      /*----------.
      | Printable |
      `----------*/

      void
      Slug::print(std::ostream& stream) const
      {
        stream << "Slug(";
        if (this->_server)
          stream << this->_server->port();
        stream << ")";
      }

      /*---------.
      | Dumpable |
      `---------*/

      elle::Status
      Slug::Dump(const elle::Natural32 margin) const
      {
        elle::String    alignment(margin, ' ');

        std::cout << alignment << "[Machine]" << std::endl;
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[State] " << this->_state << std::endl;
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Port] " << std::dec << this->_port << std::endl;

        return elle::Status::Ok;
      }

      /*------.
      | State |
      `------*/

      std::ostream&
      operator << (std::ostream& stream, Slug::State state)
      {
        switch (state)
        {
          case Slug::State::detached:
            stream << "detached";
            break;
          case Slug::State::attached:
            stream << "attached";
            break;
        }
        return stream;
      }
    }
  }
}
