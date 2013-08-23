#include <common/common.hh>
#include <elle/log.hh>
#include <elle/print.hh>
#include <plasma/meta/Client.hh>
#include <tests/unit/ExceptionExpector.hxx>

#define BOOST_TEST_MODULE MetaClient
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#define USER_COUNT 2

/*==============================================================================
  Tested features
  ==============================================================================
  - Register
  - Login
  - Logout
  - Self
  - User from public key
  - Device creation
  - Device update
  - Search
  - User from ID
  - Network creation
  - Network destruction
  - Network data
  - Network add user
  - Network add device
  - Transaction creation
  - Transaction update
  - Connect device
  - Get Endpoints

  ==============================================================================
  XXX: Missing features
  ==============================================================================
  - Swaggers
  - User icon
*/

#define PRETTY_THROW(str)                                               \
  throw std::runtime_error(std::string{__PRETTY_FUNCTION__} + str)

typedef plasma::meta::Client MetaClient;
typedef plasma::meta::Exception Exception;
typedef plasma::meta::Error Error;

static std::map<Error, Exception> const excep{
# define ERR_CODE(name, value, comment)                 \
  {Error::name, Exception(Error::name, comment)},
# include <oracle/disciples/meta/error_code.hh.inc>
# undef ERR_CODE
};

ELLE_LOG_COMPONENT("test.unit.surface.gap.metrics");

struct UniqueUser
{
  std::unique_ptr<MetaClient> client;
  plasma::meta::SelfResponse user;
  std::string email;
  std::string network_id;
  std::string device_id;
  std::string device_name;
  std::string transaction_id;
  std::string local_ip;
  int local_port;
  std::string external_ip;
  int external_port;

  UniqueUser(std::string const& name)
    : client{new plasma::meta::Client{common::meta::host(),
                                      common::meta::port(),
                                      true}}
    , user{}
    , network_id{"99510fbd8ae7798906b80000"}
    , device_id{"aa510fbd8ae7798906b80000"}
    , device_name{device_id + "device"}
    , transaction_id{}
    , local_ip{elle::sprintf("%s.%s.%s.%s",
                             std::rand() % 255,
                             std::rand() % 255,
                             std::rand() % 255,
                             std::rand() % 255)}
    , local_port{std::rand() % 65535}
    , external_ip{elle::sprintf("%s.%s.%s.%s",
                                std::rand() % 255,
                                std::rand() % 255,
                                std::rand() % 255,
                                std::rand() % 255)}
    , external_port{std::rand() % 65535}
  {
    // Initialize user.
    this->user.id = "";
    this->user.fullname = name;
    this->email = name + "@infinit.io";
    this->user.public_key = "";
    this->user.status = 0;
    this->user.identity = "";
  }
};

typedef std::vector<UniqueUser> Users;

/// Globals for register and login.
std::string password = std::string(64, 'c');;
std::string activation_code = "bitebite";

/// Create a list of users.
Users init_users();

//- Users ----------------------------------------------------------------------

/// Meta register.
plasma::meta::RegisterResponse
_register(plasma::meta::Client& client,
          std::string const& email,
          std::string const& fullname,
          std::string const& password,
          std::string const& activation_code);
auto vee_register = test::unit::build_vexception_expector<Exception>(_register);

/// Meta login.
plasma::meta::LoginResponse
_login(plasma::meta::Client& client,
            std::string const& email,
            std::string const& password);
auto vee_login = test::unit::build_vexception_expector<Exception>(_login);

/// Meta logout.
plasma::meta::LogoutResponse
_logout(plasma::meta::Client& client);
auto vee_logout = test::unit::build_vexception_expector<Exception>(_logout);

/// Meta self.
plasma::meta::SelfResponse
_self(plasma::meta::Client& client);
auto vee_self = test::unit::build_vexception_expector<Exception>(_self);

/// Meta user_from_public_key.
plasma::meta::UserResponse
_user_from_public_key(plasma::meta::Client& client,
                      std::string const& public_key);
auto vee_user_from_pkey = test::unit::build_vexception_expector<Exception>(
  _user_from_public_key);

