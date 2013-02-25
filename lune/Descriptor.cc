#include <Infinit.hh>

#include <elle/io/File.hh>
#include <elle/io/Piece.hh>
#include <elle/concurrency/Scheduler.hh>
#include <elle/os/path.hh>
#include <elle/log.hh>

#include <common/common.hh>

#include <cryptography/PrivateKey.hh>

#include <hole/Authority.hh>
#include <hole/Openness.hh>

#include <lune/Descriptor.hh>
#include <lune/Lune.hh>
#include <lune/Identity.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/neutron/Subject.hh>

namespace lune
{

  ELLE_LOG_COMPONENT("infinit.lune.Descriptor");

  ///
  /// this constant defines whether or not the history functionality
  /// is activated for this network.
  ///
  const elle::Boolean           Descriptor::History = false;

  ///
  /// this constant defines the size of the blocks stored within this
  /// network.
  ///
  const elle::Natural32         Descriptor::Extent = 8192;

//
// ---------- descriptor ------------------------------------------------------
//

  Descriptor::Descriptor(elle::String const& user,
                         elle::String const& network)
  {
    ELLE_TRACE("Creating descriptor of network %s in %s",
               network, this->_path(user, network));
    if (Descriptor::exists(user, network) == false)
      throw elle::Exception("this network does not seem to exist");
    this->load(user, network);
    this->validate(Infinit::authority());
  }

  Descriptor::Descriptor(elle::String const& id,
                         cryptography::PublicKey const& administrator_K,
                         hole::Model const& model,
                         nucleus::proton::Address const& root,
                         nucleus::neutron::Group::Identity const& everybody,
                         elle::String const& name,
                         hole::Openness const& openness,
                         horizon::Policy const& policy,
                         elle::Boolean history,
                         elle::Natural32 extent,
                         elle::Version const& version,
                         elle::Authority const& authority):
    _meta(id, administrator_K, model, root, everybody, history, extent,
          authority),
    _data(name, openness, policy, version)
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Descriptor)
  {
  }

  /*--------.
  | Methods |
  `--------*/

  void
  Descriptor::seal(cryptography::PrivateKey const& administrator_k)
  {
    this->_data.seal(administrator_k);
  }

  void
  Descriptor::validate(elle::Authority const& authority) const
  {
    this->_meta.validate(authority);
    this->_data.validate(this->_meta.administrator_K());
  }

  Descriptor::Meta const&
  Descriptor::meta() const
  {
    return (this->_meta);
  }

  Descriptor::Data const&
  Descriptor::data() const
  {
    return (this->_data);
  }

  std::string
  Descriptor::_path(elle::String const& user_id,
                    elle::String const& network_id)
  {
    return elle::os::path::join(
      common::infinit::network_directory(user_id, network_id),
      network_id + ".dsc"
    );
  }

  elle::Status
  Descriptor::Dump(const elle::Natural32  margin) const
  {
    elle::String alignment(margin, ' ');

    std::cout << alignment << "[Descriptor]" << std::endl;

    if (this->_meta.Dump(margin + 2) == elle::Status::Error)
      throw elle::Exception("XXX");

    if (this->_data.Dump(margin + 2) == elle::Status::Error)
      throw elle::Exception("XXX");

    return elle::Status::Ok;
  }

  void
  Descriptor::load(elle::String const& user,
                   elle::String const& network)
  {
    this->load(elle::io::Path{Descriptor::_path(user, network)});
  }

  void
  Descriptor::store(Identity const& identity) const
  {
    ELLE_TRACE("store descriptor with %s", identity);
    this->store(elle::io::Path{Descriptor::_path(identity.id(),
                                                 this->meta().id())});
  }

  void
  Descriptor::erase(elle::String const& user,
                    elle::String const& network)
  {
    elle::concept::Fileable<>::erase(
      elle::io::Path{Descriptor::_path(user, network)});
  }

  elle::Boolean
  Descriptor::exists(elle::String const& user,
                     elle::String const& network)
  {
    return elle::os::path::exists(
      Descriptor::_path(user, network)
    );
  }

