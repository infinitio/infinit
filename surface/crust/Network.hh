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
  Network(std::string const& name,
          cryptography::KeyPair const& keypair,
          const hole::Model& model,
          hole::Openness const& openness,
          horizon::Policy const& policy,
          infinit::Authority const& authority);

  /// Constructor with identity_path and passphrase.
  Network(std::string const& name,
          std::string const& identity_path,
          std::string const& identity_passphrase,
          const hole::Model& model = hole::Model(hole::Model::Type::TypeSlug),
          hole::Openness const& openness = hole::Openness::open,
          horizon::Policy const& policy = horizon::Policy::editable,
          infinit::Authority const& authority = infinit::MetaAuthority{});

  /// String only constructor to make python binding easier.
  Network(std::string const& name,
          std::string const& identity_path,
          std::string const& passphrase,
          std::string const& model = "slug",
          std::string const& openness = "open",
          std::string const& policy = "editable",
          infinit::Authority const& authority = infinit::MetaAuthority{});

  /// Load constructor, using local descriptor.
  Network(boost::filesystem::path const& descriptor_path);

  /// Load constructor, using descriptor from remote meta.
  // XXX: Should I put a dependencie to common by setting default value for meta
  // host and port?
  Network(std::string const& id,
          std::string const& host = common::meta::host(),
          uint16_t port = common::meta::port(),
          std::string const& token = common::meta::token());

  /*------.
  | Local |
  `------*/
  /// Store descriptor localy.
  void
  store(std::string const& descriptor_path) const;

  /// Delete local descritor.
  void
  delete_(std::string const& descriptor_path);

  /// Mount.
  void
  mount(std::string const& network_path,
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
  unpublish(std::string const& id,
            std::string const& host = common::meta::host(),
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
  list(std::string const& path,
       bool verify = true);

  /// Get the list of descriptor stored on the network.
  static
  std::vector<std::string>
  list(plasma::meta::Client::DescriptorList const& list,
       std::string const& host = common::meta::host(),
       uint16_t port = common::meta::port(),
       std::string const& token = common::meta::token());

  /*--------.
  | Members |
  `--------*/

  /*-------------------.
  | Descriptor Getters |
  `-------------------*/
#define WRAP_DESCRIPTOR(_section_, _type_, _name_)                             \
  ELLE_ATTRIBUTE_r_ACCESSOR(_type_, _name_)                                    \
  {                                                                            \
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


#undef WRAP_DESCRIPTOR

};

#endif
