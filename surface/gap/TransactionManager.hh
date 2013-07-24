#ifndef SURFACE_GAP_TRANSACTIONMANAGER_HH
# define SURFACE_GAP_TRANSACTIONMANAGER_HH

# include "NotificationManager.hh"
# include "usings.hh"

# include <plasma/meta/Client.hh>

# include <elle/attribute.hh>
# include <elle/container/set.hh>
# include <elle/Printable.hh>
# include <elle/threading/Monitor.hh>

# include <string>
# include <unordered_set>

namespace surface
{
  namespace gap
  {
    class TransactionManager:
      public Notifiable
    {
      typedef std::function<Self const&()> SelfGetter;
      typedef std::function<Device const&()> DeviceGetter;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      TransactionManager(surface::gap::NotificationManager& notification_manager,
                         plasma::meta::Client const& meta,
                         SelfGetter const& self,
                         DeviceGetter const& device);

      virtual
      ~TransactionManager();

      /*--------.
      | Storage |
      `--------*/
    private:
      typedef std::map<std::string, plasma::Transaction> TransactionsMap;
      typedef std::unique_ptr<TransactionsMap> TransactionMapPtr;
      elle::threading::Monitor<TransactionMapPtr> _transactions;

    public:
      /// @brief Get the transactions.
      TransactionsMap const&
      all();

      /// @brief Get the transactions.
      std::vector<std::string>
      all_ids();

      /// @brief Get data from a specific transaction.
      Transaction const&
      one(std::string const& transaction_id);

      /// @brief Fetch transaction from server (and update it).
      Transaction const&
      sync(std::string const& id);

      /*-----------.
      | Attributes |
      `-----------*/
      ELLE_ATTRIBUTE(plasma::meta::Client const&, meta);
      ELLE_ATTRIBUTE(SelfGetter, self);
      ELLE_ATTRIBUTE(DeviceGetter, device);

      /*----------.
      | Callbacks |
      `----------*/
    private:
      /// @brief Callback when recieving an new transaction.
      void
      _on_transaction(plasma::Transaction const& notif);
    };
  }
}

#endif
