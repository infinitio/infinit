#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cryptography/KeyPair.hh>

#include <reactor/network/ssl-server.hh>
#include <reactor/scheduler.hh>

#include <elle/reactor/tests/http_server.hh>

#include <elle/serialization/json.hh>

#include <papier/Identity.hh>
#include <papier/Passport.hh>

#include <surface/gap/State.hh>

namespace bmi = boost::multi_index;


namespace std
{
  template <>
  struct hash<boost::uuids::uuid>
  {
  public:
    std::size_t operator()(boost::uuids::uuid const& s) const;
  };
}

papier::Identity
generate_identity(cryptography::KeyPair const& keypair,
                    std::string const& id,
                    std::string const& description,
                    std::string const& password);


class Server;

class State
  : public surface::gap::State
{
public:
  State(Server& server,
        boost::uuids::uuid device_id,
        boost::filesystem::path path = boost::filesystem::path());
};

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

public:
  std::string
  json() const;

  void
  disconnect_all_users();

private:
  ELLE_ATTRIBUTE(reactor::network::SSLServer, server);
  ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter);
};

class Server
  : public reactor::http::tests::Server
{
public:
  /*-------.
  | Device |
  `-------*/
  class Device
  {
  public:
    typedef boost::uuids::uuid Id;
  public:
    Device(cryptography::PublicKey const& key);
  private:
    ELLE_ATTRIBUTE_R(Id, id);
    ELLE_ATTRIBUTE_R(papier::Passport, passport);

  public:
    std::string
    json() const;
  };

  /*-----.
  | User |
  `-----*/
  class User
    : public elle::Printable
  {
  public:
    User(boost::uuids::uuid id,
         boost::optional<boost::uuids::uuid> device_id,
         std::string email,
         boost::optional<cryptography::KeyPair> keys,
         boost::optional<papier::Identity> identity);
    User(User const&) = default;

    ELLE_ATTRIBUTE_R(boost::uuids::uuid, id);
    ELLE_ATTRIBUTE_R(boost::optional<boost::uuids::uuid>, device_id);
    ELLE_ATTRIBUTE_R(std::string, email);
    ELLE_ATTRIBUTE_R(boost::optional<cryptography::KeyPair>, keys);
    ELLE_ATTRIBUTE_R(boost::optional<papier::Identity>, identity);
    typedef std::unordered_set<User*> Swaggers;
    Swaggers swaggers;
    typedef std::unordered_set<Device::Id> Devices;
    Devices connected_devices;
    typedef std::vector<infinit::oracles::LinkTransaction> Links;
  public:
    Links links;
    Links new_links;

    std::string
    json() const;

    std::string
    self_json() const;

    std::string
    links_json() const;

    std::string
    devices_json() const;

    std::string
    swaggers_json() const;

    void
    print(std::ostream& stream) const override;
  };

  /*-------.
  | Client |
  `-------*/

  class Client
  {
  public:
    Client(Server& server,
           User& user,
           boost::filesystem::path home = boost::filesystem::path());
    Client(Server& server,
           std::string const& email);

  public:
    void
    login(std::string const& password = "password");

    void
    logout();

  private:
    Server& _server;

  public:
    User& user;
    State state;
  };

  /*------------.
  | Transaction |
  `------------*/
  class Transaction
    : public elle::Printable
  {
  public:
    Transaction();
    ELLE_ATTRIBUTE_R(boost::uuids::uuid, id);
    ELLE_ATTRIBUTE_RW(infinit::oracles::Transaction::Status, status);
    ELLE_ATTRIBUTE_RX(
      boost::signals2::signal<void (infinit::oracles::Transaction::Status)>,
      status_changed);
    friend class Server;

    void
    print(std::ostream& out) const override;
  };

public:
  Server();
  Server(Server const&) = default;


  User&
  register_user(std::string const& email, std::string const& password);
  User&
  generate_ghost_user(std::string const& email);
  void
  session_id(boost::uuids::uuid id);
  Transaction&
  transaction(std::string const& id);

  User&
  user(Cookies const& cookies) const;

protected:
  virtual
  boost::uuids::uuid
  _create_empty();
  virtual
  std::string
  _transaction_put(Server::Headers const&,
                   Server::Cookies const& cookies,
                   Server::Parameters const&,
                   elle::Buffer const& content,
                   boost::uuids::uuid const& id);
  std::string
  _get_trophonius(Headers const&,
                  Cookies const&,
                  Parameters const&,
                  elle::Buffer const&) const;

  ELLE_ATTRIBUTE_R(boost::uuids::uuid, session_id)
  Trophonius trophonius;
  // Device.
  typedef std::unordered_map<boost::uuids::uuid, Device> Devices;
  ELLE_ATTRIBUTE_R(Devices, devices);
  // Users.
  typedef
    bmi::const_mem_fun<User, boost::uuids::uuid const&, &User::id>
    UserId;
  typedef
    bmi::const_mem_fun<User, std::string const&, &User::email>
    UserEmail;
  typedef boost::multi_index_container<
    std::unique_ptr<User>,
    bmi::indexed_by<bmi::hashed_unique<UserId>,
                    bmi::hashed_unique<UserEmail>>
    > Users;
  ELLE_ATTRIBUTE_R(Users, users);
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
