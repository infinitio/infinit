#ifndef RECEIVEMACHINE_HH
# define RECEIVEMACHINE_HH

# include <memory>
# include <string>
# include <unordered_set>

# include <boost/filesystem.hpp>

# include <elle/attribute.hh>

# include <reactor/waitable.hh>
# include <reactor/signal.hh>

# include <surface/gap/State.hh>
# include <surface/gap/TransactionMachine.hh>

namespace surface
{
  namespace gap
  {
    struct ReceiveMachine:
      public TransactionMachine
    {

    public:
      // Construct from notification.
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     std::shared_ptr<TransactionMachine::Data> data);

      // Construct from snapshot (with current_state).
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     TransactionMachine::State const current_state,
                     std::shared_ptr<TransactionMachine::Data> data);
      virtual
      ~ReceiveMachine();

      virtual
      void
      transaction_status_update(infinit::oracles::Transaction::Status status) override;

    public:
      void
      accept();

      void
      reject();

    private:
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     std::shared_ptr<TransactionMachine::Data> data,
                     bool);

    private:
      void
      _wait_for_decision();

      void
      _accept();

      void
      _transfer_operation(frete::Frete& frete) override;

      void
      _fail();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_decision_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, accept_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Barrier, accepted);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);

    private:
      std::unique_ptr<frete::Frete>
      frete(infinit::protocol::ChanneledStream& socket) override;

    public:
      virtual
      bool
      is_sender() const override
      {
        return false;
      }

    public:
      /*----------.
      | Printable |
      `----------*/

      std::string
      type() const override;

    /*--------------.
    | Static Method |
    `--------------*/
    // XXX: Exposed for debugging purposes.
    static
    boost::filesystem::path
    eligible_name(boost::filesystem::path const path,
                  std::string const& name_policy);

    static
    boost::filesystem::path
    trim(boost::filesystem::path const& item,
         boost::filesystem::path const& root);
    };
  }
}

#endif
