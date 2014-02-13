#include <surface/gap/State.hh>
#include <surface/gap/Transaction.hh>
#include <surface/gap/Exception.hh>

#include <boost/filesystem.hpp>

#include <atomic>

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
      this->_transactions.emplace(
        std::piecewise_construct,
        std::make_tuple(id),
        std::forward_as_tuple(*this, id, peer_id, std::move(files), message));
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
          ELLE_TRACE("path %s", snapshot_path);

          elle::SafeFinally delete_snapshot([&snapshot_path]
            {
              boost::filesystem::remove(snapshot_path);
            });

          std::unique_ptr<TransactionMachine::Snapshot> snapshot;
          try
          {
            snapshot.reset(
              new TransactionMachine::Snapshot(
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
            this->user(snapshot->data.sender_id);
            this->user(snapshot->data.recipient_id);
            auto _id = generate_id();
            this->_transactions.emplace(
              std::piecewise_construct,
              std::make_tuple(_id),
              std::forward_as_tuple(*this, _id, std::move(*snapshot.release())));
          }
          catch (std::exception const&)
          {
            ELLE_ERR("%s: couldn't create transaction from snapshot at %s: %s",
                     *this, snapshot_path, elle::exception_string());
            continue;
          }
        }
      }

      for (auto& transaction: this->_transactions)
      {
        ELLE_ASSERT(transaction.second.data() != nullptr);
        this->_on_transaction_update(
          std::move(this->meta().transaction(transaction.second.data()->id)));
      }

      this->_transaction_resync();
    }

    void
    State::_transaction_resync()
    {
      ELLE_TRACE_SCOPE("%s: resync transactions", *this);

      for (auto const& id: this->meta().transactions().transactions)
      {
        this->_on_transaction_update(std::move(this->meta().transaction(id)));
      }

      // History.
      {
        static std::vector<infinit::oracles::Transaction::Status> final{
          infinit::oracles::Transaction::Status::rejected,
            infinit::oracles::Transaction::Status::finished,
            infinit::oracles::Transaction::Status::canceled,
            infinit::oracles::Transaction::Status::failed};

        std::list<std::string> transactions_ids{
          std::move(this->meta().transactions(final, true, 100).transactions)};

        for (auto const& id: transactions_ids)
        {
          auto it = std::find_if(
            std::begin(this->_transactions),
            std::end(this->_transactions),
            [&] (TransactionConstPair const& pair)
            {
              return (!pair.second.data()->id.empty()) &&
                     ( pair.second.data()->id == id);
            });

          if (it != std::end(this->_transactions))
          {
            if (!it->second.final())
            {
              it->second.on_transaction_update(this->meta().transaction(id));
            }
            continue;
          }

          infinit::oracles::Transaction transaction{this->meta().transaction(id)};

          this->user(transaction.sender_id);
          this->user(transaction.recipient_id);

          auto _id = generate_id();

          // true stands for history.
          this->_transactions.emplace(
            std::piecewise_construct,
            std::make_tuple(_id),
            std::forward_as_tuple(*this, _id, std::move(transaction), true));
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
      ELLE_TRACE_SCOPE("%s: transaction notification", *this);

      this->user(notif.sender_id);
      this->user(notif.recipient_id);

      ELLE_ASSERT(!notif.id.empty());

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
        ELLE_TRACE("%s: notification received for unknown transaction: %s",
                   *this, notif.id);
        for (auto const& tr: this->transactions())
        {
          ELLE_DEBUG("-- %s: %s", tr.first, tr.second);
        }
        infinit::oracles::Transaction data = notif;
        auto id = generate_id();
        this->_transactions.emplace(
          std::piecewise_construct,
          std::make_tuple(id),
          std::forward_as_tuple(*this, id, std::move(data)));
      }
      else
      {
        it->second.on_transaction_update(notif);
      }
    }

    void
    State::_on_peer_connection_update(
      infinit::oracles::trophonius::PeerConnectionUpdateNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: peer connection notification", *this);

      ELLE_ASSERT(!notif.transaction_id.empty());
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
        ELLE_ERR("%s: no transaction found for network %s",
                 *this, notif.transaction_id);
        ELLE_DEBUG("%s: transactions", *this)
          for (auto const& tr: this->transactions())
          {
            ELLE_DEBUG("-- %s: %s", tr.first, tr.second);
          }
        return;
        // throw TransactionNotFoundException(
        //   elle::sprintf("network %s", notif.network_id));
      }

      it->second.on_peer_connection_update(notif);
    }

  }
}
