#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Attributes.hh>
#include <nucleus/neutron/Token.hh>
#include <nucleus/neutron/Component.hh>
#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Author.hh>
#include <nucleus/neutron/Record.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Radix.hh>
#include <nucleus/proton/Address.hh>

#include <cryptography/Digest.hh>
#include <cryptography/PrivateKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <elle/serialize/TupleSerializer.hxx>

namespace nucleus
{
  namespace neutron
  {

    /*----------.
    | Constants |
    `----------*/

    Component const Object::Constants::component{ComponentObject};

//
// ---------- constructors & destructors --------------------------------------
//

    Object::Object():
      proton::ImprintBlock(),

      _author(nullptr)
    {
      this->_meta.state = proton::State::clean;
      this->_meta.owner.permissions = permissions::none;
      this->_meta.owner.token = Token::null();
      this->_meta.owner.record = nullptr;
      this->_meta.attributes = nullptr;
      this->_meta.access = nullptr;
      this->_meta.revision = 0;
      this->_meta.signature = nullptr;

      this->_data.contents = nullptr;
      this->_data.state = proton::State::clean;
      this->_data.size = 0;
      this->_data.revision = 0;
      this->_data.signature = nullptr;
    }

    ///
    /// this method initializes the object.
    ///
    /// XXX
    ///
    ///
    /// this method creates the object given the owner public and the
    /// genre of the object to create.
    ///
    /// the method (i) starts by initializing the underlying public key block
    /// (ii) sets the meta data, and finally (iv) initializes the data
    /// part by setting the owner as the author.
    ///
    Object::Object(proton::Network const& network,
                   cryptography::PublicKey const& owner_K,
                   Genre const genre):
      proton::ImprintBlock(network, ComponentObject, owner_K),

      _author(new Author)
    {
      //
      // the attributes below are initialized in the constructor body
      // because they belong to a sub-structure.
      //
      this->_meta.state = proton::State::clean;
      this->_meta.owner.permissions = permissions::none;
      this->_meta.owner.token = Token::null();
      this->_meta.owner.record = nullptr;
      this->_meta.attributes = nullptr;
      this->_meta.access = nullptr;
      this->_meta.revision = 0;
      this->_meta.signature = nullptr;

      this->_data.contents = nullptr;
      this->_data.state = proton::State::clean;
      this->_data.size = 0;
      this->_data.revision = 0;
      this->_data.signature = nullptr;

      // (i)
      {
        // set the meta genre.
        this->_meta.genre = genre;

        // set the initial owner permissions to all with an empty key.
        if (this->Administrate(
              proton::Radix{},
              permissions::read | permissions::write) == elle::Status::Error)
          throw Exception("unable to set the initial meta data");
      }

      // (ii)
      {
        // Set the initial data with no contents and the owner as the author.
        if (this->Update(Author{},
                         proton::Radix{},
                         0,
                         proton::Radix{},
                         this->_meta.owner.token) == elle::Status::Error)
          throw Exception("unable to set the initial data");
      }
    }

    ELLE_SERIALIZE_CONSTRUCT_DEFINE(Object, ImprintBlock)
    {
      this->_author = nullptr;
      this->_meta.owner.record = nullptr;
      this->_meta.attributes = nullptr;
      this->_meta.access = nullptr;
      this->_meta.signature = nullptr;
      this->_data.contents = nullptr;
      this->_data.signature = nullptr;
    }

