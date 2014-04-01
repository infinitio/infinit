#ifndef SURFACE_GAP_ONBOARDING_TRANSACTION_HH
# define SURFACE_GAP_ONBOARDING_TRANSACTION_HH

# include <surface/gap/State.hh>
# include <surface/gap/Transaction.hh>

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

        void
        join() override;

        float
        progress() const override;

        ELLE_ATTRIBUTE(float, progress);
        ELLE_ATTRIBUTE(reactor::Duration, duration);
        ELLE_ATTRIBUTE(reactor::Barrier, accepted);
        ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, thread);

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