/// Meta user form id or email.
plasma::meta::UserResponse
_user(plasma::meta::Client& client,
      std::string const& user_id);
auto vee_user = test::unit::build_vexception_expector<Exception>(_user);

/// Meta search users.
plasma::meta::UsersResponse
_search_users(plasma::meta::Client& client,
              std::string const& text,
              uint16_t count,
              uint16_t offset);
auto vee_search = test::unit::build_vexception_expector<Exception>(
  _search_users);

//- Devices --------------------------------------------------------------------
/// Meta create_device.
plasma::meta::CreateDeviceResponse
_create_device(plasma::meta::Client& client,
               std::string const& device_name);
auto vee_create_device = test::unit::build_vexception_expector<Exception>(
  _create_device);

bool
_is_device_connect(plasma::meta::Client& client,
                   std::string const& network_id,
                   std::string const& owner_id,
                   std::string const& owner_device_id,
                   UniqueUser const& user);

/// Meta update_device.
plasma::meta::UpdateDeviceResponse
_update_device(plasma::meta::Client& client,
               std::string const& device_id,
               std::string const& device_name);
auto vee_update_device = test::unit::build_vexception_expector<Exception>(
  _update_device);

//- Networks -------------------------------------------------------------------
/// Meta create_network.
plasma::meta::CreateNetworkResponse
_create_network(plasma::meta::Client& client, std::string const& network_name);
auto vee_create_network = test::unit::build_vexception_expector<Exception>(
  _create_network);

/// Meta delete_network.
plasma::meta::DeleteNetworkResponse
_delete_network(plasma::meta::Client& client,
                std::string const& network_id,
                bool force);
auto vee_delete_network = test::unit::build_vexception_expector<Exception>(
  _delete_network);

plasma::meta::NetworkResponse
_network(plasma::meta::Client& client, std::string const& network_id);

int
_count_networks(plasma::meta::Client& client);

void
_check_nodes(plasma::meta::Client& client, std::string const& network_id);

bool
_is_network_in_list(plasma::meta::Client& client, std::string const& network_id);

int
_count_users_in_network(plasma::meta::Client& client,
                        std::string const& network_id);

void
_network_add_user(plasma::meta::Client& client,
                  std::string const& network_id,
                  std::string const& guest_id);
void
_network_add_device(plasma::meta::Client& client,
                    std::string const& network_id,
                    std::string const& peer_device_id);
void
_network_connect_device(plasma::meta::Client& client,
                        std::string const& network_id,
                        UniqueUser const& peer);

//- Transactions ---------------------------------------------------------------
plasma::meta::TransactionResponse
_create_transaction(plasma::meta::Client& client,
                    std::string const& recipient_id,
                    std::string const& network_id,
                    std::string const& device_id);

int _count_transactions(UniqueUser const& u);

void _accept_transactions(UniqueUser const& u);

void _prepare_transactions(UniqueUser const& u);

void _start_transactions(UniqueUser const& u);

void _finish_transactions(UniqueUser const& u);

/// Check known error cases of register.
void
_register_errors();

/// Check known error cases of login.
void _login_errors();

/*----------------.
|  Test wrappers. |
`----------------*/

//- Helpers --------------------------------------------------------------------
/// Ensure none of field of user are empty.
void
_check_user(plasma::meta::User const& u);

/// Compare 2 users, throw if they are different.
void
_compare_users(plasma::meta::User const& u1, plasma::meta::User const& u2);

/// Check if none of the field of a network are empty.
void
_check_network_data(plasma::meta::NetworkResponse const& net, bool check=false);

/// Check if a user is the owner of a network.
void
_check_network_data(plasma::meta::NetworkResponse const& net,
                    UniqueUser const& u);

/// Check if none of the field is empty.
void
_check_transaction_data(plasma::Transaction const& tr);

/// Check if transaction data match sender and recipient.
void
_check_transaction_data(plasma::Transaction const& tr,
                        UniqueUser const& sender,
                        UniqueUser const& recipient);

