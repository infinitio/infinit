#ifndef FIST_SURFACE_GAP_TESTS_TRANSACTION_HH
# define FIST_SURFACE_GAP_TESTS_TRANSACTION_HH

#include <surface/gap/State.hh>

namespace tests
{
  class Transaction
    : public infinit::oracles::PeerTransaction
  {
  public:
    Transaction();
    ~Transaction() noexcept (true);
    ELLE_ATTRIBUTE_RX(
      boost::signals2::signal<void (infinit::oracles::Transaction::Status)>,
      status_changed);
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
