#include <atomic>

#include <boost/filesystem.hpp>

#include <surface/gap/State.hh>
#include <surface/gap/Transaction.hh>
#include <surface/gap/Exception.hh>

#include <common/common.hh>

ELLE_LOG_COMPONENT("surface.gap.State.Transaction");

namespace surface
{
  namespace gap
  {
    /// Generate a id for local user.
    static
    uint32_t
    generate_id()
    {
      static std::atomic<uint32_t> id{null_id};
      return ++id;
    }

    State::TransactionNotFoundException::TransactionNotFoundException(
      uint32_t id):
      Exception{
      gap_transaction_doesnt_exist, elle::sprintf("unknown transaction %s", id)}
    {
      ELLE_ERR("transaction %s not found", id);
    }

    State::TransactionNotFoundException::TransactionNotFoundException(
      std::string const& id):
      Exception{
      gap_transaction_doesnt_exist, elle::sprintf("unknown transaction %s", id)}
    {
      ELLE_ERR("transaction %s not found", id);
    }

    uint32_t
    State::send_files(std::string const& peer_id,
                      std::unordered_set<std::string>&& files,
                      std::string const& message)
    {
      auto id = generate_id();
      ELLE_TRACE("%s: create send transaction", *this)
        this->_transactions.emplace(
          std::piecewise_construct,
          std::make_tuple(id),
          std::forward_as_tuple(*this, id, peer_id, std::move(files), message));
      return id;
    }

    void
    State::_transactions_init()
    {
      ELLE_ASSERT(this->_transactions.empty());
      ELLE_TRACE("%s: load transactions from snapshots", *this)
      {
        std::string snapshots_path =
          common::infinit::transaction_snapshots_directory(this->me().id);
        boost::filesystem::create_directories(snapshots_path);
        using boost::filesystem::recursive_directory_iterator;
        recursive_directory_iterator iterator(snapshots_path);
        recursive_directory_iterator end;
        for (; iterator != end; ++iterator)
        {
          auto snapshot_path = iterator->path().string().c_str();
          ELLE_TRACE("%s: load transaction snapshot %s", *this, snapshot_path);
          elle::SafeFinally delete_snapshot(
            [&snapshot_path]
            {
              boost::filesystem::remove(snapshot_path);
            });
          std::unique_ptr<TransactionMachine::Snapshot> snapshot;
          try
          {
            snapshot.reset(
              new TransactionMachine::Snapshot(
                elle::serialize::from_file(snapshot_path)));
            // FIXME: this should be in the snapshot deserialization.
            // FIXME: this can happen if you kill the client while creating a
            //        transaction and you haven't fetch an id from the server
            //        yet. Test and fix that shit.
            ELLE_ASSERT(!snapshot->data.id.empty());
          }
          catch (std::exception const&)
          {
            ELLE_ERR("%s: couldn't load snapshot at %s: %s",
                     *this, snapshot_path, elle::exception_string());
            continue;
          }
          try
          {
            this->user(snapshot->data.sender_id);
            this->user(snapshot->data.recipient_id);
            auto _id = generate_id();
            ELLE_TRACE("%s: create transaction from snapshot", *this)
              this->_transactions.emplace(
                std::piecewise_construct,
                std::make_tuple(_id),
                std::forward_as_tuple(*this, _id,
                                      std::move(*snapshot.release())));
          }
          catch (std::exception const&)
          {
            ELLE_ERR("%s: couldn't create transaction from snapshot at %s: %s",
                     *this, snapshot_path, elle::exception_string());
            continue;
          }
        }
        for (auto& transaction: this->_transactions)
        {
          ELLE_ASSERT(transaction.second.data() != nullptr);
          this->_on_transaction_update(
            std::move(this->meta().transaction(transaction.second.data()->id)));
        }
      }
      ELLE_TRACE("%s: load transactions from meta", *this)
        this->_transaction_resync();
    }