//- Tests ----------------------------------------------------------------------
/// Try to register every users.
void test_register(Users const& users);
/// Try to log every users.
void test_login(Users& users);
/// Try to logout out every users.
void test_logout(Users const& users);
/// Try some searches.
void test_search(Users const& users);
/// Try to create and update device.
void test_device(Users& users);
/// Try to delete network. Force stands for "gonna fail but don't care."
void test_delete_network(Users const& users, bool force);
/// Try to create networks for users.
void test_create_network(Users& users);
/// Try to execute full transaction process.
void test_transactions(Users& users);

Users
init_users()
{
  // Initialization.
  std::srand(std::time(0));

  Users users;

  for (unsigned int i = 0; i < USER_COUNT; ++i)
  {
    std::string fullname = "_0_random" + std::to_string(std::rand() % 1000000);

    users.emplace_back(fullname);
  }

  return users;
}

//- Users ----------------------------------------------------------------------
plasma::meta::RegisterResponse
_register(plasma::meta::Client& client,
          std::string const& email,
          std::string const& fullname,
          std::string const& password,
          std::string const& activation_code)
{
  ELLE_TRACE_FUNCTION(client, email, fullname, password, activation_code);

  auto res = client.register_(email, fullname, password, activation_code);

  return res;
}

plasma::meta::LoginResponse
_login(plasma::meta::Client& client,
       std::string const& email,
       std::string const& password)
{
  auto const& res = client.login(email, password);

  // _check_user(res);

  BOOST_CHECK(!res.token.empty());

  return res;
}

plasma::meta::LogoutResponse
_logout(plasma::meta::Client& client)
{
  auto res = client.logout();

  BOOST_CHECK(client.email().empty());
  BOOST_CHECK(client.token().empty());
  BOOST_CHECK(client.identity().empty());

  return res;
}

plasma::meta::SelfResponse
_self(plasma::meta::Client& client)
{
  auto res = client.self();

  _check_user(res);

  BOOST_CHECK(!res.identity.empty());

  return res;
}

plasma::meta::UserResponse
_user_from_public_key(plasma::meta::Client& client,
                      std::string const& public_key)
{
  auto res = client.user_from_public_key(public_key);

  _check_user(res);

  return res;
}

plasma::meta::UserResponse
_user(plasma::meta::Client& client,
      std::string const& user_id)
{
  auto res = client.user(user_id);

  _check_user(res);

  return res;
}

plasma::meta::UsersResponse
_search_users(plasma::meta::Client& client,
              std::string const& text,
              uint16_t count,
              uint16_t offset)
{
  auto res = client.search_users(text, count, offset);

  BOOST_CHECK(res.users.size() <= count);

  return res;
}

plasma::meta::CreateDeviceResponse
_create_device(plasma::meta::Client& client,
               std::string const& device_name)
{
  auto res = client.create_device(device_name);

  BOOST_CHECK(!res.id.empty());
  BOOST_CHECK(!res.passport.empty());

  return res;
}

plasma::meta::UpdateDeviceResponse
_update_device(plasma::meta::Client& client,
               std::string const& device_id,
               std::string const& device_name)
{
  auto res = client.update_device(device_id,
                                  device_name);

  BOOST_CHECK(!res.id.empty());
  BOOST_CHECK(!res.passport.empty());

  return res;
}

void
_register_errors()
{
  MetaClient serv{common::meta::host(), common::meta::port(), true};
  vee_register(excep.at(Error::email_not_valid),
               serv, "", "", "", "");
  vee_register(excep.at(Error::email_not_valid),
               serv, "no_at", "", "", "");
  vee_register(excep.at(Error::email_not_valid),
               serv, "a@a.a", "", "", "");
  vee_register(excep.at(Error::handle_not_valid),
               serv, "valid@mail.fr", "", "", "");
  vee_register(excep.at(Error::handle_not_valid),
               serv, "valid@mail.fr", "a", "", "");
  vee_register(excep.at(Error::password_not_valid),
               serv, "valid@mail.fr", "castor", "", "");
  vee_register(excep.at(Error::password_not_valid),
               serv, "valid@mail.fr", "castor", "aa", "");
  vee_register(excep.at(Error::activation_code_doesnt_exist),
               serv, "valid@mail.fr", "castor", password, "");
  vee_register(excep.at(Error::activation_code_doesnt_exist),
               serv, "valid@mail.fr", "castor", password, "ae392813");
}

