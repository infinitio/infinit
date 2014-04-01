

#include <elle/log.hh>

#include <surface/gap/onboarding/Transaction.hh>

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
                               reactor::Duration const& transfer_duration):
        surface::gap::Transaction(state,
                                  id,
                                  transaction_data(state.me(),
                                                   peer)),
        _progress(0.0f),
        _duration(transfer_duration),
        _accepted("accepted"),
        _thread(
          new reactor::Thread(
            *reactor::Scheduler::scheduler(),
            "onboarding transaction",
            [&]
            {
              auto set_status = [&] (gap_TransactionStatus status)
                {
                  this->last_status(status);
                  state.enqueue(Notification(this->id(), status));
                };
              set_status(gap_transaction_waiting_for_accept);
              this->_accepted.wait();
              this->data()->status = TransactionStatus::accepted;
              set_status(gap_transaction_accepted);
              reactor::sleep(500_ms);
              set_status(gap_transaction_preparing);
              set_status(gap_transaction_running);
              while (this->_progress < 1.0f)
              {
                static reactor::Duration step = 200_ms;
                this->_progress +=
                  (step.total_milliseconds() /
                   (1.0f * this->_duration.total_milliseconds()));
                reactor::sleep(step);
              }
              this->data()->status = TransactionStatus::finished;
              set_status(gap_transaction_cleaning);
              set_status(gap_transaction_finished);
            }))
      {
        ELLE_DEBUG_SCOPE("%s: creation", *this);
      }

      Transaction::~Transaction()
      {
        ELLE_DEBUG_SCOPE("%s: destruction", *this);
        if (this->_thread)
          this->_thread->terminate_now();
      }

      void
      Transaction::accept()
      {
        ELLE_DEBUG("%s: accepted", *this);
        this->_accepted.open();
      }

      void
      Transaction::join()
      {
        this->_thread->terminate_now();
        this->_thread.reset();
      }

      float
      Transaction::progress() const
      {
        return this->_progress;
      }

      void
      Transaction::print(std::ostream& stream) const
      {
        stream << "OnboardingTransaction";
      }
    }
  }
}
