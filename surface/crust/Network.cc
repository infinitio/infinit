#include "Network.hh"

#include <hole/storage/Memory.hh>
#include <hole/storage/Directory.hh>

#include <infinit/Identity.hh>

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

static
plasma::meta::Client
meta(std::string const& host,
     uint16_t port,
     std::string const& token)
{
  return plasma::meta::Client(host, port, true, token);
}

class Blocks
{
public:
  Blocks(nucleus::proton::Address&& group_address,
         std::vector<std::unique_ptr<nucleus::proton::Block>>&& blocks,
         std::unique_ptr<nucleus::neutron::Object>&& directory,
         nucleus::proton::Address&& directory_address):
    _group_address(std::move(group_address)),
    blocks(std::move(blocks)),
    directory(std::move(directory)),
    _directory_address(std::move(directory_address))
  {}

  ELLE_ATTRIBUTE_R(nucleus::proton::Address, group_address);
  std::vector<std::unique_ptr<nucleus::proton::Block>> blocks;
  std::unique_ptr<nucleus::neutron::Object> directory;
  ELLE_ATTRIBUTE_R(nucleus::proton::Address, directory_address);
};

static
std::pair<std::vector<std::unique_ptr<nucleus::proton::Block>>,
          nucleus::proton::Address>
create_groups(nucleus::proton::Network const& network,
              cryptography::KeyPair const& key_pair)
{
  ELLE_TRACE_FUNCTION(network, key_pair);

  ELLE_DEBUG("create group from key '%s' and network '%s'", key_pair, network);
  nucleus::neutron::Group group(network, key_pair.K(), "everybody");

  ELLE_DEBUG("seal group '%s'", group);
  group.seal(key_pair.k());

  nucleus::proton::Address address(group.bind());
  // Store the content blocks in a main memory storage.
  //
  // Ok for now, there is a single one but this is just to illustrate the logic
  // behind the process.
  hole::storage::Memory storage(network);
  storage.store(address, group);

  // Iterate over the storage and convert the string-based representations into
  // a vector of blocks.
  std::vector<std::unique_ptr<nucleus::proton::Block>> blocks;

  for (auto const& pair: storage.container())
  {
    nucleus::Derivable derivable;

    elle::serialize::from_string(pair.second) >> derivable;

    blocks.push_back(std::move(derivable.release()));
  }

  return std::make_pair(std::move(blocks), address);
}

static
std::pair<std::vector<std::unique_ptr<nucleus::proton::Block>>,
          nucleus::proton::Address>
create_groups(std::string const& uid,
              cryptography::KeyPair const& key_pair)
{
  return create_groups(nucleus::proton::Network(uid),
                       key_pair);
}

static
std::pair<std::unique_ptr<nucleus::neutron::Object>, nucleus::proton::Address>
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

      ELLE_ASSERT_EQ(access_radix->strategy(),
                     nucleus::proton::Strategy::value);

      break;
    }
    case horizon::Policy::confidential:
    {
      access_radix.reset(new nucleus::proton::Radix{});

      ELLE_ASSERT_EQ(access_radix->strategy(),
                     nucleus::proton::Strategy::none);

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
  return std::make_pair(std::unique_ptr<nucleus::neutron::Object>(new nucleus::neutron::Object(directory)), // XXX: move?
                        nucleus::proton::Address(directory.bind()));
}

static
std::pair<std::unique_ptr<nucleus::neutron::Object>, nucleus::proton::Address>
create_root(std::string const& uid,
            cryptography::KeyPair const& key_pair,
            horizon::Policy policy)
{
  return create_root(uid,
                     policy,
                     key_pair,
                     create_groups(uid, key_pair).second);
}

static
Blocks
_blocks(std::string const& uid,
        horizon::Policy policy,
        cryptography::KeyPair const& key_pair)
{
  auto group = create_groups(uid, key_pair);
  auto root = create_root(uid, key_pair, policy);

  return Blocks(std::move(group.second),
                std::move(group.first),
                std::move(root.first),
                std::move(root.second));
}