void
_login_errors()
{
  MetaClient serv{common::meta::host(), common::meta::port(), true};
  vee_login(excep.at(Error::email_not_valid), serv, "", "");
  vee_login(excep.at(Error::email_not_valid), serv, "no_at", "");
  vee_login(excep.at(Error::password_not_valid),
            serv, "valid@infinit.io", "");
  vee_login(excep.at(Error::password_not_valid),
            serv, "valid@infinit.io", "bisou");
  vee_login(excep.at(Error::email_password_dont_match),
            serv, "valid@infinit.io", std::string(64, 'e'));

}

//- Networks -------------------------------------------------------------------
plasma::meta::CreateNetworkResponse
_create_network(plasma::meta::Client& client,
                std::string const& network_name)
{
  auto res = client.create_network(network_name);

  BOOST_CHECK(res.created_network_id.length() != 0);

  return res;
}

plasma::meta::DeleteNetworkResponse
_delete_network(plasma::meta::Client& client,
                std::string const& network_id,
                bool force)
{
  auto res = client.delete_network(network_id, force);

  if (!force && res.deleted_network_id != network_id)
    PRETTY_THROW(elle::sprintf("netorks id don't match",
                               res.error_details));

  return res;
}


plasma::meta::NetworkResponse
_network(plasma::meta::Client& client, std::string const& network_id)
{
  auto res = client.network(network_id);

  _check_network_data(res);

  return res;
}


int
_count_networks(plasma::meta::Client& client)
{
  auto res = client.networks();

  return res.networks.size();
}

void
_check_nodes(plasma::meta::Client& client,
             std::string const& network_id)
{
  auto res = client.network_nodes(network_id);

  //XXX
}

bool
_is_network_in_list(plasma::meta::Client& client,
                    std::string const& network_id)
{
  auto res = client.networks();

  auto found = std::find_if(res.networks.begin(),
                            res.networks.end(),
                            [&](std::string n) -> bool
                            { return (n == network_id); });

  if (found == res.networks.end())
    return false;

  return true;
}

int
_count_users_in_network(plasma::meta::Client& client,
                        std::string const& network_id)
{
  auto res = client.network(network_id);

  return res.users.size();
}

void
_network_add_user(plasma::meta::Client& client,
                  std::string const& network_id,
                  std::string const& guest_id)
{
  auto res = client.network_add_user(network_id, guest_id);

  BOOST_CHECK(res.updated_network_id == network_id);
}

void
_network_add_device(plasma::meta::Client& client,
                    std::string const& network_id,
                    std::string const& peer_device_id)
{
  auto res = client.network_add_device(network_id, peer_device_id);

  BOOST_CHECK(res.updated_network_id == network_id);
}

void
_network_connect_device(plasma::meta::Client& client,
                        std::string const& network_id,
                        UniqueUser const& peer)
{
  auto res = client.network_connect_device(network_id,
                                           peer.device_id,
                                           &peer.local_ip,
                                           peer.local_port,
                                           &peer.external_ip,
                                           peer.external_port);

  BOOST_CHECK(res.updated_network_id == network_id);
}

bool
_is_device_connect(plasma::meta::Client& client,
                   std::string const& network_id,
                   std::string const& owner_id,
                   std::string const& owner_device_id,
                   UniqueUser const& u)
{
  auto res = client.device_endpoints(network_id,
                                     owner_device_id,
                                     u.device_id);

  if (res.externals.empty() && owner_id != u.user.id)
    return false;
  if (res.locals.empty())
    return false;

  std::string peer_ext = u.external_ip + ":" + std::to_string(u.external_port);
  auto itext = std::find(res.externals.begin(), res.externals.end(),
                         peer_ext);

  if (itext == res.externals.end() && owner_id != u.user.id)
    return false;
  std::string peer_locals = u.local_ip + ":" + std::to_string(u.local_port);
  auto itloc = std::find(res.locals.begin(), res.locals.end(),
                         peer_locals);

  if (itloc == res.locals.end())
    return false;

  return true;
}


