#include <surface/gap/GhostReceiveMachine.hh>

#include <boost/filesystem/fstream.hpp>

#include <elle/AtomicFile.hh>
#include <elle/serialization/json/SerializerIn.hh>
#include <elle/serialization/json/SerializerOut.hh>
#include <reactor/http/Request.hh>
#include <reactor/http/exceptions.hh>

#include <infinit/metrics/Reporter.hh>

ELLE_LOG_COMPONENT("surface.gap.GhostReceiveMachine");


namespace surface
{
  namespace gap
  {

    using TransactionStatus = infinit::oracles::Transaction::Status;

    GhostReceiveMachine::GhostReceiveMachine(
      Transaction& transaction,
      uint32_t id,
      std::shared_ptr<Data> data)
      : TransactionMachine(transaction, id, data)
      , ReceiveMachine(transaction, id, data)
      , _wait_for_cloud_upload_state(
          this->_machine.state_make(
          "wait_for_cloud_upload_state",
          std::bind(&GhostReceiveMachine::_wait_for_cloud_upload, this)))
      , _snapshot_path(this->transaction().snapshots_directory()
                        / "ghostreceive.snapshot")
    {
      this->_machine.transition_add(this->_accept_state,
                                    this->_finish_state);
      this->_machine.transition_add(this->_wait_for_cloud_upload_state,
                                    this->_wait_for_decision_state,
                                    reactor::Waitables{&this->_cloud_uploaded});
      ELLE_TRACE("%s: starting with status %s", *this, this->data()->status);
      switch (this->data()->status)
      {
        case TransactionStatus::created:
        case TransactionStatus::initialized:
          this->_run(this->_wait_for_cloud_upload_state);
          break;
        case TransactionStatus::ghost_uploaded:
          this->_run(this->_wait_for_decision_state);
          break;
        case TransactionStatus::accepted:
          if (this->concerns_this_device())
            this->_run(this->_accept_state);
          else
            this->_run(this->_another_device_state);
          break;
        case TransactionStatus::finished:
          this->_run(this->_finish_state);
          break;
        case TransactionStatus::canceled:
          this->_run(this->_cancel_state);
          break;
        case TransactionStatus::failed:
          this->_run(this->_fail_state);
          break;
        case TransactionStatus::rejected:
          break;
        case TransactionStatus::started:
        case TransactionStatus::none:
        case TransactionStatus::deleted:
          elle::unreachable();
      }
    }
    void
    GhostReceiveMachine::transaction_status_update(TransactionStatus status)
    {
      ELLE_TRACE("%s: received new status %s", *this, status);
      if (status == TransactionStatus::ghost_uploaded)
        _cloud_uploaded.open();
      else
        ReceiveMachine::transaction_status_update(status);
    }
    float
    GhostReceiveMachine::progress() const
    {
      if (!_request)
        return 0;
      reactor::http::Request::Progress p = _request->progress();
      if (p.download_total == 0)
      {
        ELLE_TRACE("%s: unknown download total, cannot compute progress", *this);
        return 0;
      }
      return (float)(p.download_current + _previous_progress)
           / (float)(_previous_progress +  p.download_total);
    }
    void
    GhostReceiveMachine::_accept()
    {
      ELLE_TRACE("%s accepting", *this);
      ReceiveMachine::_accept();
      if (this->state().metrics_reporter())
        this->state().metrics_reporter()->transaction_accepted(
          this->transaction_id(), false);

      auto peer_data =
        std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
          transaction().data());
      ELLE_ASSERT(!!peer_data);
      std::string url;
      if (peer_data->download_link)
        url = peer_data->download_link.get();
      else
      {
        ELLE_WARN("%s: empty url, re-fetching transaction from meta");
        State& state = transaction().state();
        infinit::oracles::PeerTransaction pt =
          state.meta().transaction(transaction().data()->id);
        url = pt.download_link.get();
      }
      ELLE_TRACE("%s accepting on url '%s'", *this, url);
      if (boost::filesystem::exists(snapshot_path()))
      {
        elle::AtomicFile file(snapshot_path());
        file.read() << [&] (elle::AtomicFile::Read& read)
        {
          elle::serialization::json::SerializerIn input(read.stream());
          std::string target_file;
          input.serialize("target_file", target_file);
          _path = target_file;
        };
      }
      ELLE_TRACE("%s: saved path is: '%s'", *this, _path.string());
      if (_path.empty())
      {
        // extract path from url
        size_t end = url.find_first_of('?');
        size_t begin = url.substr(0, end).find_last_of('/');
        std::string filename = url.substr(begin+1, end - begin - 1);
        ELLE_TRACE("%s: extracted file name of '%s' from '%s'", *this, filename, url);
        _path = boost::filesystem::path(transaction().state().output_dir())
          / filename;
        std::map<boost::filesystem::path, boost::filesystem::path> unused;
        _path = eligible_name(transaction().state().output_dir(),
                              filename,
                              " (%s)",
                              unused);
        ELLE_TRACE("%s: will write to %s, saving into %s", *this, _path,
                   snapshot_path());

        elle::AtomicFile file(snapshot_path());
        file.write() << [&] (elle::AtomicFile::Write& write)
        {
          elle::serialization::json::SerializerOut output(write.stream());
          std::string target_file = _path.string();
          output.serialize("target_file", target_file);
        };
      }