    Object::~Object()
    {
      delete this->_author;
      delete this->_meta.owner.record;
      delete this->_meta.attributes;
      delete this->_meta.access;
      delete this->_meta.signature;
      delete this->_data.contents;
      delete this->_data.signature;
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method updates the data section.
    ///
    elle::Status        Object::Update(const Author&            author,
                                       proton::Radix const& contents,
                                       const Size&              size,
                                       proton::Radix const& access,
                                       const Token&             token)

    {
      // set the author.
      // XXX[delete and re-new]
      *this->_author = author;

      //
      // update the elements in the data section.
      //
      {
        // set the last update time.
        if (this->_data.modification_timestamp.Current() == elle::Status::Error)
          throw Exception("unable to set the last update time");

        // XXX[hack to prevent too much allocations]
        if (this->_data.contents != &contents)
          {
            delete this->_data.contents;
            this->_data.contents = new proton::Radix{contents};
          }

        // set the size.
        this->_data.size = size;
      }

      //
      // update the elements in the meta section, though they are
      // included in the data signature.
      //
      {
        // XXX[hack to prevent too much allocations]
        if (this->_meta.access != &access)
          {
            delete this->_meta.access;
            this->_meta.access = new proton::Radix{access};
          }

        // set the owner token.
        this->_meta.owner.token = token;
      }

      // mark the section as dirty.
      this->_data.state = proton::State::dirty;

      // mark the block as dirty.
      this->state(proton::State::dirty);

      return elle::Status::Ok;
    }

    ///
    /// this method updates the meta section.
    ///
    elle::Status        Object::Administrate(proton::Radix const& attributes,
                                             Permissions const permissions)
    {
      // set the last management time.
      if (this->_meta.modification_timestamp.Current() == elle::Status::Error)
        throw Exception("unable to set the last management time");

      // XXX[hack to prevent too much allocations]
      if (this->_meta.attributes != &attributes)
        {
          delete this->_meta.attributes;
          this->_meta.attributes = new proton::Radix{attributes};
        }

      // set the owner permissions.
      this->_meta.owner.permissions = permissions;

      // mark the section as dirty.
      this->_meta.state = proton::State::dirty;

      // make sure the owner's record is computed.
      this->_owner_record();

      assert(this->_meta.owner.record != nullptr);

      // re-compute the owner's access record. just like this->owner.subject,
      // this attribute is not mandatory but has been introduced in order
      // to simplify access control management.
      delete this->_meta.owner.record;
      this->_meta.owner.record = new Record(this->owner_subject(),
                                            this->_meta.owner.permissions,
                                            this->_meta.owner.token);

      // set the the block as dirty.
      this->state(proton::State::dirty);

      return elle::Status::Ok;
    }

    ///
    /// this method seals the data and meta data by signing them.
    ///
    elle::Status        Object::Seal(cryptography::PrivateKey const&    k,
                                     cryptography::Digest const& fingerprint)
    {
      // re-sign the data if required.
      if (this->_data.state == proton::State::dirty)
        {
          // increase the data revision.
          this->_data.revision += 1;

          // sign the archive with the author key.
          ELLE_ASSERT(this->_data.contents != nullptr);
          ELLE_ASSERT(this->_meta.access != nullptr);

          this->_data.signature = new cryptography::Signature{
            k.sign(elle::serialize::make_tuple(
                     *this->_data.contents,
                     this->_data.size,
                     this->_data.modification_timestamp,
                     this->_data.revision,
                     this->_meta.owner.token,
                     *this->_meta.access))};

          // mark the section as consistent.
          this->_data.state = proton::State::consistent;
        }

      // re-sign the meta data if required.
      if (this->_meta.state == proton::State::dirty)
        {
          // increase the meta revision.
          this->_meta.revision += 1;

          // perform the meta signature depending on the presence of a
          // reference to an access block.

          // sign the meta data, making sure to include the access
          // fingerprint.
          ELLE_ASSERT(this->_meta.attributes != nullptr);

          this->_meta.signature = new cryptography::Signature{
            k.sign(elle::serialize::make_tuple(
                     this->_meta.owner.permissions,
                     this->_meta.genre,
                     this->_meta.modification_timestamp,
                     *this->_meta.attributes,
                     this->_meta.revision,
                     fingerprint))};

          // mark the section as consistent.
          this->_meta.state = proton::State::consistent;
        }

      // set the mutable block's revision.
      assert((this->_meta.revision + this->_data.revision) > this->revision());
      this->revision(this->_meta.revision + this->_data.revision);

      // set the block as consistent.
      this->state(proton::State::consistent);

      return elle::Status::Ok;
    }

    proton::Radix const&
    Object::access() const
    {
      ELLE_ASSERT(this->_meta.access != nullptr);

      return (*this->_meta.access);
    }

    Record&
    Object::owner_record()
    {
      _owner_record();

      return (*this->_meta.owner.record);
    }

    Token const&
    Object::owner_token() const
    {
      return (this->_meta.owner.token);
    }

    Permissions const&
    Object::owner_permissions() const
    {
      return (this->_meta.owner.permissions);
    }

    Genre const&
    Object::genre() const
    {
      return (this->_meta.genre);
    }

    Author const&
    Object::author() const
    {
      assert(this->_author != nullptr);

      return (*this->_author);
    }

    proton::Radix const&
    Object::contents() const
    {
      ELLE_ASSERT(this->_data.contents);

      return (*this->_data.contents);
    }

    proton::Radix const&
    Object::attributes() const
    {
      ELLE_ASSERT(this->_meta.attributes);

      return (*this->_meta.attributes);
    }

    Size const&
    Object::size() const
    {
      return (this->_data.size);
    }

    elle::utility::Time const&
    Object::data_modification_timestamp() const
    {
      return (this->_data.modification_timestamp);
    }

    elle::utility::Time const&
    Object::meta_modification_timestamp() const
    {
      return (this->_meta.modification_timestamp);
    }

    proton::Revision const&
    Object::data_revision() const
    {
      return (this->_data.revision);
    }

    proton::Revision const&
    Object::meta_revision() const
    {
      return (this->_meta.revision);
    }

    void
    Object::_owner_record()
    {
      // Create the record corresponding to the object owner, if necessary.
      // Note that this record will never be serialized but is used to ease
      // the process of access control since most method return a record.
      if (this->_meta.owner.record == nullptr)
        this->_meta.owner.record = new Record(this->owner_subject(),
                                              this->_meta.owner.permissions,
                                              this->_meta.owner.token);

      assert (this->_meta.owner.record != nullptr);
    }

//
// ---------- block -----------------------------------------------------------
//

    /// Implements the Block's validate() interface method.
    ///
    /// However, since the Object requires additional information in
    /// order to be validated, this method must *never* be used and therefore
    /// returns an error.
    void
    Object::validate(proton::Address const&) const
    {
      throw Exception("this method should never have been called");
    }

    void
    Object::validate(proton::Address const& address,
                     cryptography::Digest const& fingerprint) const
    {
      /// The method (i) calls the parent class for validation (iii) verifies
      /// the meta part's signature (iv) retrieves the author's public key
      /// (v) verifies the data signature and (vi) verify that the mutable
      /// block's general revision number matches the object's revisions.

      cryptography::PublicKey const* author = nullptr;

      // (i)
      {
        // call the parent class.
        proton::ImprintBlock::validate(address);
      }

      // (ii)
      {
        // verify the meta part, including the access fingerprint.
        ELLE_ASSERT(this->_meta.attributes != nullptr);
        ELLE_ASSERT(this->_meta.signature != nullptr);

        if (this->owner_K().verify(
              *this->_meta.signature,
              elle::serialize::make_tuple(
                this->_meta.owner.permissions,
                this->_meta.genre,
                this->_meta.modification_timestamp,
                *this->_meta.attributes,
                this->_meta.revision,
                fingerprint)) == false)
          throw Exception("unable to verify the meta's signature");
      }

      // (iii)
      {
        switch (this->_author->role)
          {
          case Object::RoleOwner:
            {
              // copy the public key.
              author = &this->owner_K();

              //
              // note that the owner's permission to write the object
              // is not checked because of the regulation mechanism.
              //
              // indeed, whenever the owner removes the write permission
              // from the object's current author, the object gets inconsistent
              // because anyone retrieving the object and verifying it would
              // conclude that the author does not have the write permission.
              //
              // to overcome this problem, the owner re-signs the data
              // in order to ensure consistency, no matter the owner's
              // permission on his own object.
              //
              // therefore, since one cannot distinguish both cases, the
              // owner's permissions are never checked through the validation
              // process. note however that by relying on the Infinit API,
              // the owner would not be able to modify his object without
              // the write permission, the software rejecting such an
              // operation.
              //

              break;
            }
          case Object::RoleLord:
            {
              // XXX[c'est la merde pour valider, il faut le record du author,
              //     a paser en argument du coup]
              ELLE_ASSERT(false);
              /* XXX
              // check that an access block has been provided.
              if (access == nullptr)
                throw Exception("the Validate() method must take the object's "
                                "access block");

              // retrieve the access record corresponding to the author's
              // index.
              Record const& record = access->select(this->_author->lord.index);

              // check the access record permissions for the given author.
              if ((record.permissions() & permissions::write) !=
                  permissions::write)
                throw Exception("the object's author does not seem to have had "
                                "the permission to modify this object");

              // check that the subject is indeed a user.
              if (record.subject().type() != Subject::TypeUser)
                throw Exception("the author references an access record which "
                                "is not related to a user");

              // finally, set the user's public key.
              //
              // note that a copy is made to avoid any complications.
              author = &record.subject().user();
              */

              break;
            }
          case Object::RoleVassal:
            {
              // XXX to implement.

              break;
            }
          default:
            {
              throw Exception(elle::sprintf("unexpected author's role '%u'",
                                            this->_author->role));
            }
          }
      }

      ELLE_ASSERT(author != nullptr);

      // (iv)
      {
        // verify the signature.
        ELLE_ASSERT(this->_data.contents != nullptr);
        ELLE_ASSERT(this->_data.signature != nullptr);
        ELLE_ASSERT(this->_meta.access != nullptr);

        if (author->verify(*this->_data.signature,
                           elle::serialize::make_tuple(
                             *this->_data.contents,
                             this->_data.size,
                             this->_data.modification_timestamp,
                             this->_data.revision,
                             this->_meta.owner.token,
                             *this->_meta.access)) == false)
          throw Exception("unable to verify the data signature");
      }

      // (v)
      {
        // check the mutable block's general revision.
        if (this->revision() != (this->_data.revision + this->_meta.revision))
          throw Exception("invalid revision number");
      }
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this function dumps an object.
    ///
    elle::Status        Object::Dump(elle::Natural32            margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[Object]" << std::endl;

      // dump the parent class.
      if (proton::ImprintBlock::Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the underlying owner key block");

      // dump the author part.
      if (this->_author->Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the author");

      // dump the meta part.
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Meta]" << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Owner] " << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift << "[Permissions] " << std::dec
                << (int)this->_meta.owner.permissions << std::endl;

      if (this->_meta.owner.token.Dump(margin + 6) == elle::Status::Error)
        throw Exception("unable to dump the meta owner's token");

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Genre] " << this->_meta.genre
                << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Modification Timestamp] " << std::endl;
      if (this->_meta.modification_timestamp.Dump(margin + 6) == elle::Status::Error)
        throw Exception("unable to dump the meta timestamp");

      ELLE_ASSERT(this->_meta.attributes != nullptr);
      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Attributes] " << *this->_meta.attributes << std::endl;

      ELLE_ASSERT(this->_meta.access != nullptr);
      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Access] " << *this->_meta.access << std::endl;