//
// ---------- meta ------------------------------------------------------------
//

  Descriptor::Meta::Meta():
    _signature(nullptr)
  {
    this->_everybody.subject = nullptr;
  }

  Descriptor::Meta::Meta(elle::String const& id,
                         cryptography::PublicKey const& administrator_K,
                         hole::Model const& model,
                         nucleus::proton::Address const& root,
                         nucleus::neutron::Group::Identity const& everybody,
                         elle::Boolean history,
                         elle::Natural32 extent,
                         elle::Authority const& authority):
    _id(id),
    _administrator_K(administrator_K),
    _model(model),
    _root(root),
    _everybody{everybody, nullptr},
    _history(history),
    _extent(extent),
    _signature(nullptr)
  {
    if (authority.type != elle::Authority::TypePair)
      throw std::runtime_error("unable to sign with a public authority");

    delete this->_signature;
    this->_signature = nullptr;
    this->_signature = new cryptography::Signature{
      authority.k->sign(
        elle::serialize::make_tuple(
          this->_id,
          this->_administrator_K,
          this->_model,
          this->_root,
          this->_everybody.identity,
          this->_history,
          this->_extent))};
  }

  Descriptor::Meta::~Meta()
  {
    delete this->_everybody.subject;
    delete this->_signature;
  }

  void
  Descriptor::Meta::validate(elle::Authority const& authority) const
  {
    ELLE_ASSERT(this->_signature != nullptr);

    if (authority.K().verify(
          *this->_signature,
          elle::serialize::make_tuple(
            this->_id,
            this->_administrator_K,
            this->_model,
            this->_root,
            this->_everybody.identity,
            this->_history,
            this->_extent)) == false)
      throw std::runtime_error("unable to validate the meta section "
                               "with the authority's key");
  }

  elle::String const&
  Descriptor::Meta::id() const
  {
    return (this->_id);
  }

  void
  Descriptor::Meta::id(elle::String const& id)
  {
    this->_id = id;
  }

  cryptography::PublicKey const&
  Descriptor::Meta::administrator_K() const
  {
    return (this->_administrator_K);
  }

  hole::Model const&
  Descriptor::Meta::model() const
  {
    return (this->_model);
  }

  nucleus::proton::Address const&
  Descriptor::Meta::root() const
  {
    return (this->_root);
  }

  nucleus::neutron::Group::Identity const&
  Descriptor::Meta::everybody_identity() const
  {
    return (this->_everybody.identity);
  }

  nucleus::neutron::Subject const&
  Descriptor::Meta::everybody_subject() const
  {
    const_cast<Descriptor::Meta*>(this)->_everybody_subject();
    return (*this->_everybody.subject);
  }

  elle::Boolean
  Descriptor::Meta::history() const
  {
    return (this->_history);
  }

  elle::Natural32
  Descriptor::Meta::extent() const
  {
    return (this->_extent);
  }

  void
  Descriptor::Meta::_everybody_subject()
  {
    // Create the subject corresponding to the everybody group, if necessary.
    // Note that this subject will never be serialized but is used to ease
    // the process of access control since most method manipulate subjects.
    if (this->_everybody.subject == nullptr)
      this->_everybody.subject =
        new nucleus::neutron::Subject(this->_everybody.identity);

    assert(this->_everybody.subject != nullptr);
  }

  elle::Status
  Descriptor::Meta::Dump(const elle::Natural32  margin) const
  {
    elle::String alignment(margin, ' ');
    elle::io::Unique unique;

    std::cout << alignment << "[Meta]" << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[ID] " << this->_id << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Administrator K] " << this->_administrator_K << std::endl;

    if (this->_model.Dump(margin + 2) == elle::Status::Error)
      throw elle::Exception("unable to dump the model");

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Root] " << std::endl;

    if (this->_root.Dump(margin + 4) == elle::Status::Error)
      throw elle::Exception("unable to dump the address");

    if (this->_everybody.identity.Save(unique) == elle::Status::Error)
      throw elle::Exception("unable to save the address");

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Everybody] " << unique << std::endl;

    if (this->_everybody.identity.Dump(margin + 4) == elle::Status::Error)
      throw elle::Exception("unable to dump the address");

    std::cout << alignment << elle::io::Dumpable::Shift << "[History] "
              << static_cast<elle::Natural32>(this->_history) << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift << "[Extent] "
              << this->_extent << std::endl;

    if (this->_signature != nullptr)
      {
        std::cout << alignment << elle::io::Dumpable::Shift << "[Signature] "
                  << *this->_signature << std::endl;
      }

    return (elle::Status::Ok);
  }