      int attempt = 0;
      while (true)
      {
        ++attempt;
        { // Extra scope so that catch clauses can set metrics arguments
          // declared just below.
          auto start_time = boost::posix_time::microsec_clock::universal_time();
          infinit::metrics::TransferExitReason exit_reason =
            infinit::metrics::TransferExitReasonUnknown;
          std::string exit_message;
          uint64_t total_bytes_transfered = 0;
          elle::SafeFinally write_end_message([&,this]
            {
              if (auto& mr = state().metrics_reporter())
              {
                auto now = boost::posix_time::microsec_clock::universal_time();
                float duration =
                  float((now - start_time).total_milliseconds()) / 1000.0f;
                mr->transaction_transfer_end(this->transaction_id(),
                                             infinit::metrics::TransferMethodGhostCloud,
                                             duration,
                                             total_bytes_transfered,
                                             exit_reason,
                                             exit_message);
              }
            });
          try
          {
            if (auto& mr = state().metrics_reporter())
            {
              mr->transaction_transfer_begin(
                this->transaction_id(),
                infinit::metrics::TransferMethodCloud,
                0);
            }
            boost::system::error_code erc;
            _previous_progress = boost::filesystem::file_size(_path, erc);
            if (erc)
              _previous_progress = 0;
            ELLE_TRACE("%s: resuming at %s", *this, _previous_progress);
            boost::filesystem::ofstream stream(_path, std::ios::app|std::ios::binary);
            using namespace reactor::http;
            Request::Configuration config
              = Request::Configuration(reactor::DurationOpt(), 60_sec);
            config.header_add("Range", elle::sprintf("bytes=%s-", _previous_progress));

            _request = elle::make_unique<Request>(url, Method::GET, config);
            _request->finalize();
            // Waiting for the status here will wait for full download.
            static const int buffer_size = 16000;
            char* buffer = new char[buffer_size];
            elle::SafeFinally cleanup([&] { delete[] buffer;});
            while (true)
            {
              ELLE_DUMP("%s: read", *this);
              _request->read(buffer, buffer_size);
              int bytes_read = _request->gcount();
              ELLE_DUMP("%s: read %s,  progress %s", *this, bytes_read, progress());
              StatusCode s = _request->status();
              if (s != static_cast<StatusCode>(0) && s != StatusCode::OK
                && s != StatusCode::Partial_Content)
                 throw std::runtime_error( // Consider this as fatal.
                   elle::sprintf("HTTP error %s : %s",
                                 s, std::string(buffer, bytes_read)));
              if (!bytes_read)
                break;
              stream.write(buffer, bytes_read);
              total_bytes_transfered += bytes_read;
            }
            exit_reason = infinit::metrics::TransferExitReasonFinished;
            break;
          }
          catch(reactor::Terminate const&)
          {
            exit_reason = infinit::metrics::TransferExitReasonTerminated;
            throw;
          }
          catch(reactor::network::Exception const& e)
          {
            exit_message = e.what();
            exit_reason = infinit::metrics::TransferExitReasonError;
            ELLE_WARN("%s: network exception: %s", *this, e);
            reactor::sleep(boost::posix_time::milliseconds(
              std::min(int(500 * pow(2,attempt)), 20000)));
          }
          catch (reactor::http::RequestError const& e)
          {
            exit_message = e.what();
            exit_reason = infinit::metrics::TransferExitReasonError;
            ELLE_WARN("%s: request exception: %s", *this, e);
            reactor::sleep(boost::posix_time::milliseconds(
              std::min(int(500 * pow(2,attempt)), 20000)));
          }
          catch(std::exception const& e)
          {
            exit_message = e.what();
            exit_reason = infinit::metrics::TransferExitReasonError;
            ELLE_WARN("%s: interupted by %s", *this, e.what());
            throw;
          }
        }
      }
      this->state().meta().update_transaction(this->transaction_id(),
                                              TransactionStatus::finished,
                                              this->state().device().id,
                                              this->state().device().name);
    }
    void
    GhostReceiveMachine::_wait_for_cloud_upload()
    {
      ELLE_TRACE("%s: _wait_for_cloud_upload()", *this);
    }

    void
    GhostReceiveMachine::_finalize(TransactionStatus status)
    {
      ELLE_TRACE("%s: finalize with status %s", *this, status);
      if (status != infinit::oracles::Transaction::Status::finished)
      {
        this->state().meta().update_transaction(
          this->transaction_id(), status);
      }
      if (this->state().metrics_reporter())
        this->state().metrics_reporter()->transaction_ended(
          this->transaction_id(),
          status,
          "",
          false,
          this->transaction().canceled_by_user());
    }

    void
    GhostReceiveMachine::cleanup()
    {
    }

    GhostReceiveMachine::~GhostReceiveMachine()
    {
      this->_stop();
    }
  }
}
