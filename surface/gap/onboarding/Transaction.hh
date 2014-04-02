#ifndef SURFACE_GAP_ONBOARDING_TRANSACTION_HH
# define SURFACE_GAP_ONBOARDING_TRANSACTION_HH

# include <elle/attribute.hh>

# include <reactor/duration.hh>

# include <surface/gap/State.hh>
# include <surface/gap/Transaction.hh>
# include <surface/gap/onboarding/fwd.hh>

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      class Transaction:
        public surface::gap::Transaction
      {
      public:
        Transaction(surface::gap::State const& state,
                    uint32_t id,
                    State::User const& peer,
                    reactor::Duration const& transfer_duration = 5_sec);

        ~Transaction();

        void
        accept() override;

        // In order to give the onboarding the ability to control the process,
        // the onboarding transaction expose the hidden transfer machine api.
        void
        peer_connection_status(bool status);

        void
        peer_availability_status(bool status);

        void
        pause();
        // void
        // join() override;

        // float
        // progress() const override;

        ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, thread);

        surface::gap::onboarding::ReceiveMachine&
        machine();

      /*----------.
      | Printable |
      `----------*/
      public:
        void
        print(std::ostream& stream) const override;
      };
    }
  }
}

#endif
