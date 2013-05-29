#include <boost/bind.hpp>
#include <boost/function.hpp>

#include <hole/Passport.hh>
#include <hole/Exception.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Revision.hh>

#include <hole/Hole.hh>
#include <hole/implementations/remote/Customer.hh>
#include <hole/implementations/remote/Server.hh>

#include <nucleus/proton/ImmutableBlock.hh>
#include <nucleus/proton/MutableBlock.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Revision.hh>

#include <Infinit.hh>

ELLE_LOG_COMPONENT("infinit.hole.remote.Customer");

namespace hole
{
  namespace implementations
  {
    namespace remote
    {
      const elle::Natural32             Customer::Timeout = 180000;

      /*-------------.
      | Construction |
      `-------------*/

      Customer::Customer(Server& server,
                         std::unique_ptr<reactor::network::Socket> socket)
        : _state(State::connected)
        , _server(server)
        , _socket(socket.release())
        , _serializer(*reactor::Scheduler::scheduler(), *_socket.get())
        , _channels(*reactor::Scheduler::scheduler(), _serializer)
        , _rpcs(_channels)
        , _runner(new reactor::Thread(*reactor::Scheduler::scheduler(),
                                      elle::sprintf("%s RPCs", *this),
                                      boost::bind(&Customer::_run, this),
                                      true))
      {
        _rpcs.challenge = boost::bind(&Customer::challenge, this, _1);
        _rpcs.push = boost::bind(&Customer::push, this, _1, _2);
        _rpcs.pull = boost::bind(&Customer::pull, this, _1, _2);
        _rpcs.wipe = boost::bind(&Customer::wipe, this, _1);
        assert(_socket);
      }

      Customer::~Customer()
      {
        try
          {
            this->_runner->terminate_now();
          }
        catch (reactor::Terminate&)
          {
            // If we are in the customer's thread, we're dying anyway.
          }
      }

      void
      Customer::_run()
      {
        try
          {
            _rpcs.run();
          }
        catch (reactor::Exception& e)
          {
            ELLE_TRACE("%s: %s", *this, e);
            _server._remove(this);
          }
      }

      /*----.
      | API |
      `----*/

      bool
      Customer::challenge(elle::Passport const& passport)
      {
        ELLE_TRACE_SCOPE("%s: challenge", *this);
        if (!passport.validate(this->_server.hole().authority()))
          {
            _server._remove(this);
            return false;
          }
        else
          {
            _state = State::authenticated;
            return true;
          }
      }

      void
      Customer::push(nucleus::proton::Address const& address,
                     nucleus::Derivable const& derivable)
      {
        ELLE_TRACE_SCOPE("%s: push block at %s", *this, address);

        _state_check_authenticated();

        auto const& block = derivable.block();

        // forward the request depending on the nature of the block which
        // the addres indicates.
        switch (address.family())
          {
          case nucleus::proton::Family::content_hash_block:
            {
              nucleus::proton::ImmutableBlock const* ib =
                static_cast<nucleus::proton::ImmutableBlock const*>(&block);
              assert(dynamic_cast<const nucleus::proton::ImmutableBlock*>(
                       &block) != nullptr);
              _server.put(address, *ib);
              break;
            }
          case nucleus::proton::Family::public_key_block:
          case nucleus::proton::Family::owner_key_block:
          case nucleus::proton::Family::imprint_block:
            {
              nucleus::proton::MutableBlock const*  mb =
                static_cast<nucleus::proton::MutableBlock const*>(&block);
              assert(dynamic_cast<nucleus::proton::MutableBlock const*>(&block)
                     != nullptr);
              _server.put(address, *mb);
              break;
            }
          default:
            {
              reactor::Exception("unknown block family");
            }
          }
      }

      nucleus::Derivable
      Customer::pull(const nucleus::proton::Address& address,
                     const nucleus::proton::Revision& revision)
      {
        ELLE_TRACE_SCOPE("%s: pull block at %s, %s", *this, address, revision);
        _state_check_authenticated();

        std::unique_ptr<nucleus::proton::Block> block;

        // Forward the request depending on the nature of the block which
        // the addres indicates.
        switch (address.family())
          {
          case nucleus::proton::Family::content_hash_block:
            {
              block = _server.get(address);

              break;
            }
          case nucleus::proton::Family::public_key_block:
          case nucleus::proton::Family::owner_key_block:
          case nucleus::proton::Family::imprint_block:
            {
              block = _server.get(address, revision);

              break;
            }
          default:
            {
              throw Exception("unknown block family");
            }
          }

        assert(block);
        return nucleus::Derivable(address.component(), *block);
      }

      void
      Customer::wipe(const nucleus::proton::Address& address)
      {
        ELLE_TRACE_SCOPE("Wipe");
        _state_check_authenticated();
        _server.kill(address);
      }

      /*------.
      | State |
      `------*/
      void
      Customer::_state_check_authenticated() const
      {
        if (_state != State::authenticated)
          throw Exception(elle::sprintf("%s has not been authenticated", *this));
      }

      /*---------.
      | Dumpable |
      `---------*/

      elle::Status      Customer::Dump(const elle::Natural32    margin) const
      {
        elle::String    alignment(margin, ' ');
        std::cout << alignment << "[Customer]" << std::endl;
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[State] "
                  << (this->_state == State::authenticated ? "authenticated" : "connected")
                  << std::endl;
        return elle::Status::Ok;
      }

      /*----------------.
      | Pretty printing |
      `----------------*/

      void
      Customer::print(std::ostream& s) const
      {
        s << "Customer"; // FIXME: print remote endpoint
      }

      std::ostream& operator << (std::ostream& s, Customer const& c)
      {
        c.print(s);
        return s;
      }
    }
  }
}
