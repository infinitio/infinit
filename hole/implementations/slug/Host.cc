#include <memory>

#include <elle/serialize/VectorSerializer.hxx>
#include <elle/utility/Time.hh>

#include <reactor/network/exception.hh>

#include <papier/Passport.hh>

#include <hole/Exception.hh>

#include <nucleus/Derivable.hh>
#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/proton/ImmutableBlock.hh>
#include <nucleus/proton/MutableBlock.hh>

#include <hole/Hole.hh>
#include <hole/implementations/slug/Host.hh>
#include <hole/implementations/slug/Manifest.hh>
#include <hole/implementations/slug/Slug.hh>

ELLE_LOG_COMPONENT("infinit.hole.slug.Host");

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      /*-------------.
      | Construction |
      `-------------*/

      Host::Host(Slug& slug,
                 elle::network::Locus const& locus,
                 std::unique_ptr<reactor::network::Socket> socket)
        : _slug(slug)
        , _locus(locus)
        , _state(State::connected)
        , _authenticated(false)
        , _remote_passport(nullptr)
        , _socket(std::move(socket))
        , _serializer(*reactor::Scheduler::scheduler(), *_socket)
        , _channels(*reactor::Scheduler::scheduler(), _serializer)
        , _rpcs(_channels)
        , _rpcs_handler(new reactor::Thread(*reactor::Scheduler::scheduler(),
                                            elle::sprintf("RPC %s", *this),
                                            boost::bind(&Host::_rpc_run, this),
                                            true))
      {
        _rpcs.authenticate = boost::bind(&Host::_authenticate, this, _1);
        _rpcs.push = boost::bind(&Host::_push, this, _1, _2);
        _rpcs.pull = boost::bind(&Host::_pull, this, _1, _2);
        _rpcs.wipe = boost::bind(&Host::_wipe, this, _1);
      }

      Host::~Host()
      {
        // Stop operations on the socket before it is deleted.
        // Check if we are not committing suicide.
        ELLE_TRACE_SCOPE("%s: finalize", *this);
        auto sched = reactor::Scheduler::scheduler();
        if (sched != nullptr)
        {
          auto current = sched->current();
          if (this->_rpcs_handler &&
              !_rpcs_handler->done() &&
              current != _rpcs_handler)
          {
            _rpcs_handler->terminate_now();
          }
        }
        try
        {
          delete this->_socket.release();
        }
        catch (elle::Exception const& e)
        {
          // If we have an error cleaning up the socket - namely, the latest
          // bytes couldn't be sent, just ignored it since the host is being
          // removed anyway.
          ELLE_WARN("%s: socket cleanup error ignored: %s", *this, e.what());
        }
      }

      /*-----.
      | RPCs |
      `-----*/

      void
      Host::_rpc_run()
      {
        auto fn_on_exit = [&] {
          ELLE_TRACE("%s: left", *this);
          this->_rpcs_handler = nullptr;
          this->_slug._remove(this);
        };
        elle::Finally on_exit(std::move(fn_on_exit));

        try
        {
          this->_rpcs.run();
        }
        catch (reactor::network::Exception& e)
        {
          ELLE_WARN("%s: discarded: %s", *this, e.what());
        }
      }

      /*----.
      | API |
      `----*/

      void
      Host::push(nucleus::proton::Address const& address,
                 nucleus::proton::Block const& block)
      {
        ELLE_TRACE_SCOPE("%s: push block at address %s", *this, address);
        nucleus::Derivable derivable(address.component(), block);
        _rpcs.push(address, derivable);
      }

      std::unique_ptr<nucleus::proton::Block>
      Host::pull(nucleus::proton::Address const& address,
                 nucleus::proton::Revision const& revision)
      {
        ELLE_TRACE_SCOPE("%s: pull block at address %s with revision %s", *this, address, revision);
        return _rpcs.pull(address, revision).release();
      }

      void
      Host::wipe(nucleus::proton::Address const& address)
      {
        ELLE_TRACE_SCOPE("%s: wipe address %s", *this, address);
        _rpcs.wipe(address);
      }

      std::vector<elle::network::Locus>
      Host::authenticate(papier::Passport const& passport)
      {
        assert(this->state() == State::connected);
        ELLE_TRACE_SCOPE("%s: authenticate with %s", *this, passport);
        this->_state = State::authenticating;
        auto res = _rpcs.authenticate(passport);
        if (this->_state == State::authenticating)
          this->_state = State::authenticated;
        return (res);
      }

      std::vector<elle::network::Locus>
      Host::_authenticate(papier::Passport const& passport)
      {
        ELLE_TRACE_SCOPE("%s: peer authenticates with %s", *this, passport);
        this->_remote_passport.reset(new papier::Passport(passport));
        if (this->_slug._host_connected(passport))
        {
          ELLE_TRACE("%s: peer is already connected, reject", *this);
          this->_state = State::duplicate;
          throw AlreadyConnected();
        }
        while (true)
        {
          std::shared_ptr<Host> host(this->_slug._host_pending(passport));
          if (!host)
            break;
          else
            ELLE_TRACE("%s: already negociating with this peer", *this);
          auto hash = std::hash<papier::Passport>()(this->_slug.passport());
          auto remote_hash = std::hash<papier::Passport>()
            (*host->_remote_passport);
          ELLE_ASSERT_NEQ(hash, remote_hash);
          if (hash < remote_hash)
          {
            ELLE_TRACE_SCOPE("%s: we are master, wait", *this);
            {
              if (this->_slug._host_wait(host))
              {
                ELLE_TRACE("%s: previous negociation succeeded, reject",
                           *this);
                this->_state = State::duplicate;
                throw AlreadyConnected();
              }
              else
                ELLE_TRACE("%s: previous negociation failed, carry on",
                           *this);
            }
          }
          else
          {
            ELLE_TRACE_SCOPE("%s: peer is master, just carry on", *this);
            break;
          }
        }
        if (!passport.validate(this->_slug.authority()))
          throw Exception("unable to validate the passport");
        else
        {
          this->_authenticated = true;
          this->_authenticated_signal.signal();
        }
        // Also authenticate to this host if we're not already doing so.
        if (this->_state == State::connected)
          this->authenticate(this->_slug.passport());
        // If we're authenticated, validate this host.
        if (this->_state == State::authenticated)
          this->_slug._host_register(this->shared_from_this());
        // Send back all the hosts we know.
        return this->_slug.loci();
      }

      void
      Host::_push(nucleus::proton::Address const& address,
                  nucleus::Derivable& derivable)
      {
        ELLE_TRACE_SCOPE("%s: peer pushes block at address %s", *this, address);

        if (this->_state != State::authenticated)
          throw Exception("unable to process a request from an unauthenticated host");

        std::unique_ptr<nucleus::proton::Block> block = derivable.release();
        // Forward the request depending on the nature of the block
        // which the address indicates.
        switch (address.family())
          {
          case nucleus::proton::Family::content_hash_block:
            {
              ELLE_DEBUG("%s: block is immutable", *this);

              assert(dynamic_cast<nucleus::proton::ImmutableBlock*>(
                       block.get()));
              nucleus::proton::ImmutableBlock const& ib =
                static_cast<nucleus::proton::ImmutableBlock const&>(*block);

              this->_slug.storage().store(address, ib);

              break;
            }
          case nucleus::proton::Family::public_key_block:
          case nucleus::proton::Family::owner_key_block:
          case nucleus::proton::Family::imprint_block:
            {
              assert(dynamic_cast<nucleus::proton::MutableBlock*>(block.get()));
              nucleus::proton::MutableBlock const* mb =
                static_cast<nucleus::proton::MutableBlock const*>(block.get());

              ELLE_DEBUG("%s: block is mutable", *this);
              {
                // Validate the block, depending on its component.
                // Indeed, the Object component requires as additional
                // block for being validated.
                switch (address.component())
                  {
                    case nucleus::neutron::ComponentObject:
                    {
                      /* XXX[need to change the way validation works by relying
                             on a callback]
                      assert(dynamic_cast<nucleus::neutron::Object const*>(mb));
                      nucleus::neutron::Object const* object =
                        static_cast<nucleus::neutron::Object const*>(mb);

                      ELLE_DEBUG("%s: validate the object block", *this);

                      // Validate the object according to the presence
                      // of a referenced access block.
                      if (object->access() != nucleus::proton::Address::null())
                        {
                          // Load the access block.
                          std::unique_ptr<nucleus::proton::Block> block
                            (this->_slug.pull(object->access(),
                                                   nucleus::proton::Revision::Last));
                          std::unique_ptr<nucleus::neutron::Access> access
                            (dynamic_cast<nucleus::neutron::Access*>(block.release()));
                          if (access == nullptr)
                            throw Exception("expected an access block");

                          ELLE_DEBUG("%s: retrieve the access block", *this);

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
                      ELLE_TRACE("%s: validate the mutable block", *this);

                      // Validate the block through the common interface.
                      mb->validate(address);

                      break;
                    }
                  case nucleus::neutron::ComponentUnknown:
                    {
                      throw Exception(elle::sprintf("unknown component '%u'",
                                                             address.component()));
                    }
                  }

                this->_slug.storage().store(address, *mb);
              }
              break;
            }
          default:
            {
              throw Exception("unknown block family");
            }
          }
      }

      nucleus::Derivable
      Host::_pull(nucleus::proton::Address const& address,
                  nucleus::proton::Revision const& revision)
      {
        ELLE_TRACE_SCOPE("%s: peer retrieves block at address %s for revision %s",
                         *this, address, revision);

        using nucleus::proton::Block;
        using nucleus::proton::ImmutableBlock;
        using nucleus::proton::MutableBlock;

        if (this->_state != State::authenticated)
          throw Exception("unable to process a request from an unauthenticated host");

        std::unique_ptr<Block> block;

        // forward the request depending on the nature of the block which
        // the addres indicates.
        switch (address.family())
          {
            case nucleus::proton::Family::content_hash_block:
            {
              ELLE_DEBUG("%s: block is immutable", *this);

              block = this->_slug.storage().load(address);

              // validate the block.
              block->validate(address);

              break;
            }
            case nucleus::proton::Family::public_key_block:
            case nucleus::proton::Family::owner_key_block:
            case nucleus::proton::Family::imprint_block:
            {
              ELLE_TRACE("%s: block is mutable", *this);

              // Load block.
              block = this->_slug.storage().load(address, revision);

              // validate the block, depending on its component.
              //
              // indeed, the Object component requires as additional
              // block for being validated.
              switch (address.component())
              {
                case nucleus::neutron::ComponentObject:
                {
                  /* XXX[need to change the way validation works by relying
                         on a callback]
                  const nucleus::neutron::Object* object =
                    static_cast<const nucleus::neutron::Object*>(block.get());
                  assert(dynamic_cast<const nucleus::neutron::Object*>(
                           block.get()) != nullptr);

                  // validate the object according to the presence of
                  // a referenced access block.
                  if (object->access() !=
                      nucleus::proton::Address::null())
                  {
                    // Load the access block.
                    std::unique_ptr<Block> addressBlock
                      (this->_slug.storage().load(object->access(),
                                                            nucleus::proton::Revision::Last));

                    // Get address block.
                    nucleus::neutron::Access *access =
                      dynamic_cast<nucleus::neutron::Access *> (addressBlock.get());

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
              break;
            }
            default:
            {
              throw Exception("unknown block family");
            }
          }

        return nucleus::Derivable(address.component(),
                                  *block.release(),
                                  nucleus::Derivable::Kind::input,
                                  true);
      }

      void
      Host::_wipe(nucleus::proton::Address const& address)
      {
        ELLE_TRACE_SCOPE("%s: peer wipes block at address %s", *this, address);

        // check the host's state.
        if (this->_state != State::authenticated)
          throw Exception("unable to process a request from an unauthenticated host");

        //
        // remove the block locally.
        //
        {
          // treat the request depending on the nature of the block which
          // the addres indicates.
          switch (address.family())
            {
            case nucleus::proton::Family::content_hash_block:
              {
                this->_slug.storage().erase(address);

                break;
              }
            case nucleus::proton::Family::public_key_block:
            case nucleus::proton::Family::owner_key_block:
            case nucleus::proton::Family::imprint_block:
              {
                this->_slug.storage().erase(address);

                break;
              }
            default:
              {
                throw Exception("unknown block family");
              }
            }
        }
      }

      /*---------.
      | Dumpable |
      `---------*/

      static std::ostream&
      operator << (std::ostream& stream, Host::State state)
      {
        switch (state)
          {
            case Host::State::connected:
              stream << "connected";
              break;
            case Host::State::authenticating:
              stream << "authenticating";
              break;
            case Host::State::authenticated:
              stream << "authenticated";
              break;
            case Host::State::duplicate:
              stream << "duplicate";
              break;
          }
        return stream;
      }

      elle::Status      Host::Dump(elle::Natural32      margin) const
      {
        elle::String    alignment(margin, ' ');
        std::cout << alignment << "[Host] " << std::hex << this << std::endl
                  << alignment << elle::io::Dumpable::Shift
                  << "[State] " << std::dec << this->_state << std::endl;
        return elle::Status::Ok;
      }

      void
      Host::print(std::ostream& stream) const
      {
        stream << "Host(" << _locus << ")";
      }

      std::ostream&
      operator << (std::ostream& stream, const Host& host)
      {
        host.print(stream);
        return stream;
      }
    }
  }
}
