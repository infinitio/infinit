#include <surface/gap/State.hh>
#include <surface/gap/Transaction.hh>

#include <boost/filesystem.hpp>
ELLE_LOG_COMPONENT("surface.gap.State.Transaction");

namespace surface
{
  namespace gap
  {
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
      auto tr = TransactionPtr{
          new Transaction{*this, peer_id, std::move(files), message}};
      auto id = tr->id();
      this->_transactions.emplace(id, std::move(tr));
      return id;
    }

    void
    State::_transactions_init()
    {
      ELLE_TRACE_SCOPE("%s: pull transactions", *this);

      ELLE_ASSERT(this->_transactions.empty());

      {
        std::string snapshots_path =
          common::infinit::transaction_snapshots_directory(this->me().id);
        boost::filesystem::create_directories(snapshots_path);

        boost::filesystem::recursive_directory_iterator iterator(snapshots_path);
        boost::filesystem::recursive_directory_iterator end;

        for (; iterator != end; ++iterator)
        {
          auto snapshot_path = iterator->path().string().c_str();
          ELLE_ERR("path %s", snapshot_path);

          elle::Finally delete_snapshot([&snapshot_path]
            {
              boost::filesystem::remove(snapshot_path);
            });

          std::unique_ptr<TransferMachine::Snapshot> snapshot;
          try
          {
            snapshot.reset(
              new TransferMachine::Snapshot(
                elle::serialize::from_file(snapshot_path)));
          }
          catch (std::exception const&)
          {
            ELLE_ERR("%s: couldn't load snapshot at %s: %s",
                     *this, snapshot_path, elle::exception_string());
            continue;
          }

          try
          {
            auto tr = TransactionPtr{new Transaction{*this, std::move(*snapshot.release())}};
            auto _id = tr->id();
            this->_transactions.emplace(std::move(_id), std::move(tr));
          }
          catch (std::exception const&)
          {
            ELLE_ERR("%s: couldn't create transaction from snapshot at %s: %s",
                     *this, snapshot_path, elle::exception_string());
            continue;
          }
        }
      }

      {
        std::list<std::string> transactions_ids{
          std::move(this->meta().transactions().transactions)};

        for (auto const& id: transactions_ids)
        {
          this->_on_transaction_update(std::move(this->meta().transaction(id)));
        }
      }

      // History.
      {
        static std::vector<plasma::TransactionStatus> final{
          plasma::TransactionStatus::rejected,
            plasma::TransactionStatus::finished,
            plasma::TransactionStatus::canceled,
            plasma::TransactionStatus::failed};

        std::list<std::string> transactions_ids{
          std::move(this->meta().transactions(final, true, 5).transactions)};

        for (auto const& id: transactions_ids)
        {
          plasma::Transaction transaction{this->meta().transaction(id)};

          this->user(transaction.sender_id);
          this->user(transaction.recipient_id);

          auto tr =
            TransactionPtr{new Transaction{*this, std::move(transaction)}};
          auto _id = tr->id();

          this->_transactions.emplace(_id, std::move(tr));
        }
      }
    }

    void
    State::_transaction_resync()
    {
      ELLE_TRACE_SCOPE("%s: resync transactions", *this);

      auto transactions_ids = this->meta().transactions().transactions;

      for (auto const& id: transactions_ids)
      {
        ELLE_DEBUG("%s: update transaction %s", *this, id);
        this->_on_transaction_update(this->meta().transaction(id));
      }
    }

    void
    State::_transactions_clear()
    {
      ELLE_TRACE_SCOPE("%s: clear transactions", *this);

      // XXX: Could be improved a lot.
      // XXX: Not thread safe.
      // auto it = this->_transactions.begin();
      // auto end = this->_transactions.end();

      // while (this->_transactions.size())
      // {
      //   try
      //   {
      //     this->_transactions.remove();
      //   }
      //   catch (std::exception const& e)
      //   {
      //     ELLE_ERR("%s: error while deleting machine: %s",
      //              *this, elle::exception_string());
      //     throw;
      //   }
      //}
      this->_transactions.clear();
    }

    void
    State::_on_transaction_update(plasma::Transaction const& notif)
    {
      ELLE_TRACE_SCOPE("%s: transaction notification", *this);

      this->user(notif.sender_id);
      this->user(notif.recipient_id);

      auto it = std::find_if(
        std::begin(this->_transactions),
        std::end(this->_transactions),
        [&] (TransactionConstPair const& pair)
        {
          return pair.second->data()->id == notif.id;
        });

      if (it == std::end(this->_transactions))
      {
        plasma::Transaction data = notif;
        auto tr = TransactionPtr{new Transaction{*this, std::move(data)}};
        auto id = tr->id();

        this->_transactions.emplace(id, std::move(tr));
      }
      else
      {
        it->second->on_transaction_update(notif);
      }
    }

    void
    State::_on_peer_connection_update(
      plasma::trophonius::PeerConnectionUpdateNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: peer connection notification", *this);

      auto it = std::find_if(
        std::begin(this->_transactions),
        std::end(this->_transactions),
        [&] (TransactionConstPair const& pair)
        {
          ELLE_ASSERT(pair.second != nullptr);
          return pair.second->data()->network_id == notif.network_id;
        });

      if (it == std::end(this->_transactions))
      {
        ELLE_ERR("%s: no transaction found for network %s",
                 *this, notif.network_id);
        ELLE_DEBUG("%s: transactions", *this)
          for (auto const& tr: this->transactions())
          {
            ELLE_DEBUG("-- %s: %s", tr.first, *tr.second);
          }
        return;
        // throw TransactionNotFoundException(
        //   elle::sprintf("network %s", notif.network_id));
      }

      it->second->on_peer_connection_update(notif);
    }

  }
}
