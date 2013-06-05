#include <elle/io/File.hh>
#include <elle/io/Piece.hh>
#include <elle/os/path.hh>
#include <elle/log.hh>

#include <common/common.hh>

#include <cryptography/PrivateKey.hh>
#include <cryptography/KeyPair.hh>
#include <cryptography/oneway.hh>

#include <hole/Openness.hh>

#include <lune/Identity.hh>

#include <nucleus/proton/Address.hh>

#include <Infinit.hh>
#include <infinit/Descriptor.hh>

ELLE_LOG_COMPONENT("infinit.Descriptor");

//
// ---------- Descriptor ------------------------------------------------------
//

namespace infinit
{
  // XXX remove this constructor in favor of something like:
  //       template <typename T>
  //       Descriptor(elle::String const& path,
  //                  T const& authority);
  Descriptor::Descriptor(elle::String const& user,
                         elle::String const& network)
  {
    ELLE_TRACE("creating descriptor of network %s from %s",
               network,
               common::infinit::descriptor_path(user, network));

    if (Descriptor::exists(user, network) == false)
      throw elle::Exception(
        elle::sprintf("network %s does not seem to exist for user %s",
                      network, user));

    this->load(user, network);

    if (this->validate(Infinit::authority().K()) == false)
      throw elle::Exception("unable to validate the descriptor");
  }

  Descriptor::Descriptor(descriptor::Meta const& meta,
                         descriptor::Data const& data):
    _meta(new descriptor::Meta(meta)),
    _data(new descriptor::Data(data))
  {
  }

  Descriptor::Descriptor(descriptor::Meta&& meta,
                         descriptor::Data&& data):
    _meta(new descriptor::Meta(std::move(meta))),
    _data(new descriptor::Data(std::move(data)))
  {
  }

  Descriptor::Descriptor(Descriptor const& other):
    _meta(new descriptor::Meta(*other._meta)),
    _data(new descriptor::Data(*other._data))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Descriptor)
  {
  }

  /*--------.
  | Methods |
  `--------*/

  descriptor::Meta const&
  Descriptor::meta() const
  {
    ELLE_ASSERT_NEQ(this->_meta, nullptr);

    return (*this->_meta);
  }

  descriptor::Data const&
  Descriptor::data() const
  {
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    return (*this->_data);
  }

  void
  Descriptor::data(descriptor::Data const& data)
  {
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    this->_data.reset(new descriptor::Data(data));
  }

  void
  Descriptor::data(descriptor::Data&& data)
  {
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    this->_data.reset(new descriptor::Data(std::move(data)));
  }

  /*---------.
  | Fileable |
  `---------*/

  void
  Descriptor::load(elle::String const& user,
                   elle::String const& network)
  {
    ELLE_TRACE_FUNCTION(user, network);

    this->load(
      elle::io::Path{common::infinit::descriptor_path(user, network)});
  }

  void
  Descriptor::store(lune::Identity const& identity) const
  {
    ELLE_TRACE_METHOD(identity);

    ELLE_ASSERT_NEQ(this->_meta, nullptr);

    this->store(
      elle::io::Path{
        common::infinit::descriptor_path(identity.id(),
                                         this->_meta->identifier())});
  }

  void
  Descriptor::erase(elle::String const& user,
                    elle::String const& network)
  {
    ELLE_TRACE_FUNCTION(user, network);

    elle::concept::Fileable<>::erase(
      elle::io::Path{common::infinit::descriptor_path(user, network)});
  }

  elle::Boolean
  Descriptor::exists(elle::String const& user,
                     elle::String const& network)
  {
    ELLE_TRACE_FUNCTION(user, network);

    return (elle::os::path::exists(
              common::infinit::descriptor_path(user, network)));
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Descriptor::print(std::ostream& stream) const
  {
    ELLE_ASSERT_NEQ(this->_meta, nullptr);
    ELLE_ASSERT_NEQ(this->_data, nullptr);

    stream << "[" << *this->_meta << ", " << *this->_data << "]";
  }
}

//
// ---------- Meta ------------------------------------------------------------
//

namespace infinit
{
  namespace descriptor
  {
    /*-------------.
    | Construction |
    `-------------*/

    Meta::Meta()
    {
    }

    Meta::Meta(elle::String identifier,
               cryptography::PublicKey administrator_K,
               hole::Model model,
               nucleus::proton::Address root,
               nucleus::neutron::Group::Identity everybody,
               elle::Boolean history,
               elle::Natural32 extent,
               cryptography::Signature signature):
      _identifier(std::move(identifier)),
      _administrator_K(std::move(administrator_K)),
      _model(std::move(model)),
      _root(std::move(root)),
      _everybody_identity(std::move(everybody)),
      _history(std::move(history)),
      _extent(std::move(extent)),
      _signature(std::move(signature))
    {
    }

