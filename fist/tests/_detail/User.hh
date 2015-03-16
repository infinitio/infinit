#ifndef FIST_SURFACE_GAP_TESTS_USER_HH
# define FIST_SURFACE_GAP_TESTS_USER_HH

# include <papier/Identity.hh>

# include <fist/tests/_detail/Device.hh>
# include <fist/tests/_detail/State.hh>

namespace tests
{
  class Server;

  class User
    : public elle::Printable
  {
  public:
    static
    std::string
    link_representation(infinit::oracles::LinkTransaction const& link);

  public:
    User(elle::UUID id,
         std::string email,
         boost::optional<cryptography::KeyPair> keys,
         std::unique_ptr<papier::Identity> identity);
    User(User const&) = default;

    ELLE_ATTRIBUTE_R(elle::UUID, id);
    ELLE_ATTRIBUTE_R(std::string, email);
    ELLE_ATTRIBUTE_R(boost::optional<cryptography::KeyPair>, keys);
    ELLE_ATTRIBUTE_RX(std::unique_ptr<papier::Identity>, identity);
    ELLE_ATTRIBUTE_R(std::string, facebook_id);
    typedef std::unordered_set<User*> Swaggers;
    Swaggers swaggers;
    typedef std::unordered_set<elle::UUID> Devices;
    Devices devices;
    Devices connected_devices;
    typedef std::unordered_map<std::string, infinit::oracles::LinkTransaction> Links;
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

    bool
    ghost() const;

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
