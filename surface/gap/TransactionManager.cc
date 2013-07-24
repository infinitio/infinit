#include "TransactionManager.hh"

#include <elle/assert.hh>
#include <elle/log.hh>


ELLE_LOG_COMPONENT("infinit.surface.gap.TransactionManager");

namespace surface
{
  namespace gap
  {
    TransactionManager::TransactionManager(surface::gap::NotificationManager& notification_manager,
                                           plasma::meta::Client const& meta,
                                           SelfGetter const& self,
                                           DeviceGetter const& device):
      Notifiable(notification_manager),
      _meta(meta),
      _self(self),
      _device(device)
    {
      this->_notification_manager.transaction_callback(
        [&] (TransactionNotification const &n, bool) -> void
        {
          this->_on_transaction(n);
        });
    }

    TransactionManager::~TransactionManager()
    {
      ELLE_TRACE_METHOD("");
    }

    TransactionManager::TransactionsMap const&
    TransactionManager::all()
    {
      if (this->_transactions->get() != nullptr)
        return *this->_transactions->get();

      this->_transactions([] (TransactionMapPtr& map) {
        if (map == nullptr)
          map.reset(new TransactionsMap{});
      });

      auto response = this->_meta.transactions();
      for (auto const& id: response.transactions)
      {
        auto transaction = this->_meta.transaction(id);
        this->_transactions([&id, &transaction] (TransactionMapPtr& map) {
            (*map)[id] = transaction;
        });
      }

      return *(this->_transactions->get());
    }

    std::vector<std::string>
    TransactionManager::all_ids()
    {
      this->all(); // Ensure creation.

      return this->_transactions(
        [](TransactionMapPtr const& map)
        {
          std::vector<std::string> res{map->size()};
          for (auto const& pair: *map)
            res.emplace_back(pair.first);
          return res;
        }
      );
    }

    Transaction const&
    TransactionManager::one(std::string const& id)
    {
      auto it = this->all().find(id);
      if (it != this->all().end())
        return it->second;
      return this->sync(id);
    }

    Transaction const&
    TransactionManager::sync(std::string const& id)
    {
      ELLE_TRACE_SCOPE("%s: sync transaction %s from meta", *this, id);
      this->all(); // ensure _transactions is not null;
      try
      {
        auto transaction = this->_meta.transaction(id);
        ELLE_DEBUG("Synched transaction %s has status %d",
                   id, transaction.status);
        return this->_transactions(
          [&id, &transaction] (TransactionMapPtr& map)
            -> plasma::Transaction const&
          {
            return (*map)[id] = transaction;
          }
        );
      }
      catch (std::runtime_error const& e)
      {
        throw Exception{gap_transaction_doesnt_exist, e.what()};
      }
    }

    void
    TransactionManager::_on_transaction(Transaction const& tr)
    {
      ELLE_TRACE_SCOPE("%s: transaction callback for %s", *this, tr);

      ELLE_ASSERT(tr.recipient_id == this->_self().id ||
                  tr.sender_id == this->_self().id);

      if (tr.sender_device_id != this->_device().id &&
          (tr.recipient_device_id != this->_device().id &&
           not tr.recipient_device_id.empty()))
      {
        ELLE_TRACE("ignore transaction %s: not related to my device", tr);
        return;
      }

      ELLE_DEBUG("received transaction %s, update local copy", tr)
      {
        // Ensure map is not null
        this->all();
        this->_transactions(
          [&tr] (TransactionMapPtr& ptr) {
            auto it = ptr->find(tr.id);
            if (it != ptr->end() && it->second.status > tr.status)
            {
              throw elle::Exception{
                elle::sprintf(
                  "ignore transaction update %s: local status of %s is greater",
                  tr, it->second)};
            }
            (*ptr)[tr.id] = tr;
        });
      }
    }

  }
}