//XXX: This may failed, due to random.
void
test_register(Users const& users)
{
  _register_errors();

  for(UniqueUser const& u: users)
  {
    _register(*u.client, u.email, u.user.fullname, password, activation_code);

    vee_register(excep.at(Error::email_already_registred),
                 *u.client,
                 u.email,
                 u.user.fullname,
                 password,
                 activation_code);
  }
}

void
test_login(Users& users)
{
  _login_errors();

  for (UniqueUser& u: users)
  {
    // Log user in.
    _login(*u.client, u.email, password);

    // Get data using self.
    u.user = _self(*u.client);

    // Get data using public key search.
    _user_from_public_key(*u.client,
                          u.user.public_key);
  }
}

void
test_device(Users& users)
{
  for (UniqueUser& u: users)
  {
    {
      auto const& created_device = _create_device(*u.client, u.device_name);
      u.device_id = created_device.id;
    }
    u.device_name += "_new";
    {
      auto const& device = _update_device(*u.client, u.device_id, u.device_name);
      u.device_id = device.id;
    }
  }
}

void
test_logout(Users const& users)
{
  // Logout every users.
  for (UniqueUser const& u: users)
  {
    _logout(*u.client);
  }
}

void
test_search(Users const& users)
{
  std::string prefix("random");

  // Search users with fullname starting by prefix.
  for (UniqueUser const& u: users)
  {
    auto res = _search_users(*u.client, prefix, USER_COUNT, 0);

    // XXX: this fails for now, fix it Antony.
    // BOOST_CHECK(res.users.size() >= USER_COUNT);

    // Ensure search returned only users with the good fullname.
    for (auto user_id: res.users)
    {
      auto user = _user(*u.client, user_id);

      BOOST_CHECK(user.id == user_id);
      BOOST_CHECK(!user.fullname.compare(0, prefix.size(), prefix));
    }
  }
}

void
test_delete_network(Users const& users, bool force)
{
  for (UniqueUser const& u: users)
  {
    int count = _count_networks(*u.client);
    _delete_network(*u.client, u.network_id, force);
    if (!force && _count_networks(*u.client) != (count - 1))
      PRETTY_THROW("network count is wrong");
    BOOST_CHECK(!_is_network_in_list(*u.client, u.network_id));
  }
}

void
test_create_network(Users& users)
{
  for (UniqueUser& u: users)
  {
    // count networks before creation.
    int count = _count_networks(*u.client);

    auto cnet_res = _create_network(*u.client, u.user.fullname);
    auto net_res = _network(*u.client, cnet_res.created_network_id);

    _check_network_data(net_res);

    u.network_id = cnet_res.created_network_id;

    _check_nodes(*u.client, u.network_id);

    BOOST_CHECK(_count_networks(*u.client) == (count + 1));
    BOOST_CHECK(_is_network_in_list(*u.client, u.network_id));


    // Check if number of user in network is 1 (the user only).
    BOOST_CHECK(_count_users_in_network(*u.client, u.network_id) == 1);

    // Add users in network.
    for (UniqueUser const& guest: users)
    {
      if (guest.user.id != u.user.id)
        _network_add_user(*u.client, u.network_id, guest.user.id);

      _network_add_device(*u.client, u.network_id, guest.device_id);
      _network_connect_device(*u.client,
                              u.network_id,
                              guest);
    }

    for (UniqueUser const& guest: users)
    {
      BOOST_CHECK(_is_device_connect(*u.client,
                                     u.network_id,
                                     u.user.id,
                                     u.device_id,
                                     guest));
    }

    // Count user in the network. Should be USER_COUNT.
    BOOST_CHECK(_count_users_in_network(*u.client, u.network_id) == USER_COUNT);
  }
}

void
test_transactions(Users& users)
{
  for (UniqueUser& u: users)
  {
    for (UniqueUser const& recipient: users)
    {
      // Can't send to your self (on the same device).
      if (recipient.user.id == u.user.id)
        continue;

      auto transaction = _create_transaction(*u.client,
                                             recipient.user.id,
                                             recipient.network_id,
                                             recipient.device_id);

      _check_transaction_data(transaction);
    }
  }

  // Accept each transaction.
  for (UniqueUser& u: users)
    _accept_transactions(u);
  // Prepare each transaction.
  for (UniqueUser& u: users)
    _prepare_transactions(u);
  // Start each transaction.
  for (UniqueUser& u: users)
    _start_transactions(u);
  // Finish each transaction.
  for (UniqueUser& u: users)
    _finish_transactions(u);
}

