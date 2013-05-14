#ifndef NETWORK_HH
# define NETWORK_HH

# include <plasma/meta/Client.hh>

# include <lune/Descriptor.hh>

# include <memory>

static int major_version = INFINIT_VERSION_MAJOR;
static int minor_version = INFINIT_VERSION_MINOR;

class Authority
{
private:
  template <typename T>
  void
  _seal(T const& t) const
  {
    // Do some stuff.
  }

  template <typename T>
  void
  _validate(T const& t) const
  {
    // Do some stuff.
  }

public:
  template <typename T>
  void
  seal(T const& t) const
  {
    this->_seal(t);
  }

  template <typename T>
  void
  validate(T const& t) const
  {
    this->_validate(t);
  }
};

class MetaAuthority: public Authority
{
  plasma::meta::Client _meta;

  MetaAuthority(std::string const& host,
                uint16_t port):
    _meta{host, port}
  {}

  //XXX
  std::string
  seal(std::string const& s) // const
  {
    return this->_meta.sign_hash(s).signature;
  }
};

class Network
{
  std::unique_ptr<lune::Descriptor> _desc;
  std::string _id;
  std::string _path;

public:
  Network(std::string const& name,
          cryptography::PublicKey const& administrator_K,
          hole::Model const& model = hole::Model(hole::Type::TypeSlug),
          hole::Openness const& openness = hole::Openness::open,
          horizon::Policy const& policy = horizon::Policy::editable,
          bool history = false,
          int32_t extent = 1024,
          elle::Version const& version = elle::Version(major_version,
                                                       minor_version));

  std::string const&
  create(std::string const& name);

  cryptography::Signature
  seal(Authority const& auth);

  void
  validate(Authority const& auth) const;

  void
  prepare();

  void
  store(std::string const& path);

  nucleus::proton::Address
  create_rootblock(lune::Identity const& owner_identity);

  nucleus::proton::Address
  create_group_address(lune::Identity const& owner_identity);
};

#endif
