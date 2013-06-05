#ifndef NETWORK_HH
# define NETWORK_HH

# include <plasma/meta/Client.hh>

# include <version.hh>
# include <infinit/Descriptor.hh>
# include <surface/crust/Authority.hh>
# include <surface/crust/MetaAuthority.hh>

# include <elle/serialize/Base64Archive.hh>
# include <elle/serialize/insert.hh>
# include <elle/serialize/extract.hh>

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
  /// Main constructor.
  Network(std::string const& name,
          lune::Identity const& identity,
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
  Network(std::string const& descriptor_path);

  /// Load constructor, using descriptor from remote meta.
  // XXX: Should I put a dependencie to common by setting default value for meta
  // host and port?
  Network(std::string const& id,
          std::string const& host,
          int16_t port);

  /// Copy constructor, usefull for python binding. Should be remove.
  Network(Network const& other);

  /// Store descriptor localy.
  void
  store(std::string const& descriptor_path) const;

  /// Publish descriptor to the remote meta.
  void
  publish(std::string const& host,
          int16_t port) const;

  /// Delete local descritor.
  void
  delete_(std::string const& descriptor_path);

  /// Remove descriptor from remote meta.
  // XXX: Should I put a dependencie to common by setting default value for meta
  // host and port?
  void
  unpublish(std::string const& id,
            std::string const& host,
            int16_t port);

  ///
  // void
  // update();

  /// Explore the given path and return all the (verified) .dsc.
  std::vector<std::string>
  list(std::string const& path,
       bool verify = true);

  enum NetworkList
  {
    all,
    mine,
    other,
  };
  /// Get the list of descriptor stored on the network.
  // std::vector<std::string>
  // list(std::string const& host,
  //      uint16_t port,
  //      NetworkList list);
};

#endif
