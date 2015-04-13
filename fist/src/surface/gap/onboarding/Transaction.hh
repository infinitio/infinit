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
      /*-------------.
      | Construction |
      `-------------*/
      public:
        Transaction(surface::gap::State& state,
                    uint32_t id,
                    State::User const& peer,
                    std::string const& file_path,
                    reactor::Duration const& transfer_duration = 5_sec);
        ~Transaction();
      private:
        ELLE_ATTRIBUTE(std::shared_ptr<infinit::oracles::PeerTransaction>,
                       data);

      /*---------------.
      | Implementation |
      `---------------*/
      public:
        void
        accept() override;
        void
        reset() override;
        surface::gap::onboarding::ReceiveMachine&
        machine();

      private:
        virtual
        void
        _snapshot_save() const override;

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
