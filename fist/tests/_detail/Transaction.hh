#ifndef FIST_SURFACE_GAP_TESTS_TRANSACTION_HH
# define FIST_SURFACE_GAP_TESTS_TRANSACTION_HH

#include <surface/gap/State.hh>

namespace tests
{
  class Transaction
    : public infinit::oracles::PeerTransaction
  {
  public:
    Transaction(std::string const& sender_id,
                std::string const& recipient_id);
    ~Transaction() noexcept(true);
    ELLE_ATTRIBUTE_RX(
      boost::signals2::signal<void (infinit::oracles::Transaction::Status)>,
      status_changed);
    ELLE_ATTRIBUTE_RX(std::unique_ptr<infinit::oracles::meta::CloudCredentials>,
                      cloud_credentials);
    friend class Server;

    std::string const&
    id_getter() const;

    void
    print(std::ostream& out) const override;

    std::string
    json();
  };
}

#endif
