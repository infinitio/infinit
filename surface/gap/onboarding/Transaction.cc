#include <elle/log.hh>

#include <surface/gap/onboarding/Transaction.hh>
#include <surface/gap/onboarding/ReceiveMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.onboarding.Transaction");

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      using TransactionStatus = infinit::oracles::Transaction::Status;

      static
      surface::gap::Transaction::Data
      transaction_data(State::User const& you,
                       State::User const& sender)
      {
        surface::gap::Transaction::Data data;
        data.id = "TransactionID";
        data.sender_id = sender.id;
        data.sender_fullname = sender.fullname;
        data.sender_device_id = "Infinit device id";
        data.recipient_id = you.id;
        data.recipient_fullname = you.fullname;
        data.recipient_device_id = "Your device id";
        data.recipient_device_name = "Your device";
        data.message = "Here is your fist file.";
        data.files = {"Welcome.avi"};
        data.files_count = 1;
        data.total_size = 30120;
        data.is_directory = false;
        data.status = TransactionStatus::initialized;
        data.ctime = ::time(nullptr);
        data.mtime = ::time(nullptr);

        ELLE_DEBUG("onboarding transaction: %s", data);
        return data;
      }

      Transaction::Transaction(surface::gap::State const& state,
                               uint32_t id,
                               State::User const& peer,
                               reactor::Duration const& transfer_duration)
        : surface::gap::Transaction(state,
                                      id,
                                      transaction_data(state.me(),
                                                         peer))
        , _thread(new reactor::Thread(
                    *reactor::Scheduler::scheduler(),
                    "onboarding transaction",
                    [&]
                    {
                    }))
      {
        this->_machine.reset(new surface::gap::onboarding::ReceiveMachine(
          state,
          id,
          this->data(),
          transfer_duration));

        ELLE_DEBUG_SCOPE("%s: creation", *this);
      }

      Transaction::~Transaction()
      {
        ELLE_DEBUG_SCOPE("%s: destruction", *this);
        if (this->_thread)
          this->_thread->terminate_now();
      }

      surface::gap::onboarding::ReceiveMachine&
      Transaction::machine()
      {
        return *static_cast<surface::gap::onboarding::ReceiveMachine*>(
          this->_machine.get());
      }

      void
      Transaction::accept()
      {
        ELLE_DEBUG("%s: accepted", *this);
        this->machine().accept();
        this->peer_availability_status(true);
        this->peer_connection_status(true);
      }

      void
      Transaction::peer_connection_status(bool status)
      {
        this->_machine->peer_connection_changed(status);
      }

      void
      Transaction::peer_availability_status(bool status)
      {
        this->_machine->peer_availability_changed(status);
      }

      void
      Transaction::interrupt_transfer()
      {
        this->peer_availability_status(false);
        this->machine().interrupt_transfer();
      }

      void
      Transaction::pause()
      {
         this->machine().pause();
      }

      void
      Transaction::print(std::ostream& stream) const
      {
        stream << "OnboardingTransaction";
      }
    }
  }
}
