#include "Network.hh"

#include <hole/storage/Memory.hh>
#include <hole/storage/Directory.hh>

#include <infinit/Certificate.hh>
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

#include <version.hh>

#include <cryptography/KeyPair.hh>
#include <cryptography/Cryptosystem.hh>
#include <cryptography/random.hh>

#include <elle/attribute.hh>
#include <elle/format/hexadecimal.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/io/Piece.hh>

#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.crust.Network");

/*-----.
| Meta |
`-----*/
static
plasma::meta::Client const&
meta(std::string const& host,
     uint16_t port,
     boost::filesystem::path const& tokenpath)
{
  static std::string last_host = host;
  static uint16_t last_port = port;
  static std::unique_ptr<plasma::meta::Client> client;

  if (!client || last_host != host || last_port != port)
  {
    ELLE_TRACE("creating a new meta: %s %s %s", host, port, tokenpath);
    client.reset(new plasma::meta::Client(host, port, true, tokenpath));
  }

  ELLE_ASSERT_NEQ(client, nullptr);
  return *client;
}

/*----------------.
| Block creations |
`----------------*/
static
std::unique_ptr<nucleus::neutron::Group>
create_group(nucleus::proton::Network const& network,
             infinit::cryptography::KeyPair const& key_pair)
{
  ELLE_TRACE_FUNCTION(network, key_pair);

  ELLE_DEBUG("create group from key '%s' and network '%s'", key_pair, network);
  std::unique_ptr<nucleus::neutron::Group> group(
    new nucleus::neutron::Group(network, key_pair.K(), "everybody"));

  ELLE_DEBUG("seal group '%s'", *group);
  group->seal(key_pair.k());

  return group;
}

