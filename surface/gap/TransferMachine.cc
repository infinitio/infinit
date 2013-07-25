#include "TransferMachine.hh"

#include <surface/gap/State.hh>
#include <lune/Descriptor.hh>

#include <Infinit.hh>

#include <common/common.hh>

#include <reactor/fsm/Machine.hh>

#include <elle/printf.hh>

#include <functional>

ELLE_LOG_COMPONENT("surface.gap.TransferMachine");

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
    {
      ELLE_TRACE_SCOPE("%s: creating transfer machine", *this);
    }

    TransferMachine::~TransferMachine()
    {
      ELLE_TRACE_SCOPE("%s: destroying transfer machine", *this);
    }

    void
    TransferMachine::_stop()
    {
      ELLE_TRACE_SCOPE("%s: stop machine for transaction %s",
                       *this, this->_network_id);

      ELLE_ASSERT(this->_scheduler_thread != nullptr);

      this->_scheduler.mt_run<void>(
        elle::sprintf("stop(%s)", this->_network_id),
        [this]
        {
          ELLE_DEBUG("terminate all threads")
            this->_scheduler.terminate_now();
          ELLE_DEBUG("finalize etoile")
            this->_etoile.reset();
          ELLE_DEBUG("finalize hole")
            this->_hole.reset();
        });
      this->_scheduler_thread->join();

      this->_scheduler_thread.release();
    }

    void
    TransferMachine::run()
    {
      ELLE_TRACE_SCOPE("%s: running transfer machine", *this);
      ELLE_ASSERT(this->_scheduler_thread == nullptr);

      this->_machine_thread.reset(
        new reactor::Thread{
          this->_scheduler,
          "run",
          [&] { this->_machine.run(); }});

      this->_scheduler_thread.reset(
        new std::thread{
          [&]
          {
            try
            {
              this->_scheduler.run();
            }
            catch (...)
            {
              ELLE_ERR("scheduling of network(%s) failed. Storing exception: %s",
                       this->_network_id, elle::exception_string());
              // this->exception = std::current_exception();
            }
          }
        });
    }

    std::string const&
    TransferMachine::transaction_id() const
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
    TransferMachine::network_id() const
    {
      ELLE_ASSERT_GT(this->_network_id.length(), 0u);
      return this->_network_id;
    }

    void
    TransferMachine::network_id(std::string const& id)
    {
      this->_network_id = id;
    }

    std::string const&
    TransferMachine::peer_id() const
    {
      ELLE_ASSERT_GT(this->_peer_id.length(), 0u);
      return this->_peer_id;
    }

    void
    TransferMachine::peer_id(std::string const& id)
    {
      this->_peer_id = id;
    }

    std::vector<std::string>
    TransferMachine::peers() const
    {
      return {this->state().me().id, this->peer_id()};
    }

    bool
    TransferMachine::is_sender(std::string const& user_id)
    {
      return this->_state.me().id == user_id;
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
      ELLE_TRACE_SCOPE("%s: get descriptor", *this);
      if (!this->_descriptor)
      {
        ELLE_DEBUG_SCOPE("building descriptor");
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
      ELLE_TRACE_SCOPE("%s: get storage", *this);
      if (!this->_storage)
      {
        ELLE_DEBUG_SCOPE("building storage");
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
      ELLE_TRACE_SCOPE("%s: get hole", *this);
      if (!this->_hole)
      {
        ELLE_DEBUG_SCOPE("building hole");
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
      ELLE_TRACE_SCOPE("%s: get etoile", *this);
      if (!this->_etoile)
      {
        ELLE_DEBUG_SCOPE("building descriptor");
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
