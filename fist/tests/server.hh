#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/uuid/uuid.hpp>

#include <cryptography/KeyPair.hh>

#include <reactor/network/ssl-server.hh>
#include <reactor/scheduler.hh>

#include <elle/reactor/tests/http_server.hh>

#include <papier/Identity.hh>

#include <surface/gap/State.hh>

namespace bmi = boost::multi_index;

papier::Identity
generate_identity(cryptography::KeyPair const& keypair,
                    std::string const& id,
                    std::string const& description,
                    std::string const& password);

class Trophonius
{
public:
  Trophonius();
  ~Trophonius();
  int
  port() const;

protected:
  virtual
  void
  _serve();
  virtual
  void
  _serve(std::unique_ptr<reactor::network::SSLSocket> socket);

private:
  ELLE_ATTRIBUTE(reactor::network::SSLServer, server);
  ELLE_ATTRIBUTE(reactor::Thread, accepter);
};

class Server
  : public reactor::http::tests::Server
{
public:
  Server();
  class User
  {
  public:
    User(boost::uuids::uuid id,
         boost::uuids::uuid device_id,
         std::string email,
         cryptography::KeyPair keys,
         papier::Identity identity);
    ELLE_ATTRIBUTE_R(boost::uuids::uuid, id);
    ELLE_ATTRIBUTE_R(boost::uuids::uuid, device_id);
    ELLE_ATTRIBUTE_R(std::string, email);
    ELLE_ATTRIBUTE_R(cryptography::KeyPair, keys);
    ELLE_ATTRIBUTE_R(papier::Identity, identity);
  };
  class Transaction
  {
  public:
    Transaction();
    ELLE_ATTRIBUTE_R(boost::uuids::uuid, id);
    ELLE_ATTRIBUTE_R(infinit::oracles::Transaction::Status, status);
    ELLE_ATTRIBUTE_RX(
      boost::signals2::signal<void (infinit::oracles::Transaction::Status)>,
      status_changed);
    friend class Server;
  };
  User&
  register_user(std::string const& email, std::string const& password);
  boost::uuids::uuid
  generate_ghost_user(std::string const& email);
  void
  session_id(boost::uuids::uuid id);
  Transaction&
  transaction(std::string const& id);

protected:
  std::string
  _get_trophonius(Headers const&,
                    Cookies const&,
                    Parameters const&,
                    elle::Buffer const&) const;

  ELLE_ATTRIBUTE_R(boost::uuids::uuid, session_id)
  ELLE_ATTRIBUTE_R(Trophonius, trophonius);
  typedef
    bmi::const_mem_fun<User, boost::uuids::uuid const&, &User::id>
    UserId;
  typedef
    bmi::const_mem_fun<User, std::string const&, &User::email>
    UserEmail;
  typedef boost::multi_index_container<
    std::unique_ptr<User>,
    bmi::indexed_by<bmi::hashed_unique<UserId>, bmi::hashed_unique<UserEmail>>
    > Users;
  ELLE_ATTRIBUTE(Users, users);
  typedef
    bmi::const_mem_fun<Transaction, boost::uuids::uuid const&, &Transaction::id>
    TransactionId;
  typedef boost::multi_index_container<
    std::unique_ptr<Transaction>,
    bmi::indexed_by<bmi::hashed_unique<TransactionId>>
    > Transactions;
  ELLE_ATTRIBUTE(Transactions, transactions);
  ELLE_ATTRIBUTE_R(bool, cloud_buffered);
};

class State
  : public surface::gap::State
{
public:
  State(Server& server, boost::uuids::uuid device_id);
};
