#ifndef FIST_SURFACE_GAP_TESTS_SERVER_HH
# define FIST_SURFACE_GAP_TESTS_SERVER_HH

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <cryptography/KeyPair.hh>

#include <elle/reactor/tests/http_server.hh>

#include <papier/Passport.hh>

#include <fist/tests/_detail/uuids.hh>
#include <fist/tests/_detail/User.hh>
#include <fist/tests/_detail/Device.hh>
#include <fist/tests/_detail/Transaction.hh>
#include <fist/tests/_detail/Trophonius.hh>

namespace bmi = boost::multi_index;

namespace tests
{
  class Server
    : public reactor::http::tests::Server
  {
  public:

  public:
    Server();
    Server(Server const&) = default;

    User const&
    register_user(std::string const& email,
                  std::string const& password = "password");
    User const&
    facebook_connect(std::string const& token);

    void
    register_device(User const& user,
                    boost::optional<boost::uuids::uuid> device);

    Client
    client(std::string const& email,
           std::string const& password);

    User const&
    generate_ghost_user(std::string const& email);

    void
    session_id(boost::uuids::uuid id);
    Transaction&
    transaction(std::string const& id);

    User const&
    user(Cookies const& cookies) const;

    Device const&
    device(Cookies const& cookies) const;

  protected:
    virtual
    boost::uuids::uuid
    _create_empty();

    virtual
    std::string
    _transaction_put(Headers const&,
                     Cookies const&,
                     Parameters const&,
                     elle::Buffer const&,
                     boost::uuids::uuid const&);

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
    typedef
    bmi::const_mem_fun<User, std::string const&, &User::facebook_id>
    FacebookId;
    typedef boost::multi_index_container<
      User,
      bmi::indexed_by<bmi::hashed_unique<UserId>,
                      bmi::hashed_non_unique<UserEmail>,
                      bmi::hashed_unique<FacebookId>>
      > Users;
    ELLE_ATTRIBUTE_R(Users, users);
    typedef
    bmi::const_mem_fun<Transaction, std::string const&, &Transaction::id_getter>
    TransactionId;
    typedef boost::multi_index_container<
      std::unique_ptr<Transaction>,
      bmi::indexed_by<bmi::hashed_unique<TransactionId>>
      > Transactions;
    ELLE_ATTRIBUTE_R(Transactions, transactions);
    ELLE_ATTRIBUTE_R(bool, cloud_buffered);
  };
}

#endif