    Meta::Meta(Meta const& other):
      _identifier(other._identifier),
      _administrator_K(other._administrator_K),
      _model(other._model),
      _root(other._root),
      _everybody_identity(other._everybody_identity),
      _history(other._history),
      _extent(other._extent),
      _signature(other._signature)
    {
    }

    Meta::Meta(Meta&& other):
      _identifier(std::move(other._identifier)),
      _administrator_K(std::move(other._administrator_K)),
      _model(std::move(other._model)),
      _root(std::move(other._root)),
      _everybody_identity(std::move(other._everybody_identity)),
      _history(std::move(other._history)),
      _extent(std::move(other._extent)),
      _signature(std::move(other._signature))
    {
    }

    /*--------.
    | Methods |
    `--------*/

    nucleus::neutron::Subject const&
    Meta::everybody_subject() const
    {
      // Create the subject corresponding to the everybody group, if necessary.
      // Note that this subject will never be serialized but is used to ease
      // the process of access control since most methods manipulate subjects.
      if (this->_everybody_subject == nullptr)
        this->_everybody_subject.reset(
          new nucleus::neutron::Subject(this->_everybody_identity));

      return (*this->_everybody_subject);
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(Meta /* XXX,
                                    administrator_K, model, root,
                                    everybody_identity, signature */)
    {
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Meta::print(std::ostream& stream) const
    {
      stream << this->_identifier << "("
             << this->_administrator_K << ", "
             << this->_model << ", "
             << this->_root << ", "
             << this->_history << ", "
             << this->_extent << ")";
    }

    namespace meta
    {
      /*----------.
      | Functions |
      `----------*/

      cryptography::Digest
      hash(elle::String const& identifier,
           cryptography::PublicKey const& administrator_K,
           hole::Model const& model,
           nucleus::proton::Address const& root,
           nucleus::neutron::Group::Identity const& everybody,
           elle::Boolean history,
           elle::Natural32 extent)
      {
        return (cryptography::oneway::hash(
                  elle::serialize::make_tuple(identifier,
                                              administrator_K,
                                              model,
                                              root,
                                              everybody,
                                              history,
                                              extent),
                  cryptography::KeyPair::oneway_algorithm));
      }
    }
  }
}

//
// ---------- Data ------------------------------------------------------------
//

namespace infinit
{
  namespace descriptor
  {
    /*-------------.
    | Construction |
    `-------------*/

    Data::Data()
    {
    }

    Data::Data(elle::String name,
               hole::Openness openness,
               horizon::Policy policy,
               elle::Version version,
               elle::serialize::Format format_block,
               elle::serialize::Format format_content_hash_block,
               elle::serialize::Format format_contents,
               elle::serialize::Format format_immutable_block,
               elle::serialize::Format format_imprint_block,
               elle::serialize::Format format_mutable_block,
               elle::serialize::Format format_owner_key_block,
               elle::serialize::Format format_public_key_block,
               elle::serialize::Format format_access,
               elle::serialize::Format format_attributes,
               elle::serialize::Format format_catalog,
               elle::serialize::Format format_data,
               elle::serialize::Format format_ensemble,
               elle::serialize::Format format_group,
               elle::serialize::Format format_object,
               elle::serialize::Format format_reference,
               elle::serialize::Format format_user,
               elle::serialize::Format format_identity,
               elle::serialize::Format format_descriptor,
               cryptography::Signature signature):
      _name(std::move(name)),
      _openness(std::move(openness)),
      _policy(std::move(policy)),
      _version(std::move(version)),
      _format_block(std::move(format_block)),
      _format_content_hash_block(std::move(format_content_hash_block)),
      _format_contents(std::move(format_contents)),
      _format_immutable_block(std::move(format_immutable_block)),
      _format_imprint_block(std::move(format_imprint_block)),
      _format_mutable_block(std::move(format_mutable_block)),
      _format_owner_key_block(std::move(format_owner_key_block)),
      _format_public_key_block(std::move(format_public_key_block)),
      _format_access(std::move(format_access)),
      _format_attributes(std::move(format_attributes)),
      _format_catalog(std::move(format_catalog)),
      _format_data(std::move(format_data)),
      _format_ensemble(std::move(format_ensemble)),
      _format_group(std::move(format_group)),
      _format_object(std::move(format_object)),
      _format_reference(std::move(format_reference)),
      _format_user(std::move(format_user)),
      _format_identity(std::move(format_identity)),
      _format_descriptor(std::move(format_descriptor)),
      _signature(std::move(signature))
    {
    }

