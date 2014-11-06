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

      void
      accept() override;

      void
      _accept() override;

      virtual
      void
      _wait_for_cloud_upload();

      void
      transaction_status_update(
        infinit::oracles::Transaction::Status status) override;

    protected:
      aws::Credentials
      _aws_credentials(bool regenerate) override;

      void
      _finalize(infinit::oracles::Transaction::Status) override;

      void
      cleanup() override;

    private:
      void
      _run_from_snapshot();

      reactor::Barrier _cloud_uploaded;
      reactor::fsm::State& _wait_for_cloud_upload_state;
      ELLE_ATTRIBUTE_R(boost::filesystem::path, snapshot_path);
      ELLE_ATTRIBUTE_R(std::unique_ptr<reactor::http::Request>, request);
      ELLE_ATTRIBUTE_R(int64_t, previous_progress);
      ELLE_ATTRIBUTE_R(boost::filesystem::path, path);
    };
  }
}



#endif