Network::Network(std::string const& name,
                 cryptography::KeyPair const& keypair,
                 hole::Model const& model,
                 hole::Openness const& openness,
                 horizon::Policy const& policy,
                 Authority const& authority)
{
  // Generate a 64 character long base 64 string.
  std::string uid = elle::format::base64::encode(
    cryptography::random::generate<elle::Buffer>(48));

  // Create both root and group address.
  auto blocks = _blocks(uid, policy, keypair);

  descriptor::Meta meta(uid,
                        keypair.K(),
                        model,
                        std::move(blocks.directory_address()),
                        std::move(blocks.directory),
                        std::move(blocks.group_address()),
                        false,
                        1048576,
                        keypair.k());

  elle::Version version(INFINIT_VERSION_MAJOR, INFINIT_VERSION_MINOR);

  descriptor::Data data(name,
                        openness,
                        policy,
                        std::move(blocks.blocks),
                        version,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0,
                        keypair.k());

  // Create the descriptor from both sections and store it.
  this->_descriptor.reset(new infinit::Descriptor(std::move(meta),
                                                  std::move(data)));
}

Network::Network(std::string const& name,
                 std::string const& identity_path,
                 std::string const& passphrase,
                 const hole::Model& model,
                 hole::Openness const& openness,
                 horizon::Policy const& policy,
                 Authority const& authority):
  Network(name,
          infinit::Identity(
            elle::serialize::from_file(identity_path)).decrypt(passphrase),
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
          infinit::Identity(
            elle::serialize::from_file(identity_path)).decrypt(passphrase),
          hole::Model(model),
          hole::openness_from_name(openness),
          horizon::policy_from_name(policy),
          authority)
{}

Network::Network(boost::filesystem::path const& descriptor_path)
{
  this->_descriptor.reset(
    new infinit::Descriptor(
      elle::serialize::from_file(descriptor_path.string())));
}

Network::Network(std::string const& id,
                 std::string const& host,
                 uint16_t port,
                 std::string const& token)
{
  using namespace elle::serialize;

  auto descriptor = meta(host, port, token).descriptor(id).descriptor;
  this->_descriptor.reset(new infinit::Descriptor(from_string<InputBase64Archive>(descriptor)));
}


void
Network::store(std::string const& descriptor_path) const
{
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);

  elle::serialize::to_file(descriptor_path);
}

void
Network::delete_(std::string const& descriptor_path)
{
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);
  // rm.
}

void
Network::mount(std::string const& mount_point,
               bool run) const
{
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);

  hole::storage::Directory storage(
    nucleus::proton::Network(this->_descriptor->meta().identifier()),
    mount_point);

  storage.store(this->_descriptor->meta().root_address(),
                this->_descriptor->meta().root_object());

  for (auto const& block: this->_descriptor->data().blocks())
  {
    nucleus::proton::Address block_address = block->bind();

    storage.store(block_address, *block);
  }

  if (run)
  {
    // XXX: Run 8infinit

  }
}

void
Network::unmount() const
{
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);

  // XXX:
}

void
Network::publish(std::string const& host,
                 uint16_t port,
                 std::string const& token) const
{
  using namespace elle::serialize;

  std::string serialized;
  to_string<OutputBase64Archive>(serialized) << *this->_descriptor;

  auto id = meta(host, port, token).descriptor_publish(serialized).id;
}

void
Network::unpublish(std::string const& id,
                   std::string const& host,
                   uint16_t port,
                   std::string const& token) const
{
  using namespace elle::serialize;

  meta(host, port, token).descriptor_unpublish(id);
}

std::vector<std::string>
Network::list(std::string const& path_str,
              bool verify)
{
  std::vector<std::string> dsc;

  boost::filesystem::path path(path_str);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(path); itr != end_itr; ++itr)
  {
    if (boost::filesystem::is_directory(itr->status()))
    {
      continue;
    }
    else
    {
      std::cerr << itr->path() << std::endl;
      if (itr->path().extension() == ".dsc" && Network::validate(itr->path().string()))
        dsc.push_back(itr->path().string());
    }
  }

  return dsc;
}

std::vector<std::string>
Network::list(plasma::meta::Client::DescriptorList const& list,
              std::string const& host,
              uint16_t port,
              std::string const& token)
{
  return meta(host, port, token).descriptor_list(list).descriptors;
}
