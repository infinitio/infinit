#include "Network.hh"
#include <cassert>

#include <hole/Passport.hh>
#include <hole/implementations/slug/Manifest.hh>
#include <hole/storage/Directory.hh>

#include <lune/Identity.hh>
#include <lune/Lune.hh>
#include <lune/Phrase.hh>
#include <lune/Set.hh>

#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Genre.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Subject.hh>
#include <nucleus/neutron/Trait.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Porcupine.hh>

#include <protocol/Serializer.hh>
#include <protocol/ChanneledStream.hh>

#include <elle/format/json.hh>
#include <elle/io/Piece.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/system/Process.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

ELLE_LOG_COMPONENT("infinit.crust.Network");

static
std::string
generate_id(std::string const& name,
            cryptography::PublicKey const& administrator_K)
{
  return "42";
}

Network::Network(std::string const& name,
                 cryptography::PublicKey const& administrator_K,
                 hole::Model const& model = hole::Model(hole::Type::TypeSlug),
                 hole::Openness const& openness = hole::Openness::open,
                 horizon::Policy const& policy = horizon::Policy::editable,
                 bool history = false,
                 int32_t extent = 1024,
                 elle::Version const& version = elle::Version(0, 0))
{
  this->_id = generate_id(name, administrator_K);

  auto root_address = this->create_rootblock(XXX);
  auto group_address = this->create_rootblock(XXX);

  auto signature = this->seal(auth);
}

cryptorgraphy::Signature
Network::seal(Authority const& auth)
{
  return auth.seal();
}

void
Network::validate(Authority const& auth) const
{
  auth.validate(*this);
}

nucleus::proton::Address
Network::create_rootblock(lune::Identity const& owner_identity)
{
  ELLE_TRACE_METHOD(id, owner_identity);
  nucleus::proton::Network network{this->_id};

  //- subject ----------------------------------------------------------------
  nucleus::neutron::Subject subject;
  if (subject.Create(group_address) == elle::Status::Error)
    throw elle::Exception("unable to create the group subject");

  //- access-------------------------------------------------------------------
  nucleus::proton::Porcupine<nucleus::neutron::Access> access_porcupine{
    nucleus::proton::nest::none()};

  nucleus::proton::Door<nucleus::neutron::Access> access_door =
    access_porcupine.lookup(subject);

  access_door.open();
  access_door().insert(new nucleus::neutron::Record{subject, permissions});
  access_door.close();
  access_porcupine.update(subject);

// Turbo XXX:
// XXX[cf: etoile/automaton/Access.hh>, until no longer encrypted]
#define ACCESS_SECRET_KEY_LENGTH 256
#define ACCESS_SECRET_KEY "no-secret-key"

  // XXX
  ELLE_ASSERT(access_porcupine.strategy() == nucleus::proton::Strategy::value);

  cryptography::Digest access_fingerprint =
    nucleus::neutron::access::fingerprint(access_porcupine);

  nucleus::proton::Radix access_radix = access_porcupine.seal(this->_secret_key);

  //- directory ---------------------------------------------------------------
  nucleus::neutron::Object directory(network,
                                     identity.pair().K(),
                                     nucleus::neutron::Genre::directory);

  if (directory.Update(directory.author(),
                       directory.contents(),
                       directory.size(),
                       access_radix,
                       directory.owner_token()) == e)
    throw elle::Exception("unable to update the directory");

  if (directory.Seal(identity.pair().k(), access_fingerprint) == e)
    throw elle::Exception("Cannot seal the access");

  //- directory address --------------------------------------------------------
  return nucleus::proton::Address{directory.bind()};
}

nucleus::proton::Address
Network::create_group_address(lune::Identity const& owner_identity)
{
  ELLE_TRACE_METHOD(this->_id, owner_identity);
  nucleus::proton::Network network{this->_id};
  nucleus::neutron::Group group{network,
                                owner_identity.pair().K(),
                                "everybody"};
  group.seal(identity.pair().k());
  return nucleus::proton::Address{group.bind()};
}

void
Network::prepare()
{

}

void
Network::store(std::string const& shelter_path)
{
  // ELLE_DEBUG("Prepare network %s directory.", this->_id);
  // using elle::serialize::from_string;
  // using elle::serialize::InputBase64Archive;

  // ELLE_DEBUG("Shelter path == %s", shelter_path);
  // nucleus::proton::Network proton_network(this->_id);
  // hole::storage::Directory storage(proton_network, shelter_path);

  // {
  //   ELLE_DEBUG("Built storage of %s", this->_descriptor.meta().id());

  //   ELLE_DEBUG("root block %s", network.root_block);
  //   ELLE_DEBUG("descriptor %s", network.descriptor);

  //   ELLE_ASSERT(network.root_block.size());
  //   ELLE_ASSERT(network.descriptor.size());

  //   ELLE_DEBUG("Create lune descriptor of %s", network_id);

  //   lune::Descriptor descriptor{
  //     from_string<InputBase64Archive>(network.descriptor)
  //   };
  //   ELLE_DEBUG("Lune descriptor created");

  // // XXX[pas forcement necessaire si le format n'a pas change entre
  // //     la version du descriptor et celle d'Infinit. il faudrait
  // //     comparer static format avec ceux de reference dans le descriptor]
  // //if (descriptor.version() > Infinit::version)
  // //  {
  // //    throw std::runtime_error("you have to update Infinit");
  // //  }

  // // XXX[ici plutot compare static/dynamic format car on peut passer
  // //     a une nouvelle version sans que le descriptor n'ait change
  // //     de format]
  // //if (description.version() < Infinit::version && je suis owner)
  // //  {
  // //     static_assert(false, "migrate the descriptor here and send to meta");
  // //  }

  //   lune::Identity identity;
  //   identity.Restore(this->_meta.identity());

  //   ELLE_DEBUG("Storing the descriptor of %s for user %s",
  //       network_id, this->_self.id);
  //   descriptor.store(identity);

  //   nucleus::neutron::Object directory{
  //     from_string<InputBase64Archive>(network.root_block)
  //   };

  //   storage.store(descriptor.meta().root(), directory);
  //   ELLE_DEBUG("Root block stored.");
  // }

  // {
  //   ELLE_DEBUG("Storing group block.");
  //   ELLE_DEBUG("block: '%s'.", network.group_block);
  //   nucleus::neutron::Group group{
  //     from_string<InputBase64Archive>(network.group_block)
  //   };
  //   ELLE_DEBUG("address: '%s'.", network.group_address);
  //   nucleus::proton::Address group_address{
  //     from_string<InputBase64Archive>(network.group_address)
  //   };
  //   ELLE_DEBUG("Deserialization complete.");
  //   storage.store(group_address, group);
  //   ELLE_DEBUG("Group block stored.");
  // }

  // ELLE_DEBUG("End of prepare directory");
}
