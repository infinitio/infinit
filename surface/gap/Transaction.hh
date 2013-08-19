#ifndef SURFACE_GAP_DETAIL_TRANSACTION_HH
# define SURFACE_GAP_DETAIL_TRANSACTION_HH

# include <surface/gap/Notification.hh>
# include <surface/gap/TransferMachineStatus.hh>
# include <surface/gap/fwd.hh>
# include <surface/gap/Exception.hh>
# include <surface/gap/TransferMachine.hh>


# include <plasma/fwd.hh>
# include <plasma/plasma.hh>

# include <unordered_set>

# include <stdint.h>
# include <string>

namespace surface
{
  namespace gap
  {
    class Transaction
    {
      // - Exception -----------------------------------------------------------
      class BadOperation:
        public Exception
      {
      public:
        enum class Type
        {
          accept,
          reject,
        };

        BadOperation(Type type);

        ELLE_ATTRIBUTE_R(Type, type);
      };

    public:
      typedef plasma::Transaction Data;

    public:
      Transaction(State const& state,
                  Data&& data);

      Transaction(Transaction&&) = default;

      Transaction(surface::gap::State const& state,
                  std::string const& peer_id,
                  std::unordered_set<std::string>&& files);
      void
      accept();

      void
      reject();

      void
      cancel();

      void
      join();

      void
      on_transaction_update(Data const& data);

      void
      on_peer_connection_update(
        plasma::trophonius::PeerConnectionUpdateNotification const& update);

      /*------------.
      | Atttributes |
      `------------*/
      ELLE_ATTRIBUTE_R(uint32_t, id);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
      ELLE_ATTRIBUTE(std::unique_ptr<TransferMachine>, machine);

      /*--------.
      | Helpers |
      `--------*/
    public:
      bool
      concerns_user(std::string const& peer_id) const;

      bool
      concerns_device(std::string const& device_id) const;

      bool
      has_transaction_id(std::string const& id) const;

      bool
      concerns_network(std::string const& network_id) const;
    };
  }
}

#include "Transaction.hxx"

#endif
