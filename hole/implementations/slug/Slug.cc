#include <hole/implementations/slug/Slug.hh>
#include <hole/implementations/slug/Machine.hh>
#include <hole/Exception.hh>

#include <nucleus/proton/Block.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {

//
// ---------- constructors & destructors --------------------------------------
//

      ///
      /// default constructor.
      ///
      Slug::Slug(
        hole::storage::Storage& storage,
        elle::Passport const& passport,
        elle::Authority const& authority,
        reactor::network::Protocol protocol,
        std::vector<elle::network::Locus> const& members,
        int port,
        reactor::Duration connection_timeout):
        Hole(storage, passport, authority),
        _protocol(protocol),
        _members(members),
        _connection_timeout(connection_timeout),
        _port(port)
      {}

      /*------------.
      | Join, leave |
      `------------*/

      void
      Slug::_join()
      {
        this->_machine.reset(new Machine(*this, this->_port,
                                         this->_connection_timeout));
      }

      void
      Slug::_leave()
      {
        this->_machine.reset(nullptr);
      }

      void
      Slug::_push(const nucleus::proton::Address& address,
                            const nucleus::proton::ImmutableBlock& block)
      {
        this->_machine->put(address, block);
      }

      void
      Slug::_push(const nucleus::proton::Address& address,
                            const nucleus::proton::MutableBlock& block)
      {
        this->_machine->put(address, block);
      }

      std::unique_ptr<nucleus::proton::Block>
      Slug::_pull(const nucleus::proton::Address& address)
      {
        return this->_machine->get(address);
      }

      std::unique_ptr<nucleus::proton::Block>
      Slug::_pull(const nucleus::proton::Address& address,
                            const nucleus::proton::Revision& revision)
      {
        return this->_machine->get(address, revision);
      }

      void
      Slug::_wipe(const nucleus::proton::Address& address)
      {
        this->_machine->wipe(address);
      }

      /*---------.
      | Dumpable |
      `---------*/

      elle::Status
      Slug::Dump(const elle::Natural32 margin) const
      {
        elle::String    alignment(margin, ' ');

        std::cout << alignment << "[Slug]" << std::endl;

        // dump the machine.
        if (this->_machine->Dump(margin + 2) == elle::Status::Error)
          throw Exception("unable to dump the machine");

        return elle::Status::Ok;
      }

    }
  }
}
