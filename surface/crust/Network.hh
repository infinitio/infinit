#ifndef NETWORK_HH
# define NETWORK_HH

# include <infinit/Identifier.hh>

# include <plasma/meta/Client.hh>

# include <version.hh>
# include <infinit/Descriptor.hh>
# include <surface/crust/Authority.hh>
# include <surface/crust/MetaAuthority.hh>

# include <common/common.hh>

# include <elle/serialize/Base64Archive.hh>
# include <elle/serialize/insert.hh>
# include <elle/serialize/extract.hh>
# include <elle/attribute.hh>

# include <boost/filesystem/path.hpp>

# include <memory>

class Network
{
  ELLE_ATTRIBUTE(std::unique_ptr<infinit::Descriptor>, descriptor);

public:
  static
  bool
  validate(std::string const& path,
           infinit::Authority const& authority = infinit::MetaAuthority{})
  {
    // XXX: Authority are not the same (elle:: and infinit::)
    return true;
  }

  static
  bool
  validate(infinit::Descriptor const& dsc,
           infinit::Authority const& authority = infinit::MetaAuthority{})
  {
    // XXX: Authority are not the same (elle:: and infinit::)
    return true;
  }

public:
  /*-------------.
  | Construction |
  `-------------*/
  /// Main constructor.
  explicit
  Network(cryptography::KeyPair const& keypair,
          const hole::Model& model,
          hole::Openness const& openness,
          horizon::Policy const& policy,
          std::string const& description = "No network descriptior",
          infinit::Authority const& authority = infinit::MetaAuthority{});

  /// Constructor with identity_path and passphrase.
  explicit
  Network(boost::filesystem::path const& identity_path,
          std::string const& identity_passphrase,
          const hole::Model& model,
          hole::Openness const& openness,
          horizon::Policy const& policy,
          std::string const& description = "No network descriptior",
          infinit::Authority const& authority = infinit::MetaAuthority{});

  /// Load constructor, using local descriptor.
  Network(boost::filesystem::path const& descriptor_path);

  /// Load constructor from network_name and home.
  explicit
  Network(std::string const& administrator_name,
          std::string const& network_name,
          boost::filesystem::path const& home_path = common::infinit::home());

  /// Load constructor, using descriptor from remote meta.
  // XXX: Should I put a dependencie to common by setting default value for meta
  // host and port?
  explicit
  Network(std::string const& owner_handle,
          std::string const& network_name,
          std::string const& host = common::meta::host(),
          uint16_t port = common::meta::port(),
          std::string const& token_path = common::meta::token_path());

  /*------.
  | Local |
  `------*/
  /// Store descriptor localy.
  void
  store(boost::filesystem::path const& descriptor_path = common::infinit::home(),
        bool force = false) const;

  /// Delete local descritor.
  static
  void
  erase(boost::filesystem::path const& descriptor_path);

  // Install.
  void
  install(std::string const& administrator_name,
          std::string const& network_name,
          boost::filesystem::path const& home_path = common::infinit::home())
    const;

  /// Uninstall.
  static
  void
  uninstall(std::string const& administrator_name,
            std::string const& network_name,
            boost::filesystem::path const& home_path = common::infinit::home());

  /// Mount.
  uint16_t
  mount(boost::filesystem::path const& path,
        bool run = true) const;

  /// Unmount.
  void
  unmount() const;

  /*------------.
  | Publication |
  `------------*/
   /// Publish descriptor to the remote meta.
  void
  publish(std::string const& network_name,
          std::string const& host = common::meta::host(),
          uint16_t port = common::meta::port(),
          std::string const& token_path = common::meta::token_path()) const;

  /// Remove descriptor from remote meta.
  static
  void
  unpublish(std::string const& name,
            std::string const& host = common::meta::host(),
            uint16_t port = common::meta::port(),
            std::string const& token_path = common::meta::token_path());

  ///
  // void
  // update();

  /*-----.
  | List |
  `-----*/
  /// Explore the given path and return all the (verified) .dsc.
  static
  std::vector<std::string>
  list(std::string const& user_name,
       boost::filesystem::path const& home_path = common::infinit::home());

  /// Get the list of descriptor id stored on the network.
  static
  std::vector<std::string>
  fetch(std::string const& host = common::meta::host(),
        uint16_t port = common::meta::port(),
        boost::filesystem::path const& token_path =
          common::meta::token_path(),
        plasma::meta::Client::DescriptorList const& list =
          plasma::meta::Client::DescriptorList::all);

  /*--------.
  | Look up |
  `--------*/
  static
  std::string
  lookup(std::string const& owner_handle,
         std::string const& network_name,
         std::string const& host = common::meta::host(),
         uint16_t port = common::meta::port(),
         boost::filesystem::path const& token_path =
           common::meta::token_path());

  /*-------------------.
  | Descriptor Getters |
  `-------------------*/
#define WRAP_DESCRIPTOR(_section_, _type_, _name_)                             \
  ELLE_ATTRIBUTE_r_ACCESSOR(_type_, _name_)                                    \
  {                                                                            \
    ELLE_ASSERT_NEQ(this->_descriptor, nullptr);                               \
    return this->_descriptor->_section_()._name_();                            \
  }

#define WRAP_META_DESCRIPTOR(_type_, _name_)                                   \
  WRAP_DESCRIPTOR(meta, _type_,  _name_)

#define WRAP_DATA_DESCRIPTOR(_type_, _name_)                                   \
  WRAP_DESCRIPTOR(data, _type_,  _name_)

  WRAP_META_DESCRIPTOR(infinit::Identifier, identifier);
  WRAP_META_DESCRIPTOR(cryptography::PublicKey, administrator_K);
  WRAP_META_DESCRIPTOR(hole::Model, model);
  WRAP_META_DESCRIPTOR(nucleus::neutron::Group::Identity, everybody_identity);
  WRAP_META_DESCRIPTOR(elle::Boolean, history);
  WRAP_META_DESCRIPTOR(elle::Natural32, extent);

  WRAP_DATA_DESCRIPTOR(elle::String, description);
  WRAP_DATA_DESCRIPTOR(hole::Openness, openness);
  WRAP_DATA_DESCRIPTOR(horizon::Policy, policy);
  WRAP_DATA_DESCRIPTOR(elle::Version, version);

#undef WRAP_DATA_DESCRIPTOR
#undef WRAP_META_DESCRIPTOR
#undef WRAP_DESCRIPTOR

};

#endif
