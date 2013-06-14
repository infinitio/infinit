#ifndef SURFACE_GAP_CREATETRANSACTIONOPERATION_HH
# define SURFACE_GAP_CREATETRANSACTIONOPERATION_HH

# include "OperationManager.hh"
# include "TransactionManager.hh"
# include "NetworkManager.hh"
# include "UserManager.hh"

# include <plasma/meta/Client.hh>

# include <elle/log.hh>

# include <string>
# include <unordered_set>

namespace surface
{
  namespace gap
  {
    class CreateTransactionOperation:
      public Operation
    {
      TransactionManager& _transaction_manager;
      NetworkManager& _network_manager;
      UserManager& _user_manager;
      elle::metrics::Reporter& _reporter;
      plasma::meta::Client& _meta;
      plasma::meta::SelfResponse& _me;
      std::string _device_id;
      std::string _recipient_id_or_email;
      std::unordered_set<std::string> _files;
      std::string _transaction_id;
      std::string _network_id;
      std::function<void(std::string const&)> _on_transaction;

    public:
      CreateTransactionOperation(TransactionManager& transaction_manager,
                                 NetworkManager& network_manager,
                                 UserManager& user_manager,
                                 plasma::meta::Client& meta,
                                 elle::metrics::Reporter& reporter,
                                 plasma::meta::SelfResponse& me,
                                 std::string const& device_id,
                                 std::string const& recipient_id_or_email,
                                 std::unordered_set<std::string> const& files,
                                 std::function<void(std::string const&)> cb);

    protected:
      void
      _run() override;

      void
      _cancel() override;
    };
  }
}

#endif
