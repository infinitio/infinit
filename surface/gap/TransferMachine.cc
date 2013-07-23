#include "TransferMachine.hh"

#include <surface/gap/State.hh>
#include <lune/Descriptor.hh>

#include <Infinit.hh>

#include <common/common.hh>

# include <reactor/fsm/Machine.hh>

#include <functional>

namespace surface
{
  namespace gap
  {
    TransferMachine::TransferMachine(surface::gap::State const& state):
      _scheduler(),
      _scheduler_thread(),
      _machine(),
      _machine_thread(),
      _state(state)
    {}

    TransferMachine::~TransferMachine()
    {}

    void
    TransferMachine::run()
    {
      ELLE_ASSERT(this->_scheduler_thread == nullptr);

      this->_machine_thread.reset(
        new reactor::Thread{
          this->_scheduler,
          "run",
          [&] { this->_machine.run(); }});

      this->_scheduler_thread.reset(
        new std::thread{[&] { this->_scheduler.run(); }});
    }

    std::string const&
    TransferMachine::transaction_id()
    {
      ELLE_ASSERT_GT(this->_transaction_id.length(), 0u);
      return this->_transaction_id;
    }

    void
    TransferMachine::transaction_id(std::string const& id)
    {
      this->_transaction_id = id;
    }

    std::string const&
    TransferMachine::network_id()
    {
      ELLE_ASSERT_GT(this->_network_id.length(), 0u);
      return this->_network_id;
    }

    void
    TransferMachine::network_id(std::string const& id)
    {
      this->_network_id = id;
    }

    nucleus::proton::Network&
    TransferMachine::network()
    {
      if (!this->_network)
      {
        this->_network.reset(
          new nucleus::proton::Network(this->network_id()));
      }

      ELLE_ASSERT(this->_network != nullptr);
      return *this->_network;
    }

    lune::Descriptor const&
    TransferMachine::descriptor()
    {
      if (!this->_descriptor)
      {
        using namespace elle::serialize;
        std::string descriptor = this->state().meta().network(this->network_id()).descriptor; //; // Pull it from networks.

        ELLE_ASSERT_NEQ(descriptor.length(), 0u);

        this->_descriptor.reset(
          new lune::Descriptor(from_string<InputBase64Archive>(descriptor)));
      }
      ELLE_ASSERT(this->_descriptor != nullptr);
      return *this->_descriptor;
    }

    hole::storage::Directory&
    TransferMachine::storage()
    {
      if (!this->_storage)
      {
        this->_storage.reset(
          new hole::storage::Directory(
            this->network().name(),
            common::infinit::network_shelter(this->state().me().id,
                                             this->network().name())));
      }
      ELLE_ASSERT(this->_storage != nullptr);
      return *this->_storage;
    }

    hole::implementations::slug::Slug&
    TransferMachine::hole()
    {
      if (!this->_hole)
      {
        this->_hole.reset(
          new hole::implementations::slug::Slug(
          this->storage(), this->state().passport(), Infinit::authority(),
            reactor::network::Protocol::tcp));
      }
      ELLE_ASSERT(this->_hole != nullptr);
      return *this->_hole;
    }

    etoile::Etoile&
    TransferMachine::etoile()
    {
      if (!this->_etoile)
      {
        this->_etoile.reset(
          new etoile::Etoile(this->state().identity().pair(),
                             &(this->hole()),
                             this->descriptor().meta().root()));
      }
      ELLE_ASSERT(this->_etoile != nullptr);
      return *this->_etoile;
    }
  }
}
