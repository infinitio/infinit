#ifndef RECEIVEMACHINE_HH
# define RECEIVEMACHINE_HH

# include <memory>
# include <string>
# include <unordered_set>

# include <boost/filesystem.hpp>

# include <elle/attribute.hh>

# include <reactor/waitable.hh>
# include <reactor/signal.hh>

# include <frete/TransferSnapshot.hh>

# include <surface/gap/State.hh>
# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/TransferBufferer.hh>

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
      _transfer_operation(frete::RPCFrete& frete) override;

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
      ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_path);
      ELLE_ATTRIBUTE_R(std::unique_ptr<frete::TransferSnapshot>, snapshot)
    private:
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& channels) override;

    public:
      float
      progress() const override;

    public:
      virtual
      bool
      is_sender() const override
      {
        return false;
      }

    /*----------.
    | Printable |
    `----------*/
    public:
      std::string
      type() const override;

    /*---------.
    | Transfer |
    `---------*/
    public:
      void
      get(frete::RPCFrete& frete,
          std::string const& name_policy = " (%s)");
      void
      get(TransferBufferer& bufferer,
          std::string const& name_policy = " (%s)");

    private:
      template <typename Source>
      void
      _get(Source& source,
           bool strong_encryption,
           std::string const& name_policy,
           elle::Version const& peer_version);
      template <typename Source>
      void
      _finish_transfer(Source& source,
                       unsigned int index,
                       frete::TransferSnapshot::TransferProgressInfo& tr,
                       int chunk_size,
                       const boost::filesystem::path& full_path,
                       bool strong_encryption,
                       infinit::cryptography::SecretKey const& key,
                       elle::Version const& peer_version);

    /*--------------.
    | Static Method |
    `--------------*/
    public:
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
