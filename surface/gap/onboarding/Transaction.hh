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
                    std::string const& file_path,
                    reactor::Duration const& transfer_duration = 5_sec);

        ~Transaction();

        void
        accept() override;

        bool
        pause() override;

        void
        interrupt() override;

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
