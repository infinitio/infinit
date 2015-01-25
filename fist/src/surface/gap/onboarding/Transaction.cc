#include <elle/log.hh>
#include <elle/os/file.hh>

#include <surface/gap/onboarding/Transaction.hh>
#include <surface/gap/onboarding/ReceiveMachine.hh>

#include <infinit/oracles/PeerTransaction.hh>

ELLE_LOG_COMPONENT("surface.gap.onboarding.Transaction");

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      using TransactionStatus = infinit::oracles::Transaction::Status;

      static
      std::shared_ptr<infinit::oracles::PeerTransaction>
      transaction_data(State::User const& you,
                       State::User const& sender,
                       std::string const& file_path)
      {
        auto data = std::make_shared<infinit::oracles::PeerTransaction>();
        data->id = "TransactionID";
        data->sender_id = sender.id;
        data->sender_fullname = sender.fullname;
        data->sender_device_id = "Infinit device id";
        data->recipient_id = you.id;
        data->recipient_fullname = you.fullname;
        data->recipient_device_id = "";
        data->recipient_device_name = "Your device";
        data->message = "Welcome to Infinit! Here's your first file.";
        data->ctime = ::time(nullptr);
        data->mtime = ::time(nullptr);
        try
        {
          auto path = boost::filesystem::path(file_path);
          data->files = {path.filename().string()};
          data->total_size = elle::os::file::size(path.string());
          data->files_count = 1;
          data->is_directory = false;
          data->status = TransactionStatus::initialized;
        }
        catch (elle::Exception const& e)
        {
          data->files = { "Welcome.avi" };
          data->files_count = 1;
          data->total_size = 30120;
          data->is_directory = false;
          data->status = TransactionStatus::failed;
          ELLE_WARN("unable to access file, fake transaction failed: %s",
                    e.what());
        }
        ELLE_DEBUG("onboarding transaction: %s", *data);
        return data;
      }

      Transaction::Transaction(surface::gap::State& state,
                               uint32_t id,
                               State::User const& peer,
                               std::string const& file_path,
                               reactor::Duration const& transfer_duration)
        : surface::gap::Transaction(state,
                                    id,
                                    transaction_data(
                                      state.me(), peer, file_path), true)
        , _data(std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
                  this->data()))
      {
        this->_machine.reset(new surface::gap::onboarding::ReceiveMachine(
          *this,
          id,
          this->_data,
          file_path,
          transfer_duration));

        ELLE_DEBUG_SCOPE("%s: creation", *this);
      }

      Transaction::~Transaction()
      {
        ELLE_DEBUG_SCOPE("%s: destruction", *this);
      }

      void
      Transaction::_snapshot_save() const
      {}

      surface::gap::onboarding::ReceiveMachine&
      Transaction::machine()
      {
        return *dynamic_cast<surface::gap::onboarding::ReceiveMachine*>(
          this->_machine.get());
      }

      void
      Transaction::accept(boost::optional<std::string const&> output_dir)
      {
        if (!dynamic_cast<surface::gap::onboarding::ReceiveMachine*>(
        this->_machine.get()))
        {
          surface::gap::Transaction::accept(output_dir);
        }
        else
        {
          ELLE_DEBUG("%s: accepted", *this);
          this->machine().accept();
          this->_machine->notify_user_connection_status(
            this->_data->sender_id,
            true,
            this->_data->sender_device_id,
            true);
          this->notify_peer_reachable({}, {});
        }
      }

      void
      Transaction::reset()
      {}

      void
      Transaction::print(std::ostream& stream) const
      {
        stream << "OnboardingTransaction";
      }
    }
  }
}
