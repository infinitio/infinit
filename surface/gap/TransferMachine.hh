#ifndef TRANSFERMACHINE_HH
# define TRANSFERMACHINE_HH

# include <surface/gap/usings.hh>

# include <plasma/meta/Client.hh>

# include <papier/fwd.hh>

# include <etoile/fwd.hh>

# include <hole/storage/Directory.hh>
# include <hole/implementations/slug/Slug.hh>

# include <nucleus/proton/Network.hh>

# include <reactor/fsm.hh>
# include <reactor/network/Protocol.hh>
# include <reactor/scheduler.hh>
# include <reactor/thread.hh>
# include <reactor/waitable.hh>

# include <elle/Printable.hh>

# include <thread>

namespace surface
{
  namespace gap
  {
    class State;

    class TransferMachine:
      public elle::Printable
    {
      //---------- Signal ------------------------------------------------------
      /// Allow to check if the signal has been pre triggered.
      /// Notice that the pre trigger evaluation reset the state.
    public:
      class Signal
      {
      public:
        Signal(std::string const& name = std::string());
        bool signal();
        bool signal_one();

        operator reactor::Signal* ();

        ELLE_ATTRIBUTE_R(reactor::Signal, signal);
        ELLE_ATTRIBUTE_r(bool, signaled);

        bool
        signaled();
      };


    public:
      TransferMachine(surface::gap::State const& state);

      virtual
      ~TransferMachine();

    public:
      void
      run(reactor::fsm::State& initial_state);

      virtual
      void
      on_transaction_update(plasma::Transaction const& transaction) = 0;

      virtual
      void
      on_peer_connection_update(PeerConnectionUpdateNotification const& notif) = 0;

      void
      cancel();

    public:
      bool
      concerns_network(std::string const& network_id);

      bool
      concerns_transaction(std::string const& transaction_id);

      bool
      concerns_user(std::string const& user_id);

    protected:
      void
      _stop();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
    private:
      ELLE_ATTRIBUTE(reactor::Scheduler, scheduler);
      ELLE_ATTRIBUTE(std::unique_ptr<std::thread>, scheduler_thread);

    protected:
      reactor::fsm::Machine _machine;
      ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, machine_thread);

    protected:
      Signal _canceled;

    private:
      /*-------------.
      | Core Machine |
      `-------------*/
      void
      _transfer_core();

    protected:
      reactor::fsm::State& _transfer_core_state;

    private:
      reactor::fsm::Machine _core_machine;

      void
      _publish_interfaces();

      void
      _connection();

      void
      _wait_for_peer();

      void
      _transfer();

      virtual
      void
      _transfer_operation() = 0;

    protected:
      // Common on both sender and recipient process.
      ELLE_ATTRIBUTE(reactor::fsm::State&, publish_interfaces_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, connection_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_peer_state); // Broken: I you disconnect, which is quasi transparent, you must publish your interfaces.

    protected:
      // This state has to be protected to allow the children to start the
      // machine in this state.
      reactor::fsm::State& _transfer_state;

    protected:
      // User status signal.
      Signal _peer_online;
      Signal _peer_offline;

      // Slug?
      Signal _peer_connected;
      Signal _peer_disconnected;

      ELLE_ATTRIBUTE_R(surface::gap::State const&, state);

      /*------------.
      | Transaction |
      `------------*/
      ELLE_ATTRIBUTE(std::string, transaction_id);
    protected:
      std::string const&
      transaction_id() const;

      void
      transaction_id(std::string const& id);

    protected:
      ELLE_ATTRIBUTE(std::string, peer_id);

    public:
      std::string const&
      peer_id() const;

    protected:
      void
      peer_id(std::string const& id);

    public:
      bool
      is_sender();

      /*--------.
      | Network |
      `--------*/
      ELLE_ATTRIBUTE(std::string, network_id);
    protected:
      std::string const&
      network_id() const;

      void
      network_id(std::string const& id);

      ELLE_ATTRIBUTE(std::unique_ptr<nucleus::proton::Network>, network);
    protected:
      nucleus::proton::Network&
      network();

      ELLE_ATTRIBUTE(std::unique_ptr<papier::Descriptor>, descriptor);
    protected:
      papier::Descriptor const&
      descriptor();

      /*-------.
      | Etoile |
      `-------*/
      ELLE_ATTRIBUTE(std::unique_ptr<hole::storage::Directory>, storage);
    protected:
      hole::storage::Directory&
      storage();

      ELLE_ATTRIBUTE(std::unique_ptr<hole::implementations::slug::Slug>, hole);
    protected:
      hole::implementations::slug::Slug&
      hole();

      ELLE_ATTRIBUTE(std::unique_ptr<etoile::Etoile>, etoile);
    protected:
      etoile::Etoile&
      etoile();

    public:

      virtual
      std::string
      type() const;

      /*----------.
      | Printable |
      `----------*/
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