    void
    State::_transaction_resync()
    {
      ELLE_TRACE("%s: resynchronize active transactions from meta", *this)
        for (auto& transaction: this->meta().transactions())
        {
          this->_on_transaction_update(std::move(transaction));
        }
      ELLE_TRACE("%s: resynchronize transaction history from meta", *this)
      {
        static std::vector<infinit::oracles::Transaction::Status> final{
          infinit::oracles::Transaction::Status::rejected,
            infinit::oracles::Transaction::Status::finished,
            infinit::oracles::Transaction::Status::canceled,
            infinit::oracles::Transaction::Status::failed};
        for (auto& transaction: this->meta().transactions(final, true, 100))
        {
          auto it = std::find_if(
            std::begin(this->_transactions),
            std::end(this->_transactions),
            [&] (TransactionConstPair const& pair)
            {
              return (!pair.second.data()->id.empty()) &&
                     ( pair.second.data()->id == transaction.id);
            });
          if (it != std::end(this->_transactions))
          {
            if (!it->second.final())
            {
              it->second.on_transaction_update(transaction);
            }
            continue;
          }
          ELLE_DEBUG("ensure that both user are fetched")
          {
            this->user(transaction.sender_id);
            this->user(transaction.recipient_id);
          }
          auto _id = generate_id();
          ELLE_TRACE("%s: create history transaction from data: %s",
                     *this, transaction)
            this->_transactions.emplace(
              std::piecewise_construct,
              std::make_tuple(_id),
              std::forward_as_tuple(*this, _id, std::move(transaction),
                                    true /* history */));
        }
      }
    }

    void
    State::_transactions_clear()
    {
      ELLE_TRACE_SCOPE("%s: clear transactions", *this);
      // We assume that Transaction destructor doesn't throw.
      this->_transactions.clear();
    }

    void
    State::_on_transaction_update(infinit::oracles::Transaction const& notif)
    {
      ELLE_TRACE_SCOPE("%s: receive transaction notification: %s",
                       *this, notif.id);
      ELLE_ASSERT(!notif.id.empty());
      ELLE_DEBUG("ensure that both user are fetched")
      {
        this->user(notif.sender_id);
        this->user(notif.recipient_id);
      }
      ELLE_DEBUG("search for a local transaction to update");
      auto it = std::find_if(
        std::begin(this->_transactions),
        std::end(this->_transactions),
        [&] (TransactionConstPair const& pair)
        {
          return (!pair.second.data()->id.empty()) &&
                 (pair.second.data()->id == notif.id);
        });
      if (it == std::end(this->_transactions))
      {
        ELLE_TRACE_SCOPE("create transaction from notification: %s", notif);
        infinit::oracles::Transaction data = notif;
        auto id = generate_id();
        this->_transactions.emplace(
          std::piecewise_construct,
          std::make_tuple(id),
          std::forward_as_tuple(*this, id, std::move(data)));
      }
      else
      {
        ELLE_DEBUG_SCOPE("update transaction %s", notif.id);
        it->second.on_transaction_update(notif);
      }
    }

    void
    State::_on_peer_reachability_updated(
      infinit::oracles::trophonius::PeerReachabilityNotification const& notif)
    {
      ELLE_TRACE_SCOPE(
        "%s: peer (%s)published his interfaces for transaction %s",
        *this, notif.status ? "" : "un", notif.transaction_id);
      ELLE_ASSERT(!notif.transaction_id.empty());
      ELLE_DEBUG("search for the local transaction to notify");
      auto it = std::find_if(
        std::begin(this->_transactions),
        std::end(this->_transactions),
        [&] (TransactionConstPair const& pair)
        {
          return (!pair.second.data()->id.empty()) &&
                 (pair.second.data()->id == notif.transaction_id);
        });
      if (it == std::end(this->_transactions))
      {
        ELLE_WARN("interface publication: transaction %s doesn't exist",
                  notif.transaction_id);
        return;
      }
      it->second.on_peer_reachability_updated(notif);
    }
  }
}