    Data::Data(Data const& other):
      _name(other._name),
      _openness(other._openness),
      _policy(other._policy),
      _version(other._version),
      _format_block(other._format_block),
      _format_content_hash_block(other._format_content_hash_block),
      _format_contents(other._format_contents),
      _format_immutable_block(other._format_immutable_block),
      _format_imprint_block(other._format_imprint_block),
      _format_mutable_block(other._format_mutable_block),
      _format_owner_key_block(other._format_owner_key_block),
      _format_public_key_block(other._format_public_key_block),
      _format_access(other._format_access),
      _format_attributes(other._format_attributes),
      _format_catalog(other._format_catalog),
      _format_data(other._format_data),
      _format_ensemble(other._format_ensemble),
      _format_group(other._format_group),
      _format_object(other._format_object),
      _format_reference(other._format_reference),
      _format_user(other._format_user),
      _format_identity(other._format_identity),
      _format_descriptor(other._format_descriptor),
      _signature(other._signature)
    {
    }

    Data::Data(Data&& other):
      _name(std::move(other._name)),
      _openness(std::move(other._openness)),
      _policy(std::move(other._policy)),
      _version(std::move(other._version)),
      _format_block(std::move(other._format_block)),
      _format_content_hash_block(std::move(other._format_content_hash_block)),
      _format_contents(std::move(other._format_contents)),
      _format_immutable_block(std::move(other._format_immutable_block)),
      _format_imprint_block(std::move(other._format_imprint_block)),
      _format_mutable_block(std::move(other._format_mutable_block)),
      _format_owner_key_block(std::move(other._format_owner_key_block)),
      _format_public_key_block(std::move(other._format_public_key_block)),
      _format_access(std::move(other._format_access)),
      _format_attributes(std::move(other._format_attributes)),
      _format_catalog(std::move(other._format_catalog)),
      _format_data(std::move(other._format_data)),
      _format_ensemble(std::move(other._format_ensemble)),
      _format_group(std::move(other._format_group)),
      _format_object(std::move(other._format_object)),
      _format_reference(std::move(other._format_reference)),
      _format_user(std::move(other._format_user)),
      _format_identity(std::move(other._format_identity)),
      _format_descriptor(std::move(other._format_descriptor)),
      _signature(std::move(other._signature))
    {
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(Data /* XXX,
                                    openness, policy, version,
                                    format_block, format_content_hash_block,
                                    format_contents, format_immutable_block,
                                    format_imprint_block, format_mutable_block,
                                    format_owner_key_block,
                                    format_public_key_block, format_access,
                                    format_attributes, format_catalog,
                                    format_data, format_ensemble, format_group,
                                    format_object, format_reference,
                                    format_user, format_identity,
                                    format_descriptor, format_signature */)
    {
    }

    /*----------.
    | Printable |
    `----------*/

    void
    Data::print(std::ostream& stream) const
    {
      stream << this->_name << "("
             << this->_openness << ", "
             << this->_policy << ", "
             << this->_version << ")";
    }

    namespace data
    {
      /*----------.
      | Functions |
      `----------*/

      cryptography::Digest
      hash(elle::String const& name,
           hole::Openness const& openness,
           horizon::Policy const& policy,
           elle::Version const& version,
           elle::serialize::Format const& format_block,
           elle::serialize::Format const& format_content_hash_block,
           elle::serialize::Format const& format_contents,
           elle::serialize::Format const& format_immutable_block,
           elle::serialize::Format const& format_imprint_block,
           elle::serialize::Format const& format_mutable_block,
           elle::serialize::Format const& format_owner_key_block,
           elle::serialize::Format const& format_public_key_block,
           elle::serialize::Format const& format_access,
           elle::serialize::Format const& format_attributes,
           elle::serialize::Format const& format_catalog,
           elle::serialize::Format const& format_data,
           elle::serialize::Format const& format_ensemble,
           elle::serialize::Format const& format_group,
           elle::serialize::Format const& format_object,
           elle::serialize::Format const& format_reference,
           elle::serialize::Format const& format_user,
           elle::serialize::Format const& format_identity,
           elle::serialize::Format const& format_descriptor)
      {
        return (cryptography::oneway::hash(
                  elle::serialize::make_tuple(
                    name,
                    openness,
                    policy,
                    version,
                    format_block,
                    format_content_hash_block,
                    format_contents,
                    format_immutable_block,
                    format_imprint_block,
                    format_mutable_block,
                    format_owner_key_block,
                    format_public_key_block,
                    format_access,
                    format_attributes,
                    format_catalog,
                    format_data,
                    format_ensemble,
                    format_group,
                    format_object,
                    format_reference,
                    format_user,
                    format_identity,
                    format_descriptor),
                  cryptography::KeyPair::oneway_algorithm));
      }
    }
  }
}
