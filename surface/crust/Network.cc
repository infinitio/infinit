#include "Network.hh"

#include <lune/Identity.hh>

#include <etoile/automaton/Access.hh>
#include <etoile/nest/Nest.hh>

#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Genre.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Subject.hh>
#include <nucleus/neutron/Trait.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Network.hh>
#include <nucleus/neutron/Permissions.hh>
#include <nucleus/proton/Porcupine.hh>

#include <plasma/meta/Client.hh>

#include <protocol/Serializer.hh>
#include <protocol/ChanneledStream.hh>

#include <version.hh>

#include <cryptography/KeyPair.hh>
#include <cryptography/Cryptosystem.hh>
#include <cryptography/random.hh>

#include <elle/format/base64.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/io/Piece.hh>

#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.crust.Network");

class BlockAddresses
{
public:
  BlockAddresses(nucleus::proton::Address&& group,
                 nucleus::proton::Address&& directory):
    _group(std::move(group)),
    _directory(std::move(directory))
  {}

  ELLE_ATTRIBUTE_R(nucleus::proton::Address, group);
  ELLE_ATTRIBUTE_R(nucleus::proton::Address, directory);
};

static
nucleus::proton::Address
create_group(nucleus::proton::Network const& network,
             cryptography::KeyPair const& key_pair)
{
  ELLE_TRACE_FUNCTION(network, key_pair);

  ELLE_DEBUG("create group from key '%s' and network '%s'", key_pair, network);
  nucleus::neutron::Group group(network,
                                key_pair.K(),
                                "everybody");

  ELLE_DEBUG("seal group '%s'", group);
  group.seal(key_pair.k());

  return nucleus::proton::Address(group.bind());
}

static
nucleus::proton::Address
create_group(std::string const& uid,
             cryptography::KeyPair const& key_pair)
{
  return create_group(nucleus::proton::Network(uid),
                      key_pair);
}

static
nucleus::proton::Address
create_root(std::string const& uid,
            horizon::Policy policy,
            cryptography::KeyPair const& key_pair,
            nucleus::proton::Address const& group_address)
{
  ELLE_TRACE_FUNCTION(uid, policy, key_pair, group_address);

  ELLE_DEBUG("network with uid '%s'", uid);
  nucleus::proton::Network network(uid);

  ELLE_DEBUG("nest with network '%s' and key '%s'", network, key_pair);
  // XXX[we should use a null nest in this case because no block should be loaded/unloded]
  etoile::nest::Nest nest(
    ACCESS_SECRET_KEY_LENGTH,
    nucleus::proton::Limits(nucleus::proton::limits::Porcupine{},
                            nucleus::proton::limits::Node{1024, 0.5, 0.2},
                            nucleus::proton::limits::Node{1024, 0.5, 0.2}),
    network,
    key_pair.K());

  ELLE_DEBUG("procupine with nest '%s'", nest);
  nucleus::proton::Porcupine<nucleus::neutron::Access> access(nest);
  std::unique_ptr<nucleus::proton::Radix> access_radix;


  ELLE_DEBUG("subject with group address '%s'", group_address);
  nucleus::neutron::Subject subject;
  if (subject.Create(group_address) == elle::Status::Error)
    throw elle::Exception("unable to create the group subject");

  nucleus::neutron::Permissions permissions;

  // XXX: not implemented yet.
  ELLE_ASSERT_NEQ(policy, horizon::Policy::editable);

  ELLE_DEBUG("policy: '%s'", policy);
  switch (policy)
  {
    case horizon::Policy::accessible:
    case horizon::Policy::editable:
    {
      permissions = (policy == horizon::Policy::accessible) ?
        nucleus::neutron::permissions::read :
        nucleus::neutron::permissions::write;

      ELLE_DEBUG("door from subject '%s'", subject);
      nucleus::proton::Door<nucleus::neutron::Access> door =
        access.lookup(subject);

      door.open();
      // Note that a null token is provided because the root directory
      // contains no data.
      door().insert(new nucleus::neutron::Record{subject, permissions});
      door.close();
      access.update(subject);

      // XXX
      ELLE_DEBUG("generate secret key");
      static cryptography::SecretKey secret_key(
        cryptography::cipher::Algorithm::aes256,
        ACCESS_SECRET_KEY);

      ELLE_DEBUG("radix from access '%s'", access);
      access_radix.reset(new nucleus::proton::Radix{access.seal(secret_key)});
      break;
    }
    case horizon::Policy::confidential:
    {
      access_radix.reset(new nucleus::proton::Radix{});

      break;
    }
    default:
    {
      throw elle::Exception("invalid policy");
    }
  }

  ELLE_DEBUG("digest from access '%s'", access);
  cryptography::Digest fingerprint =
    nucleus::neutron::access::fingerprint(access);

  ELLE_ASSERT_NEQ(access_radix,  nullptr);
  // XXX: if policy == confidential, this assert will fail.
  ELLE_ASSERT_EQ(access_radix->strategy(), nucleus::proton::Strategy::value);

  ELLE_DEBUG("directory from network '%s' and key '%s'", network, key_pair);
  nucleus::neutron::Object directory(network,
                                     key_pair.K(),
                                     nucleus::neutron::Genre::directory);

  ELLE_DEBUG("update directory");
  if (directory.Update(directory.author(),
                       directory.contents(),
                       directory.size(),
                       *access_radix,
                       directory.owner_token()) == elle::Status::Error)
    throw elle::Exception("unable to update the object");
  if (directory.Seal(key_pair.k(), fingerprint) == elle::Status::Error)
    throw elle::Exception("unable to seal the object");

  ELLE_DEBUG("return blocks");
  return nucleus::proton::Address(directory.bind());
}

