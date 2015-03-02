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
                        std::string const& input_file_path,
                        std::string const& output_dir,
                        reactor::Duration duration = 5_sec);

         ~TransferMachine() noexcept(true) {};

        void
        _publish_interfaces() override;

        void
        _connection() override;

        void
        _wait_for_peer() override;

        void
        _cloud_buffer() override;

        void
        _cloud_synchronize() override;

        void
        _initialize() override;

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
        ELLE_ATTRIBUTE(bool, interrupt);
        ELLE_ATTRIBUTE(std::string, file_path);
        ELLE_ATTRIBUTE(std::string, output_dir);
      };
    }
  }
}

#endif
