#ifndef SURFACE_GAP_ONBOARDING_TRANSFERMACHINE_HH
# define SURFACE_GAP_ONBOARDING_TRANSFERMACHINE_HH

# include <elle/attribute.hh>

# include <reactor/duration.hh>
# include <reactor/Barrier.hh>

# include <surface/gap/TransferMachine.hh>

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      class TransferMachine:
        public surface::gap::Transferer
      {
      public:
        TransferMachine(TransactionMachine& owner,
                        reactor::Duration duration = 5_sec);

        void
        _publish_interfaces() override;

        void
        _connection() override;

        void
        _wait_for_peer() override;

        void
        _transfer() override;

        void
        _stopped() override;

        float
        progress() const override;

        bool
        finished() const override;

        void
        interrupt();

        ELLE_ATTRIBUTE(float, progress);
        ELLE_ATTRIBUTE(reactor::Duration, duration);
        ELLE_ATTRIBUTE_X(reactor::Barrier, running);
        ELLE_ATTRIBUTE(bool, interrupt);
      };
    }
  }
}

#endif
