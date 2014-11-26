#ifndef SURFACE_GAP_ONBOARDING_RECEIVEMACHINE_HH
# define SURFACE_GAP_ONBOARDING_RECEIVEMACHINE_HH

# include <aws/Credentials.hh>

# include <elle/attribute.hh>

# include <surface/gap/ReceiveMachine.hh>
# include <surface/gap/PeerMachine.hh>
# include <surface/gap/onboarding/fwd.hh>

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      class ReceiveMachine
        : public surface::gap::ReceiveMachine
        , public surface::gap::PeerMachine
      {
      public:
        typedef infinit::oracles::PeerTransaction Data;

      public:
        ReceiveMachine(Transaction& transaction,
                       uint32_t id,
                       std::shared_ptr<Data> data,
                       std::string const& file_path,
                       reactor::Duration duration = 5_sec);

        ~ReceiveMachine();

        virtual
        float
        progress() const override;

        // XXX: not all transactions will need AWS credentials.
        virtual
        aws::Credentials
        _aws_credentials(bool regenerate) override;

        virtual
        void
        _save_snapshot() const override;

        virtual
        void
        _transfer_operation(frete::RPCFrete& frete) override;

        virtual
        void
        _cloud_synchronize() override;

        virtual
        void
        _cloud_operation() override;

        virtual
        void
        cleanup() override;

        virtual
        void
        accept() override;

        virtual
        void
        reject() override;

        virtual
        std::unique_ptr<frete::RPCFrete>
        rpcs(infinit::protocol::ChanneledStream& socket) override;

        // Overload because it talks to meta.
        void
        _accept() override;

        // Overload because it talks to meta.
        void
        _finalize(infinit::oracles::Transaction::Status) override;

        ELLE_ATTRIBUTE(std::string, file_path);

      /*--------.
      | Metrics |
      `--------*/
      protected:
      virtual
      void
      _metrics_ended(infinit::oracles::Transaction::Status status,
                     std::string reason = "") override;
      };
    }
  }
}

#endif
