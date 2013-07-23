#ifndef SENDMACHINE_HH
# define SENDMACHINE_HH

# include <surface/gap/TransferMachine.hh>
# include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
    class  SendMachine:
      public TransferMachine
    {
    public:
      SendMachine(surface::gap::State const& state,
                  std::string const& recipient,
                  std::unordered_set<std::string>&& files);

      virtual
      ~SendMachine();

    public:
      void
      on_transaction_update(plasma::meta::TransactionResponse const& transaction);

      void
      on_user_update(plasma::meta::UserResponse const& user);

      void
      on_network_update(plasma::meta::NetworkResponse const& network);

    private:
      SendMachine(surface::gap::State const& state);

    private:
      void
      _request_network();

      void
      _create_transaction();

      void
      _copy_files();

      void
      _wait_for_accept();

      void
      _set_permissions();

      void
      _publish_interfaces();

      void
      _connection();

      void
      _transfer();

      void
      _clean();

      void
      _fail();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, request_network_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, create_transaction_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, copy_files_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_accept_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, set_permissions_state);
      // Common on both sender and recipient process, could be put in base class.
      ELLE_ATTRIBUTE(reactor::fsm::State&, publish_interfaces_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, connection_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, transfer_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, clean_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, fail_state);

      // User status signal.
      ELLE_ATTRIBUTE(reactor::Signal, peer_online);
      ELLE_ATTRIBUTE(reactor::Signal, peer_offline);

      // Slug signal.
      ELLE_ATTRIBUTE(reactor::Signal, peer_connected);
      ELLE_ATTRIBUTE(reactor::Signal, peer_disconnected);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Signal, accepted);
      ELLE_ATTRIBUTE(reactor::Signal, finished);
      ELLE_ATTRIBUTE(reactor::Signal, canceled);
      ELLE_ATTRIBUTE(reactor::Signal, failed);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);
    };
  }
}

#endif
