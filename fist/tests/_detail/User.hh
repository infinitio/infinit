#ifndef FIST_SURFACE_GAP_TESTS_USER_HH
# define FIST_SURFACE_GAP_TESTS_USER_HH

# include <algorithm>

# include <elle/UUID.hh>

# include <papier/Identity.hh>

# include <fist/tests/_detail/Device.hh>
# include <fist/tests/_detail/State.hh>

namespace tests
{
  class Server;

  class User
    : public infinit::oracles::meta::User
  {
  public:
    User(elle::UUID id,
         std::string email,
         boost::optional<infinit::cryptography::rsa::KeyPair> keys,
         std::unique_ptr<papier::Identity> identity);
    User(User const&) = default;

    std::string const&
    bmi_id() const
    {
      return this->id;
    }
    ELLE_ATTRIBUTE_R(std::string, email);
    ELLE_ATTRIBUTE_R(boost::optional<infinit::cryptography::rsa::KeyPair>, keys);
    ELLE_ATTRIBUTE_RX(std::unique_ptr<papier::Identity>, identity);
    ELLE_ATTRIBUTE_R(std::string, facebook_id);
    typedef std::unordered_set<User*> Swaggers;
    Swaggers swaggers;
    std::unordered_set<elle::UUID> devices;
    typedef std::unordered_map<std::string, infinit::oracles::LinkTransaction> Links;
  public:
    Links links;
    Links new_links;

    void
    add_connected_device(elle::UUID const& device_id);

    void
    remove_connected_device(elle::UUID const& device_id);

    std::string
    json() const;

    void
    print(std::ostream& stream) const override;
  };

  class Client
  {
  public:
    Client(Server& server,
           User const& user,
           boost::filesystem::path const& home_path = boost::filesystem::path{});

    Client(Server& server,
           std::string const& email,
           boost::filesystem::path const& home_path = boost::filesystem::path{});

    virtual
    ~Client();

  public:
    void
    login(std::string const& password = "password");

    void
    logout();

  private:
    Server& _server;

  public:
    elle::UUID device_id;
    User& user;
    State state;
  };
}

#endif
