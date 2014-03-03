#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <elle/finally.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>

#include <reactor/exception.hh>
#include <reactor/thread.hh>

#include <station/Station.hh>

#include <frete/Frete.hh>
#include <frete/TransferSnapshot.hh>

#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/Rounds.hh>

ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine");

namespace surface
{
  namespace gap
  {
    using TransactionStatus = infinit::oracles::Transaction::Status;
    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   std::shared_ptr<TransactionMachine::Data> data,
                                   bool):
      TransactionMachine(state, id, std::move(data)),
      _wait_for_decision_state(
        this->_machine.state_make(
          "wait for decision", std::bind(&ReceiveMachine::_wait_for_decision, this))),
      _accept_state(
        this->_machine.state_make(
          "accept", std::bind(&ReceiveMachine::_accept, this))),
      _accepted("accepted barrier"),
      _snapshot_path(boost::filesystem::path(
                       common::infinit::frete_snapshot_path(
                         this->data()->recipient_id,
                         this->data()->id))),
      _snapshot(nullptr)
    {
      // Normal way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_accept_state,
                                    reactor::Waitables{&this->_accepted});
      this->_machine.transition_add(this->_accept_state,
                                      this->_transfer_core_state);

      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_finish_state);

      // Reject way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_reject_state,
                                    reactor::Waitables{&this->rejected()});

      // Cancel.
      this->_machine.transition_add(_wait_for_decision_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add(_accept_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add(_reject_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add(_transfer_core_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);

      // Exception.
      this->_machine.transition_add_catch(_wait_for_decision_state, _fail_state);
      this->_machine.transition_add_catch(_accept_state, _fail_state);
      this->_machine.transition_add_catch(_reject_state, _fail_state);
      this->_machine.transition_add_catch(_transfer_core_state, _fail_state);

      if (boost::filesystem::exists(this->_snapshot_path))
      {
        ELLE_LOG("snapshot exist at %s", this->_snapshot_path);
        try
        {
          elle::SafeFinally delete_snapshot{
            [&]
            {
              try
              {
                boost::filesystem::remove(this->_snapshot_path);
              }
              catch (std::exception const&)
              {
                ELLE_ERR("couldn't delete snapshot at %s: %s",
                         this->_snapshot_path, elle::exception_string());
              }
            }};

          this->_snapshot.reset(
            new frete::TransferSnapshot(
              elle::serialize::from_file(this->_snapshot_path.string())));
        }
        catch (std::exception const&) //XXX: Choose the right exception here.
        {
          ELLE_ERR("%s: snap shot was invalid: %s", *this, elle::exception_string());
        }
      }
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   TransactionMachine::State const current_state,
                                   std::shared_ptr<TransactionMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: construct from data %s, starting at %s",
                       *this, *this->data(), current_state);

      switch (current_state)
      {
        case TransactionMachine::State::NewTransaction:
          //
          break;
        case TransactionMachine::State::SenderCreateTransaction:
        case TransactionMachine::State::SenderWaitForDecision:
          elle::unreachable();
        case TransactionMachine::State::RecipientWaitForDecision:
          this->_run(this->_wait_for_decision_state);
          break;
        case TransactionMachine::State::RecipientAccepted:
          this->_run(this->_accept_state);
          break;
        case TransactionMachine::State::PublishInterfaces:
        case TransactionMachine::State::Connect:
        case TransactionMachine::State::PeerDisconnected:
        case TransactionMachine::State::PeerConnectionLost:
        case TransactionMachine::State::Transfer:
          this->_run(this->_transfer_core_state);
          break;
        case TransactionMachine::State::Finished:
          this->_run(this->_finish_state);
          break;
        case TransactionMachine::State::Rejected:
          this->_run(this->_reject_state);
          break;
        case TransactionMachine::State::Canceled:
          this->_run(this->_cancel_state);
          break;
        case TransactionMachine::State::Failed:
          this->_run(this->_fail_state);
          break;
        default:
          elle::unreachable();
      }
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   std::shared_ptr<TransactionMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: constructing machine for transaction %s",
                       *this, data);

      switch (this->data()->status)
      {
        case TransactionStatus::initialized:
          this->_run(this->_wait_for_decision_state);
          break;
        case TransactionStatus::accepted:
          this->_run(this->_transfer_core_state);
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
        case TransactionStatus::created:
          break;
        case TransactionStatus::started:
        case TransactionStatus::none:
          elle::unreachable();
      }
    }

    ReceiveMachine::~ReceiveMachine()
    {
      this->_stop();
    }

    void
    ReceiveMachine::transaction_status_update(TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      switch (status)
      {
        case TransactionStatus::canceled:
          ELLE_DEBUG("%s: open canceled barrier", *this)
            this->canceled().open();
          break;
        case TransactionStatus::failed:
          ELLE_DEBUG("%s: open failed barrier", *this)
            this->failed().open();
          break;
        case TransactionStatus::finished:
          ELLE_DEBUG("%s: open finished barrier", *this)
            this->finished().open();
          break;
        case TransactionStatus::accepted:
        case TransactionStatus::rejected:
        case TransactionStatus::initialized:
          ELLE_DEBUG("%s: ignore status %s", *this, status);
          break;
        case TransactionStatus::created:
        case TransactionStatus::started:
        case TransactionStatus::none:
          elle::unreachable();
      }
    }

    void
    ReceiveMachine::accept()
    {
      ELLE_TRACE_SCOPE("%s: open accept barrier %s", *this, this->transaction_id());
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      if (!this->_accepted.opened())
      {
        this->state().composite_reporter().transaction_accepted(
          this->transaction_id()
        );
      }

      this->_accepted.open();
    }

    void
    ReceiveMachine::reject()
    {
      ELLE_TRACE_SCOPE("%s: open rejected barrier %s", *this, this->transaction_id());
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      if (!this->rejected().opened())
      {
        this->state().composite_reporter().transaction_ended(
          this->transaction_id(),
          infinit::oracles::Transaction::Status::rejected,
          ""
        );
      }

      this->rejected().open();
    }

    void
    ReceiveMachine::_wait_for_decision()
    {
      ELLE_TRACE_SCOPE("%s: waiting for decision %s", *this, this->transaction_id());
      this->current_state(TransactionMachine::State::RecipientWaitForDecision);
    }

    void
    ReceiveMachine::_accept()
    {
      ELLE_TRACE_SCOPE("%s: accepted %s", *this, this->transaction_id());
      this->current_state(TransactionMachine::State::RecipientAccepted);

      try
      {
        this->state().meta().update_transaction(this->transaction_id(),
                                                TransactionStatus::accepted,
                                                this->state().device().id,
                                                this->state().device().name);
      }
      catch (infinit::oracles::meta::Exception const& e)
      {
        if (e.err == infinit::oracles::meta::Error::transaction_already_has_this_status)
          ELLE_TRACE("%s: transaction already accepted: %s", *this, e.what());
        else if (e.err == infinit::oracles::meta::Error::transaction_operation_not_permitted)
          ELLE_TRACE("%s: transaction can't be accepted: %s", *this, e.what());
        else
          throw;
      }
    }

    boost::filesystem::path
    ReceiveMachine::eligible_name(boost::filesystem::path const path,
                                  std::string const& name_policy)
    {
      if (!boost::filesystem::exists(path))
        return path;

      auto _path = path.filename();
      // Remove the extensions, add the name_policy and set the extension.
      std::string extensions;
      for (; !_path.extension().empty(); _path = _path.stem())
        extensions = _path.extension().string() + extensions;
      _path = path.parent_path() / _path;
      _path += name_policy;
      _path += extensions;

      // Ugly.
      for (size_t i = 2; i < std::numeric_limits<size_t>::max(); ++i)
      {
        if (!boost::filesystem::exists(elle::sprintf(_path.string().c_str(), i)))
        {
          return elle::sprintf(_path.string().c_str(), i);
        }
      }

      throw elle::Exception(
        elle::sprintf("unable to find a suitable name that matches %s", _path));
    }

    boost::filesystem::path
    ReceiveMachine::trim(boost::filesystem::path const& item,
                         boost::filesystem::path const& root)
    {
      if (item == root)
        return "";

      auto it = item.begin();
      boost::filesystem::path rel;
      for(; rel != root && it != item.end(); ++it)
        rel /= *it;
      if (it == item.end())
        throw elle::Exception(
          elle::sprintf("%s is not the root of %s", root, item));

      boost::filesystem::path trimed;
      for (; it != item.end(); ++it)
        trimed /= *it;

      return trimed;
    }

    void
    ReceiveMachine::_transfer_operation(frete::RPCFrete& frete)
    {
      ELLE_TRACE_SCOPE("%s: transfer operation", *this);

      elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        scope.run_background(
          elle::sprintf("download %s", this->id()),
          [&frete, this] ()
          {
            this->get(frete,
                      boost::filesystem::path{this->state().output_dir()});
            this->finished().open();
          });

        scope.run_background(
          elle::sprintf("run rpcs %s", this->id()),
          [&frete] ()
          {
            frete.run();
          });

        scope.wait();
      };
    }

    std::unique_ptr<frete::RPCFrete>
    ReceiveMachine::rpcs(infinit::protocol::ChanneledStream& channels)
    {
      return elle::make_unique<frete::RPCFrete>(channels);
    }

    float
    ReceiveMachine::progress() const
    {
      if (this->_snapshot != nullptr)
        return this->_snapshot->progress();
      return 0.0f;
    }

    void
    ReceiveMachine::get(frete::RPCFrete& frete,
                        boost::filesystem::path const& output_path,
                        bool strong_encryption,
                        std::string const& name_policy)
    {
      auto peer_version = frete.version();
      if (peer_version < elle::Version(0, 8, 3))
      {
        // XXX: Create better exception.
        if (strong_encryption)
          ELLE_WARN("peer version doesn't support strong encryption");
        strong_encryption = false;
      }

      auto count = frete.count();

      // total_size can be 0 if all files are empty.
      auto total_size = frete.full_size();

      if (this->_snapshot != nullptr)
      {
        if ((this->_snapshot->total_size() != total_size) ||
            (this->_snapshot->count() != count))
        {
          ELLE_ERR("snapshot data (%s) are invalid", *this->_snapshot);
          throw elle::Exception("invalid transfer data");
        }
      }
      else
      {
        this->_snapshot.reset(new frete::TransferSnapshot(count, total_size));
      }

      static std::streamsize const chunk_size = 1 << 18;

      ELLE_DEBUG("transfer snapshot: %s", *this->_snapshot);

      auto last_index = this->_snapshot->transfers().size();
      if (last_index > 0)
        --last_index;

      // If files are present in the snapshot, take the last one.
      for (auto index = last_index; index < count; ++index)
      {
        ELLE_DEBUG("%s: index %s", *this, index);

        boost::filesystem::path fullpath;
        // XXX: Merge file_size & rpc_path.
        auto file_size = frete.file_size(index);

        if (this->_snapshot->transfers().find(index) != this->_snapshot->transfers().end())
        {
          auto const& transfer = this->_snapshot->transfers().at(index);
          fullpath = this->_snapshot->transfers().at(index).full_path();

          if (file_size != transfer.file_size())
          {
            ELLE_ERR("%s: transfer data (%s) at index %s are invalid.",
                     *this, transfer, index);

            throw elle::Exception("invalid transfer data");
          }
        }
        else
        {
          auto relativ_path = boost::filesystem::path{frete.path(index)};
          fullpath = ReceiveMachine::eligible_name(output_path / relativ_path,
                                                   name_policy);
          relativ_path = ReceiveMachine::trim(fullpath, output_path);

          this->_snapshot->transfers().emplace(
            std::piecewise_construct,
            std::make_tuple(index),
            std::forward_as_tuple(index, output_path, relativ_path, file_size));
        }

        ELLE_ASSERT(this->_snapshot->transfers().find(index) !=
                    this->_snapshot->transfers().end());

        auto& tr = this->_snapshot->transfers().at(index);

        ELLE_DEBUG("%s: index (%s) - path %s - size %s",
                   *this, index, fullpath, file_size);

        // Create subdir.
        boost::filesystem::create_directories(fullpath.parent_path());
        boost::filesystem::ofstream output{fullpath,
            std::ios::app | std::ios::binary};

        if (tr.complete())
        {
          ELLE_DEBUG("%s: transfer was marked as complete", *this);
          continue;
        }

        auto key = strong_encryption ?
          infinit::cryptography::SecretKey(
            this->state().identity().pair().k().decrypt<
              infinit::cryptography::SecretKey>(frete.key_code())) :
          infinit::cryptography::SecretKey(
            infinit::cryptography::cipher::Algorithm::aes256,
            this->transaction_id());

        while (true)
        {
          if (!output.good())
            throw elle::Exception("output is invalid");

          // Get the buffer from the rpc.
          elle::Buffer buffer{
              key.decrypt<elle::Buffer>(
                strong_encryption ?
                frete.encrypted_read(index, tr.progress(), chunk_size) :
                frete.read(index, tr.progress(), chunk_size))};

          ELLE_ASSERT_LT(index, this->_snapshot->transfers().size());
          ELLE_DUMP("%s: %s (size: %s)",
                    index, fullpath, boost::filesystem::file_size(fullpath));
          {
            boost::system::error_code ec;
            auto size = boost::filesystem::file_size(fullpath, ec);

            if (ec)
            {
              ELLE_ERR("destination file deleted");
              throw elle::Exception("destination file deleted");
            }

            if (size != this->_snapshot->transfers()[index].progress())
            {
              uintmax_t current_size;
              try
              {
                current_size = boost::filesystem::file_size(fullpath);
              }
              catch (boost::filesystem::filesystem_error const& e)
              {
                ELLE_ERR("%s: expected size and actual size differ, "
                         "unable to determine actual size: %s", *this, e);
                throw elle::Exception("destination file corrupted");
              }
              ELLE_ERR(
                "%s: expected file size %s and actual file size %s are different",
                *this,
                this->_snapshot->transfers()[index].progress(),
                current_size);
              throw elle::Exception("destination file corrupted");
            }
          }

          ELLE_DUMP("content: %s (%sB)", buffer, buffer.size());

          // Write the file.
          output.write((char const*) buffer.contents(), buffer.size());
          output.flush();

          if (!output.good())
            throw elle::Exception("writing left the stream in a bad state");
          {
            this->_snapshot->increment_progress(index, buffer.size());
            elle::serialize::to_file(this->_snapshot_path.string()) << *this->_snapshot;
            frete.set_progress(this->_snapshot->progress());
            // this->_progress_changed.signal();
          }
          ELLE_DEBUG("%s: %s (size: %s)",
                     index, fullpath, boost::filesystem::file_size(fullpath));
          // XXX: Shouldn't be an assert, the user can rm the file. The
          // transaction should fail but not create an assertion error.
          ELLE_ASSERT_EQ(boost::filesystem::file_size(fullpath),
                         this->_snapshot->transfers()[index].progress());
          if (buffer.size() < unsigned(chunk_size))
          {
            output.close();
            ELLE_TRACE("finished %s: %s", index, *this->_snapshot);
            break;
          }
        }
      }

      // this->finished.open();
      this->_finish();

      try
      {
        boost::filesystem::remove(this->_snapshot_path);
      }
      catch (std::exception const&)
      {
        ELLE_ERR("couldn't delete snapshot at %s: %s",
                 this->_snapshot_path, elle::exception_string());
      }
    }

    std::string
    ReceiveMachine::type() const
    {
      return "ReceiveMachine";
    }
  }
}
