#ifndef NETWORK_HH
# define NETWORK_HH

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

// ID represents a richer class than a simple string and avoid conflicts in the
// API.
class ID
{
public:
  ID(size_t size);
  ID(ID const& id);
  ID(std::string const& id);

  bool
  operator ==(std::string const& rhs);

  bool
  operator <(std::string const& rhs);

  ELLE_ATTRIBUTE_R(std::string, value);
};

class Network
{
  ELLE_ATTRIBUTE(std::unique_ptr<infinit::Descriptor>, descriptor);
  ELLE_ATTRIBUTE_P(boost::filesystem::path, descriptor_path, mutable);
  ELLE_ATTRIBUTE_P(boost::filesystem::path, install_path, mutable);
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
  Network(std::string const& name,
          cryptography::KeyPair const& keypair,
          const hole::Model& model,
          hole::Openness const& openness,
          horizon::Policy const& policy,
          infinit::Authority const& authority);

  /// Constructor with identity_path and passphrase.
  explicit
  Network(std::string const& name,
          boost::filesystem::path const& identity_path,
          std::string const& identity_passphrase,
          const hole::Model& model,
          hole::Openness const& openness,
          horizon::Policy const& policy,
          infinit::Authority const& authority = infinit::MetaAuthority{});

  /// Load constructor, using local descriptor.
  Network(boost::filesystem::path const& descriptor_path);

  /// Load constructor, using descriptor from remote meta.
  // XXX: Should I put a dependencie to common by setting default value for meta
  // host and port?
  explicit
  Network(ID const& id,
          std::string const& host = common::meta::host(),
          uint16_t port = common::meta::port(),
          std::string const& token = common::meta::token());

  /*------.
  | Local |
  `------*/
  /// Store descriptor localy.
  void
  store(boost::filesystem::path const& descriptor_path = common::infinit::home()) const;

  /// Delete local descritor.
  void
  erase(boost::filesystem::path const& descriptor_path = "");

  /// Install.
  void
  install(boost::filesystem::path const& path = common::infinit::home()) const;

  /// Uninstall.
  void
  uninstall(boost::filesystem::path const& path = "") const;

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
  publish(std::string const& host = common::meta::host(),
          uint16_t port = common::meta::port(),
          std::string const& token = common::meta::token()) const;

  /// Remove descriptor from remote meta.
  void
  unpublish(std::string const& host = common::meta::host(),
            uint16_t port = common::meta::port(),
            std::string const& token = common::meta::token()) const;

  ///
  // void
  // update();

  /*-----.
  | List |
  `-----*/
  /// Explore the given path and return all the (verified) .dsc.
  static
  std::vector<std::string>
  list(boost::filesystem::path const& path,
       bool verify = true);

  /// Get the list of descriptor id stored on the network.
  static
  std::vector<std::string>
  list(std::string const& host = common::meta::host(),
       uint16_t port = common::meta::port(),
       boost::filesystem::path const& token_path = common::meta::token_path(),
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
         boost::filesystem::path const& token_path = common::meta::token_path());


  /// Get the list of descriptor stored on the network.
  // XXX: This should return a list of Descriptor data, and the wrapper would
  // be able to create py::Object* representing networks.
  // static
  // std::vector<plasma::meta::Descriptor>
  // descriptors(plasma::meta::Client::DescriptorList const& list,
  //             std::string const& host = common::meta::host(),
  //             uint16_t port = common::meta::port(),
  //             std::string const& token = common::meta::token());

  /*--------.
  | Members |
  `--------*/

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

  WRAP_META_DESCRIPTOR(elle::String, identifier);
  WRAP_META_DESCRIPTOR(cryptography::PublicKey, administrator_K);
  WRAP_META_DESCRIPTOR(hole::Model, model);
  WRAP_META_DESCRIPTOR(nucleus::neutron::Group::Identity, everybody_identity);
  WRAP_META_DESCRIPTOR(elle::Boolean, history);
  WRAP_META_DESCRIPTOR(elle::Natural32, extent);

  WRAP_DATA_DESCRIPTOR(elle::String, name);
  WRAP_DATA_DESCRIPTOR(hole::Openness, openness);
  WRAP_DATA_DESCRIPTOR(horizon::Policy, policy);
  WRAP_DATA_DESCRIPTOR(elle::Version, version);

#undef WRAP_DESCRIPTOR

};

#endif
