#include <hole/Hole.hh>
#include <hole/implementations/remote/Client.hh>
#include <hole/implementations/remote/Implementation.hh>
#include <hole/implementations/remote/Machine.hh>
#include <hole/implementations/remote/Remote.hh>
#include <hole/Exception.hh>

namespace hole
{
  namespace implementations
  {
    namespace remote
    {
      /*-------------.
      | Construction |
      `-------------*/

      Implementation::Implementation(hole::storage::Storage& storage,
                                     papier::Passport const& passport,
                                     papier::Authority const& authority,
                                     elle::network::Locus const& server):
        Hole(storage, passport, authority),
        _server_locus(server)
      {
        Remote::Computer = new Machine(*this);
        Remote::Computer->Launch();
      }

      Implementation::~Implementation()
      {
        delete Remote::Computer;
      }

      /*-----.
      | Hole |
      `-----*/

      void
      Implementation::_push(const nucleus::proton::Address& address,
                            const nucleus::proton::ImmutableBlock& block)
      {
        if (Remote::Computer->role != Machine::RoleClient)
          throw Exception("the hole is not acting as a remote client as it should");
        Remote::Computer->client->put(address, block);
      }

      void
      Implementation::_push(const nucleus::proton::Address& address,
                          const nucleus::proton::MutableBlock& block)
      {
        if (Remote::Computer->role != Machine::RoleClient)
          throw Exception("the hole is not acting as a remote client as it should");
        Remote::Computer->client->put(address, block);
      }

      std::unique_ptr<nucleus::proton::Block>
      Implementation::_pull(const nucleus::proton::Address& address)
      {
        if (Remote::Computer->role != Machine::RoleClient)
          throw Exception("the hole is not acting as a remote client as it should");
        return Remote::Computer->client->get(address);
      }

      std::unique_ptr<nucleus::proton::Block>
      Implementation::_pull(const nucleus::proton::Address& address,
                          const nucleus::proton::Revision& revision)
      {
        if (Remote::Computer->role != Machine::RoleClient)
          throw Exception("the hole is not acting as a remote client as it should");
        return Remote::Computer->client->get(address, revision);
      }

      void
      Implementation::_wipe(const nucleus::proton::Address& address)
      {
        if (Remote::Computer->role != Machine::RoleClient)
          throw Exception("the hole is not acting as a remote client as it should");
        Remote::Computer->client->kill(address);
      }

      /*---------.
      | Dumpable |
      `---------*/

      elle::Status
      Implementation::Dump(const elle::Natural32 margin) const
      {
        elle::String    alignment(margin, ' ');

        std::cout << alignment << "[Implementation] Remote" << std::endl;

        // dump the machine.
        if (Remote::Computer->Dump(margin + 2) == elle::Status::Error)
          throw Exception("unable to dump the machine");

        return elle::Status::Ok;
      }
    }
  }
}