//- Transactions ---------------------------------------------------------------
std::string filename = "beaver.txt";
int count = 1;
int size = 9139;
bool is_dir = false;

plasma::meta::TransactionResponse
_create_transaction(plasma::meta::Client& client,
                    std::string const& recipient_id,
                    std::string const& network_id,
                    std::string const& device_id)
{
  auto res = client.create_transaction(recipient_id,
                                       filename,
                                       count,
                                       size,
                                       is_dir,
                                       network_id,
                                       device_id);

  auto transaction = client.transaction(res.created_transaction_id);

  _check_transaction_data(transaction);

  return transaction;
}

int
_count_transactions(UniqueUser const& u)
{
  auto res = u.client->transactions();

  return res.transactions.size();
}

void
_accept_transactions(UniqueUser const& u)
{
  int count = _count_transactions(u);
  // I send to the (USER_COUNT - 1) other users and every users did the same.
  if (count != (USER_COUNT - 1) * 2)
    PRETTY_THROW(
      elle::sprintf("wrong number of notif. expected %i, found %i",
                    (USER_COUNT - 1) * 2, count));

  auto res = u.client->transactions();

  for (std::string const& transaction_id: res.transactions)
  {
    auto transaction_res = u.client->transaction(transaction_id);

    // Only recipient can accept.
    if (transaction_res.recipient_id != u.user.id)
      continue;

    u.client->update_transaction(transaction_id,
                                 plasma::TransactionStatus::accepted,
                                 u.device_id,
                                 u.device_name);

  }
}

void
_prepare_transactions(UniqueUser const& u)
{
  int count = _count_transactions(u);
  // I send to the (USER_COUNT - 1) other users and every users did the same.
  BOOST_CHECK(count == (USER_COUNT - 1) * 2);

  auto res = u.client->transactions();

  for (std::string const& transaction_id: res.transactions)
  {
    auto transaction_res = u.client->transaction(transaction_id);

    // Only recipient can accept.
    if (transaction_res.sender_id != u.user.id)
      continue;

    u.client->update_transaction(transaction_id,
                                 plasma::TransactionStatus::prepared);

  }
}

void
_start_transactions(UniqueUser const& u)
{
  int count = _count_transactions(u);
  // I send to the (USER_COUNT - 1) other users and every users did the same.
  BOOST_CHECK(count == (USER_COUNT - 1) * 2);

  auto res = u.client->transactions();

  for (std::string const& transaction_id: res.transactions)
  {
    auto transaction_res = u.client->transaction(transaction_id);

    // Only recipient can prepare.
    if (transaction_res.recipient_id != u.user.id)
      continue;

    u.client->update_transaction(transaction_id,
                                 plasma::TransactionStatus::started);

  }
}

void
_finish_transactions(UniqueUser const& u)
{
  auto res = u.client->transactions();

  for (std::string const& transaction_id: res.transactions)
  {
    auto transaction_res = u.client->transaction(transaction_id);

    // Only recipient can finish.
    if (transaction_res.recipient_id != u.user.id)
      continue;

    u.client->update_transaction(transaction_id,
                                 plasma::TransactionStatus::finished);

  }
}


//- Helpers --------------------------------------------------------------------
/// Ensure none of field of user are empty.
void
_check_user(plasma::meta::User const& u)
{
  BOOST_CHECK(!u.id.empty());
  BOOST_CHECK(!u.fullname.empty());
  BOOST_CHECK(!u.public_key.empty());
}

void
_compare_users(plasma::meta::User const& u1,
               plasma::meta::User const& u2)
{
  BOOST_CHECK_EQUAL(u1.id, u2.id);
  BOOST_CHECK_EQUAL(u1.fullname, u2.fullname);
  BOOST_CHECK_EQUAL(u1.public_key, u2.public_key);
}