//
// ---------- data ------------------------------------------------------------
//

  Descriptor::Data::Data():
    _signature(nullptr)
  {
  }

  Descriptor::Data::Data(elle::String const& name,
                         hole::Openness const& openness,
                         horizon::Policy const& policy,
                         elle::Version const& version):
    _name(name),
    _openness(openness),
    _policy(policy),
    _version(version),
    _formats{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    _signature(nullptr)
  {
  }

  Descriptor::Data::~Data()
  {
    delete this->_signature;
  }

  void
  Descriptor::Data::seal(cryptography::PrivateKey const& administrator_k)
  {
    delete this->_signature;
    this->_signature = nullptr;
    this->_signature = new cryptography::Signature{
      administrator_k.sign(
        elle::serialize::make_tuple(
          this->_name,
          this->_openness,
          this->_policy,
          this->_version,
          this->_formats.block,
          this->_formats.content_hash_block,
          this->_formats.contents,
          this->_formats.immutable_block,
          this->_formats.imprint_block,
          this->_formats.mutable_block,
          this->_formats.owner_key_block,
          this->_formats.public_key_block,
          this->_formats.access,
          this->_formats.attributes,
          this->_formats.catalog,
          this->_formats.data,
          this->_formats.ensemble,
          this->_formats.group,
          this->_formats.object,
          this->_formats.reference,
          this->_formats.user,
          this->_formats.identity,
          this->_formats.descriptor))};
  }

  void
  Descriptor::Data::validate(
    cryptography::PublicKey const& administrator_K) const
  {
    ELLE_ASSERT(this->_signature != nullptr);

    if (administrator_K.verify(
          *this->_signature,
          elle::serialize::make_tuple(
            this->_name,
            this->_openness,
            this->_policy,
            this->_version,
            this->_formats.block,
            this->_formats.content_hash_block,
            this->_formats.contents,
            this->_formats.immutable_block,
            this->_formats.imprint_block,
            this->_formats.mutable_block,
            this->_formats.owner_key_block,
            this->_formats.public_key_block,
            this->_formats.access,
            this->_formats.attributes,
            this->_formats.catalog,
            this->_formats.data,
            this->_formats.ensemble,
            this->_formats.group,
            this->_formats.object,
            this->_formats.reference,
            this->_formats.user,
            this->_formats.identity,
            this->_formats.descriptor)) == false)
      throw std::runtime_error("unable to validate the data section "
                               "with the administrator's key");
  }

  elle::String const&
  Descriptor::Data::name() const
  {
    return (this->_name);
  }

  hole::Openness const&
  Descriptor::Data::openness() const
  {
    return (this->_openness);
  }

  horizon::Policy const&
  Descriptor::Data::policy() const
  {
    return (this->_policy);
  }

  elle::Version const&
  Descriptor::Data::version() const
  {
    return (this->_version);
  }

  /// Makes it easier to generate the accessors to the formats.
#define LUNE_DESCRIPTOR_FORMAT_ACCESSOR(_name_)                         \
  elle::serialize::Format const&                                        \
  Descriptor::Data::format_ ## _name_ () const                          \
  {                                                                     \
    return (this->_formats._name_);                                     \
  }

  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(block);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(content_hash_block);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(contents);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(immutable_block);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(imprint_block);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(mutable_block);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(owner_key_block);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(public_key_block);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(access);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(attributes);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(catalog);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(data);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(ensemble);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(group);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(object);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(reference);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(user);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(identity);
  LUNE_DESCRIPTOR_FORMAT_ACCESSOR(descriptor);

  elle::Status
  Descriptor::Data::Dump(const elle::Natural32  margin) const
  {
    elle::String alignment(margin, ' ');

    std::cout << alignment << "[Data]" << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift << "[Name] "
              << this->_name << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Openness] " << this->_openness << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Policy] " << this->_policy << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift << "[Version] "
              << this->_version << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Formats]" << std::endl;

#define LUNE_DESCRIPTOR_FORMAT_DUMPER(_name_)                           \
    std::cout << alignment << elle::io::Dumpable::Shift                 \
              << elle::io::Dumpable::Shift << "[" << #_name_ << "] "    \
              << this->_formats._name_ << std::endl;

    LUNE_DESCRIPTOR_FORMAT_DUMPER(block);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(content_hash_block);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(contents);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(immutable_block);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(imprint_block);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(mutable_block);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(owner_key_block);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(public_key_block);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(access);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(attributes);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(catalog);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(data);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(ensemble);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(group);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(object);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(reference);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(user);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(identity);
    LUNE_DESCRIPTOR_FORMAT_DUMPER(descriptor);

    if (this->_signature != nullptr)
      {
        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Signature] " << *this->_signature << std::endl;
      }

    return elle::Status::Ok;
  }

}