static
std::unique_ptr<nucleus::neutron::Object>
create_root(nucleus::proton::Network const& network,
            horizon::Policy policy,
            infinit::cryptography::KeyPair const& key_pair,
            nucleus::proton::Address const& group_address)
{
  ELLE_TRACE_FUNCTION(network, policy, key_pair, group_address);

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
      static infinit::cryptography::SecretKey secret_key(
        infinit::cryptography::cipher::Algorithm::aes256,
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
  infinit::cryptography::Digest fingerprint =
    nucleus::neutron::access::fingerprint(access);

  ELLE_DEBUG("directory from network '%s' and key '%s'", network, key_pair);
  std::unique_ptr<nucleus::neutron::Object> directory(
    new nucleus::neutron::Object(network,
                                 key_pair.K(),
                                 nucleus::neutron::Genre::directory));

  ELLE_DEBUG("update directory");
  if (directory->Update(directory->author(),
                        directory->contents(),
                        directory->size(),
                        *access_radix,
                        directory->owner_token()) == elle::Status::Error)
    throw elle::Exception("unable to update the object");
  if (directory->Seal(key_pair.k(), fingerprint) == elle::Status::Error)
    throw elle::Exception("unable to seal the object");
  return directory;
}

static
void
require_infinit_home(boost::filesystem::path const& home_path)
{
  if (!boost::filesystem::exists(home_path))
    throw elle::Exception(
      elle::sprintf("directory %s set as infinit_home doesn't exist",
                    home_path));
}

static
void
require_network_directory(std::string const& user_name,
                          boost::filesystem::path const& home_path)
{
  require_infinit_home(home_path);

  // To ensure the infinit home is good, we check if their is an identity in the
  // given infinit home architecture, deserialize it.
  {
    auto const& identity_path =
      common::infinit::identity_path(user_name,
                                     home_path.string());

    if (!boost::filesystem::exists(identity_path))
      throw elle::Exception(
        elle::sprintf("no identity file at %s", identity_path));

    try
    {

      infinit::Identity{elle::serialize::from_file(identity_path)};
    }
    catch (elle::Exception const&)
    {
      throw elle::Exception(
        elle::sprintf("identity at %s is not an identity", identity_path));
    }
  }

  auto const& networks_directory =
    common::infinit::networks_directory(user_name,
                                        home_path.string());

  boost::filesystem::create_directory(networks_directory);
}

/*--------.
| Network |
`--------*/
Network::Network(cryptography::KeyPair const& keypair,
                 hole::Model const& model,
                 hole::Openness const& openness,
                 horizon::Policy const& policy,
                 std::string const& description,
                 Authority const& authority)
{
  ELLE_TRACE_METHOD(description, model, openness, policy);

  infinit::Identifier uid(32);

  ELLE_DEBUG("network uid: '%s'", uid);
  nucleus::proton::Network network(uid.value());

  ELLE_DEBUG("group from '%s' and '%s'", network, keypair);
  auto group = create_group(network, keypair);
  ELLE_DEBUG("group address for '%s'", *group);
  nucleus::proton::Address group_address(group->bind());

  // Store the content blocks in a main memory storage.
  //
  // Ok for now, there is a single one but this is just to illustrate the logic
  // behind the process.
  ELLE_DEBUG("memory storage from '%s'", network);
  hole::storage::Memory storage(network);
  ELLE_DEBUG("store group '%s' and '%s'", group_address, *group);
  storage.store(group_address, *group);

  // Iterate over the storage and convert the string-based representations into
  // a vector of blocks.
  std::vector<std::unique_ptr<nucleus::proton::Block>> blocks;
  ELLE_DEBUG("storage: %s, container: %s", storage, storage.container());
  for (auto const& pair: storage.container())
  {
    nucleus::Derivable derivable;

    elle::serialize::from_string(pair.second) >> derivable;

    blocks.push_back(std::move(derivable.release()));
  }

  std::unique_ptr<nucleus::neutron::Object> directory(
    std::move(create_root(network, policy, keypair, group_address)));
  nucleus::proton::Address directory_address(directory->bind());

  elle::Version version(INFINIT_VERSION_MAJOR, INFINIT_VERSION_MINOR);
  {
    // Create the descriptor from both sections and store it.
    auto meta = descriptor::Meta(common::meta::repository_certificate().subject_K(),
                                 keypair.K(),
                                 model,
                                 std::move(directory_address),
                                 std::move(directory),
                                 std::move(group_address),
                                 false,
                                 1048576,
                                 authority,
                                 uid);

    auto data = descriptor::Data(description,
                                 openness,
                                 policy,
                                 std::move(blocks),
                                 {},
                                 version,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                 authority);
    this->_descriptor.reset(
      new infinit::Descriptor{std::move(meta), std::move(data)});
  }
}

Network::Network(boost::filesystem::path const& identity_path,
                 std::string const& passphrase,
                 const hole::Model& model,
                 hole::Openness const& openness,
                 horizon::Policy const& policy,
                 std::string const& description,
                 Authority const& authority):
  Network(infinit::Identity(
            elle::serialize::from_file(identity_path.string())).decrypt_0(passphrase),
          model,
          openness,
          policy,
          description,
          authority)
{}

Network::Network(boost::filesystem::path const& descriptor_path)
{
  if (!boost::filesystem::exists(descriptor_path))
    throw elle::Exception(
      elle::sprintf("file %s doesn't exist", descriptor_path));

  this->_descriptor.reset(
    new infinit::Descriptor(
      elle::serialize::from_file(descriptor_path.string())));
}

Network::Network(std::string const& administrator_name,
                 std::string const& network_name,
                 boost::filesystem::path const& home_path):
  Network(common::infinit::descriptor_path(administrator_name,
                                           network_name,
                                           home_path.string()))
{}

Network::Network(std::string const& owner_handle,
                 std::string const& network_name,
                 std::string const& host,
                 uint16_t port,
                 std::string const& token_path)
{
  using namespace elle::serialize;

  auto descriptor = meta(host, port, token_path).descriptor(owner_handle,
                                                            network_name).descriptor;
  this->_descriptor.reset(
    new infinit::Descriptor(from_string<InputBase64Archive>(descriptor)));
}

void
Network::store(boost::filesystem::path const& descriptor_path,
               bool force) const
{
  ELLE_TRACE_METHOD(descriptor_path);
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);

  if (descriptor_path.empty())
    throw elle::Exception("descriptor path is empty");

  if (!force && boost::filesystem::exists(descriptor_path))
    throw elle::Exception(
      elle::sprintf("file %s already exists", descriptor_path));

  elle::serialize::to_file(descriptor_path.string()) << *this->_descriptor;
}

void
Network::erase(boost::filesystem::path const& descriptor_path)
{
  ELLE_TRACE_FUNCTION(descriptor_path);

  if (!boost::filesystem::exists(descriptor_path))
    throw elle::Exception(
      elle::sprintf("descriptor path %s doesn't exists", descriptor_path));

  // Check if the given path represents an descriptor.
  // XXX: Does this code works (optimisation).
  Network{descriptor_path};

  boost::filesystem::remove(descriptor_path);
}