void
_check_network_data(plasma::meta::NetworkResponse const& net, bool check)
{
  BOOST_CHECK(!net._id.empty());
  BOOST_CHECK(!net.owner.empty());
  BOOST_CHECK(!net.name.empty());
  BOOST_CHECK(!net.model.empty());
  BOOST_CHECK(!net.users.empty());

  if (check)
  {
    BOOST_CHECK(!net.root_block.empty());
    BOOST_CHECK(!net.root_address.empty());
    BOOST_CHECK(!net.group_block.empty());
    BOOST_CHECK(!net.group_address.empty());
    BOOST_CHECK(!net.descriptor.empty());
  }
}

void
_check_network_data(plasma::meta::NetworkResponse const& net,
                    UniqueUser const& u)
{
  BOOST_CHECK_EQUAL(net.owner, u.user.id);
  BOOST_CHECK_EQUAL(net._id, u.network_id);
}

void
_check_transaction_data(plasma::Transaction const& tr)
{
  BOOST_CHECK(!tr.id.empty());
  BOOST_CHECK(!tr.sender_id.empty());
  BOOST_CHECK(!tr.sender_fullname.empty());
  BOOST_CHECK(!tr.sender_device_id.empty());
  BOOST_CHECK(!tr.recipient_id.empty());
  BOOST_CHECK(!tr.recipient_fullname.empty());
  BOOST_CHECK(!tr.network_id.empty());
  BOOST_CHECK(!!tr.message.empty());
  BOOST_CHECK(!tr.files.empty());
  BOOST_CHECK_PREDICATE(std::not_equal_to<int>(), (tr.files_count)(0));
  BOOST_CHECK_PREDICATE(std::not_equal_to<int>(), (tr.total_size)(0));

  if (tr.status == 1) // Pending.
  {
    BOOST_CHECK(tr.recipient_device_id.empty());
    BOOST_CHECK(tr.recipient_device_name.empty());
  }
  else if (tr.status != 5) // not canceled.
  {
    BOOST_CHECK(!tr.recipient_device_id.empty());
    BOOST_CHECK(!tr.recipient_device_name.empty());
  }
}

void
_check_transaction_data(plasma::Transaction const& tr,
                        UniqueUser const& sender,
                        UniqueUser const& recipient)
{
  BOOST_CHECK(!tr.id.empty());
  BOOST_CHECK_EQUAL(tr.sender_id, sender.user.id);
  BOOST_CHECK_EQUAL(tr.sender_fullname, sender.user.fullname);
  BOOST_CHECK_EQUAL(tr.sender_device_id, sender.device_id);
  BOOST_CHECK_EQUAL(tr.recipient_id, recipient.user.id);
  BOOST_CHECK_EQUAL(tr.recipient_fullname, recipient.user.fullname);
  BOOST_CHECK_EQUAL(tr.network_id, sender.network_id);
  BOOST_CHECK(tr.message.empty());
  BOOST_CHECK_EQUAL(tr.files[0], filename);
  BOOST_CHECK_EQUAL(tr.files_count, count);
  BOOST_CHECK_EQUAL(tr.total_size, size);
  BOOST_CHECK_EQUAL(tr.is_directory, is_dir);

  if (tr.status == 1)
  {
    BOOST_CHECK(tr.recipient_device_id.empty());
    BOOST_CHECK(tr.recipient_device_name.empty());
  }
  else if (tr.status != 5)
  {
    BOOST_CHECK_EQUAL(tr.recipient_device_id, recipient.device_id);
    BOOST_CHECK_EQUAL(tr.recipient_device_name, recipient.device_name);
  }
}

Users users = init_users();

bool
test_suite()
{
  ELLE_DEBUG("creating test_suite");
  boost::unit_test::test_suite* basics = BOOST_TEST_SUITE("Basics");
  ELLE_DEBUG("add test_suite");
  boost::unit_test::framework::master_test_suite().add(basics);

  return true;
}

BOOST_AUTO_TEST_CASE(ultra_test)
{
  test_register(users);
  test_login(users);
  test_logout(users);
  test_login(users);
  test_device(users);
  test_search(users);
  test_create_network(users);
  test_transactions(users);
  test_delete_network(users, false);
  test_delete_network(users, true);
}
