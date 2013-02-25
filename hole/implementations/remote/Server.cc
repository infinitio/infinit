#include <hole/Hole.hh>
#include <hole/implementations/remote/Manifest.hh>
#include <hole/implementations/remote/Server.hh>

#include <elle/log.hh>

#include <reactor/network/tcp-server.hh>

#include <nucleus/proton/ImmutableBlock.hh>
#include <nucleus/proton/MutableBlock.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Network.hh>
#include <nucleus/proton/Revision.hh>
#include <nucleus/neutron/Component.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Access.hh>
#include <nucleus/Derivable.hh>

#include <lune/Lune.hh>

#include <Infinit.hh>
#include <Scheduler.hh>

ELLE_LOG_COMPONENT("infinit.hole.remote.Server");

namespace hole
{
  namespace implementations
  {
    namespace remote
    {
      /*-------------.
      | Construction |
      `-------------*/

      Server::Server(Hole& hole, int port)
        : _hole(hole)
        , _port(port)
        , _server(new reactor::network::TCPServer
                  (infinit::scheduler()))
        , _acceptor(new reactor::Thread(infinit::scheduler(),
                                        "Remote Server accept",
                                        boost::bind(&Server::_accept, this)))
      {
        _server->listen(port);
      }

      Server::~Server()
      {
        this->_acceptor->terminate_now();
        for (Customer* customer: _customers)
          delete customer;
        this->_customers.clear();
      }

      void
      Server::_accept()
      {
        while (true)
          {
            std::unique_ptr<reactor::network::Socket> socket(_server->accept());
            ELLE_TRACE_SCOPE("accept");
            auto result = this->_customers.insert(new Customer(*this, std::move(socket)));
            assert(result.second);
          }
      }

      void
      Server::_remove(Customer* customer)
      {
        auto iterator = _customers.find(customer);
        assert(iterator != _customers.end());
        delete *iterator;
        this->_customers.erase(iterator);
      }

      void
      Server::put(const nucleus::proton::Address& address,
                  const nucleus::proton::ImmutableBlock& block)
      {
        // ELLE_TRACE_SCOPE("%s: put immutable block at %s", *this, address);

        // // does the block already exist.
        // if (this->_hole.storage().exist(address))
        //   throw reactor::Exception("this immutable block seems to already "
        //                            "exist");

        // // store the block.
        // block.store(this->_hole.storage().path(address));
        this->_hole.storage().store(address, block);
      }

      void
      Server::put(const nucleus::proton::Address& address,
                  const nucleus::proton::MutableBlock& block)
      {
        ELLE_TRACE_SCOPE("%s: put mutable block at %s", *this, address);

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

              // Validate the object according to the presence of a
              // referenced access block.
              if (object->access() != nucleus::proton::Address::null())
                {
                  std::unique_ptr<nucleus::proton::Block> block =
                    this->get(object->access());

                  nucleus::neutron::Access * access =
                    dynamic_cast<nucleus::neutron::Access *>(block.get());
                  // Validate the object, providing the access block
                  object->validate(address, access);
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
              block.validate(address);

              break;
            }
          case nucleus::neutron::ComponentUnknown:
            {
              throw reactor::Exception(elle::sprintf("unknown component '%u'",
                                                     address.component()));
            }
          }

        this->_hole.storage().store(address, block);
      }

      std::unique_ptr<nucleus::proton::Block>
      Server::get(const nucleus::proton::Address& address)
      {
        std::unique_ptr<nucleus::proton::Block> block =
          this->_hole.storage().load(address);

        block->validate(address);

        return block;
      }

      std::unique_ptr<nucleus::proton::Block>
      Server::get(const nucleus::proton::Address& address,
                  const nucleus::proton::Revision& revision)
      {
        ELLE_TRACE_SCOPE("%s: get mutable block at %s, %s",
                         *this, address, revision);

        // load the block.
        std::unique_ptr<nucleus::proton::Block> block =
          this->_hole.storage().load(address, revision);


        // validate the block, depending on its component.
        //
        // indeed, the Object component requires as additional block for
        // being validated.
        switch (address.component())
          {
          case nucleus::neutron::ComponentObject:
            {
              /* XXX[need to change the way validation works by relying
                     on a callback]
              const nucleus::neutron::Object* object =
                static_cast<const nucleus::neutron::Object*>(block.get());
              assert(dynamic_cast<const nucleus::neutron::Object*>(block.get()) != nullptr);

              // validate the object according to the presence of
              // a referenced access block.
              if (object->access() != nucleus::proton::Address::null())
                {
                  std::unique_ptr<nucleus::proton::Block> accesBlock =
                    this->get(object->access());

                  nucleus::neutron::Access * access =
                    dynamic_cast<nucleus::neutron::Access *>((accesBlock.get()));

                  // Validate the object, providing the access block
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
              throw reactor::Exception(elle::sprintf("unknown component '%u'",
                                                     address.component()));
            }
          }
        return block;
      }

      void
      Server::kill(const nucleus::proton::Address& address)
      {
        ELLE_TRACE_SCOPE("Kill");

         // FIXME: why a switch if we call the same method in both case.

        switch (address.family())
          {
          case nucleus::proton::Family::content_hash_block:
            {
              // erase the immutable block.
              this->_hole.storage().erase(address);

              break;
            }
          case nucleus::proton::Family::public_key_block:
          case nucleus::proton::Family::owner_key_block:
          case nucleus::proton::Family::imprint_block:
            {
              // erase the immutable block.
              this->_hole.storage().erase(address);

              break;
            }
          default:
            {
              throw reactor::Exception("unknown block family");
            }
          }
      }

      /*----------.
      | Printable |
      `----------*/

       void
       Server::print(std::ostream& stream) const
       {
         stream << "Server " << this->_port;
       }

      /*---------.
      | Dumpable |
      `---------*/

      elle::Status      Server::Dump(const elle::Natural32      margin) const
      {
        elle::String    alignment(margin, ' ');

        std::cout << alignment << "[Server]" << std::endl;

        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Customers] " << this->_customers.size() << std::endl;

        // go though the customer.
        for (Customer* customer: this->_customers)
          if (customer->Dump(margin + 4) == elle::Status::Error)
            throw reactor::Exception("unable to dump the customer");

        return elle::Status::Ok;
      }

    }
  }
}