void
Network::install(std::string const& user_name,
                 std::string const& network_name,
                 boost::filesystem::path const& home_path) const
{
  ELLE_TRACE_METHOD(user_name, network_name, home_path);
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);
  require_network_directory(user_name, home_path);

  auto install_path =
    common::infinit::network_directory(user_name,
                                       network_name,
                                       home_path.string());

  if (boost::filesystem::exists(install_path))
    throw elle::Exception(
      elle::sprintf("couldn't create %s cause it already exists",
                    install_path));

  boost::filesystem::create_directory(install_path);

  this->store(common::infinit::descriptor_path(user_name,
                                               network_name,
                                               home_path.string()));

  hole::storage::Directory storage(
    nucleus::proton::Network(this->_descriptor->meta().identifier().value()),
    common::infinit::network_shelter(user_name,
                                     network_name,
                                     home_path.string()));

  storage.store(this->_descriptor->meta().root_address(),
                this->_descriptor->meta().root_object());

  for (auto const& block: this->_descriptor->data().blocks())
  {
    nucleus::proton::Address block_address = block->bind();

    storage.store(block_address, *block);
  }
}

void
Network::uninstall(std::string const& user_name,
                   std::string const& network_name,
                   boost::filesystem::path const& home_path)
{
  ELLE_TRACE_FUNCTION(user_name, network_name, home_path);
  require_infinit_home(home_path);

  auto const& network_directory =
    common::infinit::network_directory(user_name,
                                       network_name,
                                       home_path.string());

  if (!boost::filesystem::exists(network_directory))
    throw elle::Exception(
      elle::sprintf("network path %s is empty", network_directory));

  // Check if the given path represents an descriptor.
  // XXX: Does this code work (optimisation)?
  Network{
    boost::filesystem::path{
      common::infinit::descriptor_path(user_name,
                                       network_name,
                                       home_path.string())}};

  boost::filesystem::remove_all(network_directory);
}

uint16_t
Network::mount(boost::filesystem::path const& mount_point,
               bool run) const
{
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);

  hole::storage::Directory storage(
    nucleus::proton::Network(this->_descriptor->meta().identifier().value()),
    (mount_point / this->identifier().value()).string());

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

    uint16_t pid = 0;
    return pid;
  }

  return 0;
}

void
Network::unmount() const
{
  ELLE_ASSERT_NEQ(this->_descriptor, nullptr);

  // XXX:
}

void
Network::publish(std::string const& network_name,
                 std::string const& host,
                 uint16_t port,
                 std::string const& token_path) const
{
  using namespace elle::serialize;

  std::string serialized;
  to_string<OutputBase64Archive>(serialized) << *this->_descriptor;

  meta(host, port, token_path).descriptor_publish(serialized, network_name);
}

void
Network::unpublish(std::string const& network_name,
                   std::string const& host,
                   uint16_t port,
                   std::string const& token_path)
{
  meta(host, port, token_path).descriptor_unpublish(network_name);
}

std::vector<std::string>
Network::list(std::string const& user_name,
              boost::filesystem::path const& home_path)
{
  std::vector<std::string> dsc;

  auto const& networks_directory =
    common::infinit::networks_directory(user_name,
                                        home_path.string());

  using namespace boost::filesystem;
  directory_iterator networks_itr_end;
  // Iterate over networks dir, containing network folder.
  for (directory_iterator networks_itr(networks_directory);
       networks_itr != networks_itr_end;
       ++networks_itr)
  {
    if (!is_directory(networks_itr->path()))
    {
      continue;
    }
    else
    {
      // Iterate over file in the possible network directory.
      directory_iterator network_itr_end;
      for (directory_iterator network_itr(networks_itr->path());
           network_itr != network_itr_end;
           ++network_itr)

      if (network_itr->path().filename() == "descriptor")
      {
        try
        {
          Network net(network_itr->path());
          dsc.push_back(networks_itr->path().filename().string());
        }
        catch (elle::Exception const&)
        {
          ELLE_WARN("%s is not a descriptor", network_itr->path().string());
        }
      }
    }
  }

  return dsc;
}

std::vector<std::string>
Network::fetch(std::string const& host,
              uint16_t port,
              boost::filesystem::path const& token_path_path,
              plasma::meta::Client::DescriptorList const& list)
{
  return meta(host, port, token_path_path).descriptor_list(list).descriptors;
}