      if (this->_meta.revision.Dump(margin + 4) == elle::Status::Error)
        throw Exception("unable to dump the meta revision");

      ELLE_ASSERT(this->_meta.signature != nullptr);
      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Signature] " << *this->_meta.signature << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[State] " << std::dec << this->_meta.state << std::endl;

      // dump the data part.
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Data]" << std::endl;

      ELLE_ASSERT(this->_data.contents != nullptr);
      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Contents] " << *this->_data.contents << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Size] " << std::dec << this->_data.size << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Modification Timestamp]" << std::endl;
      if (this->_data.modification_timestamp.Dump(margin + 6) == elle::Status::Error)
        throw Exception("unable to dump the data timestamp");

      if (this->_data.revision.Dump(margin + 4) == elle::Status::Error)
        throw Exception("unable to dump the data revision");

      ELLE_ASSERT(this->_data.signature != nullptr);
      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[Signature] " << *this->_data.signature << std::endl;

      std::cout << alignment << elle::io::Dumpable::Shift
                << elle::io::Dumpable::Shift
                << "[State] " << std::dec << this->_data.state << std::endl;

      return elle::Status::Ok;
    }

//
// ---------- printable -------------------------------------------------------
//

    void
    Object::print(std::ostream& stream) const
    {
      stream << "object{"
             << this->_meta.genre
             << ", "
             << this->_data.size
             << "}";
    }

//
// ---------- operators -------------------------------------------------------
//

    std::ostream&
    operator <<(std::ostream& stream,
                Object::Role const role)
    {
      switch (role)
        {
        case Object::RoleUnknown:
          {
            stream << "unknown";
            break;
          }
        case Object::RoleOwner:
          {
            stream << "owner";
            break;
          }
        case Object::RoleLord:
          {
            stream << "lord";
            break;
          }
        case Object::RoleVassal:
          {
            stream << "vassal";
            break;
          }
        case Object::RoleNone:
          {
            stream << "none";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown object roel '%s'", role));
          }
        }

      return (stream);
    }

  }
}