static
nucleus::proton::Address
create_root(std::string const& uid,
            cryptography::KeyPair const& key_pair,
            horizon::Policy policy)
{
  return create_root(uid,
                     policy,
                     key_pair,
                     create_group(uid, key_pair));
}

static
BlockAddresses
_block_addresses(std::string const& uid,
                 horizon::Policy policy,
                 cryptography::KeyPair const& key_pair)
{
  auto group_address = create_group(uid, key_pair);
  return BlockAddresses(std::move(group_address),
                        create_root(uid,
                                    key_pair,
                                    policy));
}

static
lune::Identity
identity(boost::filesystem::path const& path,
         std::string const& passphrase)
{
  ELLE_TRACE_FUNCTION(path);
  lune::Identity identity;
  {
    // Load the identity.
    identity.load(path);

    // decrypt the authority.
    if (identity.Decrypt(passphrase) == elle::Status::Error)
      throw elle::Exception("unable to decrypt the identity");
  }
  return identity;
}

Network::Network(std::string const& name,
                 lune::Identity const& identity,
                 hole::Model const& model,
                 hole::Openness const& openness,
                 horizon::Policy const& policy,
                 Authority const& authority)
{
  // Generate a 64 character long base 64 string.
  std::string uid = elle::format::base64::encode(
    cryptography::random::generate<elle::String>(48));

  // Create both root and group address.
  auto addrs = _block_addresses(uid, policy, identity.pair());

  cryptography::Signature meta_signature =
    authority.sign(
      descriptor::meta::hash(name,
                             identity.pair().K(),
                             model,
                             addrs.directory(),
                             addrs.group(),
                             false,
                             1048576));
  descriptor::Meta meta_section(uid,
                                identity.pair().K(),
                                model,
                                addrs.directory(),
                                addrs.group(),
                                false,
                                1048576,
                                meta_signature);

  elle::Version version(INFINIT_VERSION_MAJOR, INFINIT_VERSION_MINOR);

  cryptography::Signature data_signature =
    identity.pair().k().sign(
      descriptor::data::hash(name,
                             openness,
                             policy,
                             version,
                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 0, 0));
  descriptor::Data data_section(name,
                                openness,
                                policy,
                                version,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0,
                                data_signature);

  // Create the descriptor from both sections and store it.
  this->_descriptor.reset(new infinit::Descriptor(std::move(meta_section),
                                                  std::move(data_section)));
}

Network::Network(std::string const& name,
                 std::string const& identity_path,
                 std::string const& passphrase,
                 const hole::Model& model,
                 hole::Openness const& openness,
                 horizon::Policy const& policy,
                 Authority const& authority):
  Network(name,
          identity(identity_path, passphrase),
          model,
          openness,
          policy,
          authority)
{}

Network::Network(std::string const& name,
                 std::string const& identity_path,
                 std::string const& passphrase,
                 std::string const& model,
                 std::string const& openness,
                 std::string const& policy,
                 Authority const& authority):
  Network(name,
          identity(identity_path, passphrase),
          hole::Model(model),
          hole::openness_from_name(openness),
          horizon::policy_from_name(policy),
          authority)
{}

Network::Network(std::string const& descriptor_path)
{
  this->_descriptor.reset(
    new infinit::Descriptor(elle::io::Path{descriptor_path}));
}

Network::Network(Network const& other)
{
  ELLE_ASSERT_EQ(this->_descriptor, nullptr);
  this->_descriptor.reset(new infinit::Descriptor(*other._descriptor));
}

Network::Network(std::string const& id,
                 std::string const& host,
                 int16_t port)
{
  plasma::meta::Client client(host, port);

  using namespace elle::serialize;

  // auto descriptor = client.retrieve_network(id).descriptor;
  std::string descriptor;
  this->_descriptor.reset(new infinit::Descriptor(from_string<InputBase64Archive>(descriptor)));
}


void
Network::store(std::string const& descriptor_path) const
{
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);
  this->_descriptor->store(elle::io::Path{descriptor_path + ".dsc"});
}

void
Network::publish(std::string const& host,
                 int16_t port) const
{
  plasma::meta::Client client(host, port);

  using namespace elle::serialize;

  std::string serialized;
  to_string<OutputBase64Archive>(serialized) << *this->_descriptor;

//  client.publish(serialized);
}

std::vector<std::string>
Network::list(std::string const& path, bool verify)
{
  std::vector<std::string> dsc;

  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(path); itr != end_itr; ++itr)
  {
    if (boost::filesystem::is_directory(itr->status()))
    {
      continue;
    }
    else
    {
      if (itr->path().extension() == "dsc" && validate(itr->path().string()))
        dsc.push_back(itr->path().string());
    }
  }

  return dsc;
}

// std::vector<std::string>
// Network::list(std::string const& host,
//               uint16_t port,
//               NetworkList list)
// {
//   plasma::meta::Client client(host, port);

//   //return client.descriptor_list().descriptors;
//   return std::vector<std::string>();
// }
