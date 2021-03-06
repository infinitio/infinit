#ifndef SURFACE_GAP_GHOST_RECEIVE_MACHINE_HH
# define SURFACE_GAP_GHOST_RECEIVE_MACHINE_HH

# include <boost/filesystem.hpp>
# include <boost/filesystem/fstream.hpp>

# include <surface/gap/State.hh>
# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/ReceiveMachine.hh>
# include <infinit/oracles/Transaction.hh>

# include <reactor/http/Request.hh>

namespace surface
{
  namespace gap
  {
    class GhostReceiveMachine:
      virtual public ReceiveMachine
    {
    public:
      GhostReceiveMachine(Transaction& transaction,
                          uint32_t id,
                          std::shared_ptr<Data> data);

      virtual
      ~GhostReceiveMachine();
      float
      progress() const override;
      virtual
      void
      accept(boost::optional<std::string> output_dir = {}) override;

      virtual
      infinit::oracles::meta::UpdatePeerTransactionResponse
      _accept() override;
      virtual
      void
      _transfer() override;
      virtual
      void
      _wait_for_cloud_upload();
      void
      transaction_status_update(
        infinit::oracles::Transaction::Status status) override;
      virtual
      bool
      completed() const override;

    protected:
      std::unique_ptr<infinit::oracles::meta::CloudCredentials>
      _cloud_credentials(bool regenerate) override;
      virtual
      void
      _update_meta_status(infinit::oracles::Transaction::Status) override;
      void
      cleanup() override;
      virtual
      void _finish() override;
      virtual
      void
      _pause(bool pause_action) override;

    private:
      void
      _run_from_snapshot();

      reactor::Barrier _cloud_uploaded;
      reactor::fsm::State& _wait_for_cloud_upload_state;
      ELLE_ATTRIBUTE_R(boost::filesystem::path, snapshot_path);
      ELLE_ATTRIBUTE_R(std::unique_ptr<reactor::http::Request>, request);
      ELLE_ATTRIBUTE_R(int64_t, previous_progress);
      ELLE_ATTRIBUTE_R(boost::filesystem::path, path);
      ELLE_ATTRIBUTE(bool, completed);
    };
  }
}



#endif
