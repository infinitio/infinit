#ifndef SURFACE_GAP_RECEIVE_MACHINE_HH
# define SURFACE_GAP_RECEIVE_MACHINE_HH

# include <boost/filesystem.hpp>
# include <boost/filesystem/fstream.hpp>

# include <surface/gap/State.hh>
# include <surface/gap/TransactionMachine.hh>
# include <infinit/oracles/Transaction.hh>

namespace surface
{
  namespace gap
  {
    class ReceiveMachine:
      virtual public TransactionMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef infinit::oracles::Transaction Data;
      typedef ReceiveMachine Self;
      typedef TransactionMachine Super;
      typedef surface::gap::Transaction Transaction;

    /*-------------.
    | Construction |
    `-------------*/
    protected:
      ReceiveMachine(Transaction& transaction,
                     uint32_t id,
                     std::shared_ptr<Data> data);
    public:
      virtual
      ~ReceiveMachine();

    /*-----------------------.
    | Machine implementation |
    `-----------------------*/
    protected:
      reactor::fsm::State& _wait_for_decision_state;
      reactor::fsm::State& _accept_state;

    public:
      virtual
      void
      accept(boost::optional<std::string const&> output_dir = {});
      virtual
      void
      reject();

    private:
      void
      _wait_for_decision();

    protected:
      virtual
      void
      _accept();

    // Transaction status signals.
    protected:
      reactor::Barrier _accepted;
      reactor::Barrier _accepted_elsewhere;
      std::string _output_dir;
    /*-----------------.
    | Transaction data |
    `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);

    public:
      virtual
      float
      progress() const = 0;
      virtual
      bool
      is_sender() const override
      {
        return false;
      }

    protected:
      virtual
      void
      transaction_status_update(
        infinit::oracles::Transaction::Status status) override;

      virtual
      bool
      concerns_this_device() override;

    /*--------------.
    | Static Method |
    `--------------*/
    public:
      /* XXX: Exposed for debugging purposes.
       * root_component_mapping is a list of path names that we already
       * attributed a maping to (either themselves, or renamed)
       * It will be updated according to new mapping mades.
       */
      static
      boost::filesystem::path
      eligible_name(boost::filesystem::path start_point,
                    boost::filesystem::path path,
                    std::string const& name_policy,
                    std::map<boost::filesystem::path, boost::filesystem::path>& root_component_mapping);

      static
      boost::filesystem::path
      trim(boost::filesystem::path const& item,
           boost::filesystem::path const& root);
    };
  }
}

#endif
