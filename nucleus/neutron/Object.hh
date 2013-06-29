#ifndef NUCLEUS_NEUTRON_OBJECT_HH
# define NUCLEUS_NEUTRON_OBJECT_HH

# include <elle/types.hh>
# include <elle/utility/Time.hh>
# include <elle/serialize/Serializable.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/fwd.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/ImprintBlock.hh>
# include <nucleus/proton/Revision.hh>
# include <nucleus/neutron/fwd.hh>
# include <nucleus/neutron/Genre.hh>
# include <nucleus/neutron/Size.hh>
# include <nucleus/neutron/Permissions.hh>
# include <nucleus/neutron/Token.hh>

namespace nucleus
{
  namespace neutron
  {
    /// XXX[to rewrite]
    /// this class is the most important of the whole Infinit project
    /// as it describes file system objects being files, directories and
    /// references.
    ///
    /// an object can rely on several physical constructors such as an
    /// ImprintBlock, an OwnerKeyBlock or else.
    ///
    /// basically, an object contains the author i.e the user having performed
    /// the latest modification along with the address of the Access block
    /// which contains the set of authorised users and groups. note however
    /// than most objects do not reference an access block in which cases
    /// the object is considered private i.e only its owner has access.
    ///
    /// in addition, several meta information are contained such as the
    /// owner permissions, some timestamps, the attributes etc.
    ///
    /// finally, the data section contains the address of the object's
    /// contents though the nature of the contents depends upon the
    /// object's nature: file, directory or reference.
    ///
    /// noteworthy is tha meta.owner.record is generated in order to
    /// make it as easy to manipulate the owner entry as for other access
    /// records. thus, this attribute is never serialized.
    ///
    class Object:
      public proton::ImprintBlock,
      public elle::serialize::SerializableMixin<Object>,
      public elle::concept::UniquableMixin<Object>
    {
      /*----------.
      | Constants |
      `----------*/
    public:
      struct Constants
      {
        static Component const component;
      };

      //
      // enumerations
      //
    public:
      /// This defines the roles that a user can play on an object.
      enum Role
        {
          RoleUnknown,
          RoleOwner,
          RoleLord,
          RoleVassal,
          RoleNone
        };

      //
      // constructors & destructors
      //
    public:
      Object(); // XXX[use deserialize constructor]
      Object(proton::Network const& network,
             cryptography::PublicKey const& owner_K,
             Genre const genre);
      ELLE_SERIALIZE_CONSTRUCT_DECLARE(Object);
      ~Object();

      //
      // methods
      //
    public:
      elle::Status      Update(const Author& author,
                               proton::Radix const& contents,
                               const Size& size,
                               proton::Radix const& access,
                               const Token& owner_token);

      elle::Status      Administrate(proton::Radix const& attributes,
                                     Permissions const permissions);

      elle::Status      Seal(cryptography::PrivateKey const&,
                             cryptography::Digest const& fingerprint =
                               cryptography::Digest(0));

      /// XXX
      proton::Radix const&
      access() const;
      /// Returns the record associated with the object owner.
      /// XXX[the const on the return type needs to be added]
      Record&
      owner_record();
      /// Returns the owner's access token.
      Token const&
      owner_token() const;
      /// Returns the owner's access permissions.
      Permissions const&
      owner_permissions() const;
      /// Returns the object's genre: file, directory or link.
      Genre const&
      genre() const;
      /// Returns the object's author i.e last writer.
      Author const&
      author() const;
      /// Returns the address of the contents block.
      proton::Radix const&
      contents() const;
      /// Returns the attributes associated with the object.
      proton::Radix const&
      attributes() const;
      /// Returns the size of the object's content.
      Size const&
      size() const;
      /// Returns the timestamp associated with the data section.
      elle::utility::Time const&
      data_modification_timestamp() const;
      /// Returns the timestamp associated with the meta section.
      elle::utility::Time const&
      meta_modification_timestamp() const;
      /// Returns the revision associated with the data section.
      proton::Revision const&
      data_revision() const;
      /// Returns the revision associated with the meta section.
      proton::Revision const&
      meta_revision() const;

    private:
      /// Computes the owner's record, if necessary.
      void
      _owner_record();

      //
      // interfaces
      //
    public:
      // block
      void
      validate(proton::Address const& address,
               cryptography::Digest const& fingerprint =
                 cryptography::Digest(0)) const;
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // printable
      virtual
      void
      print(std::ostream& stream) const;
      // serialize
      ELLE_SERIALIZE_FRIEND_FOR(Object);
      ELLE_SERIALIZE_SERIALIZABLE_METHODS(Object);

      //
      // attributes
      //
    private:
      Author* _author;

      struct
      {
        // XXX to implement: proton::Base               base;

        struct
        {
          Permissions permissions;
          Token token;
          Record* record;
        } owner;

        Genre genre;
        elle::utility::Time modification_timestamp;

        proton::Radix* attributes;
        proton::Radix* access;

        proton::Revision revision;
        cryptography::Signature* signature;

        proton::State state;
      } _meta;

      struct
      {
        // XXX to implement: proton::Base               base;

        proton::Radix* contents;

        Size size;
        elle::utility::Time modification_timestamp;

        proton::Revision revision;
        cryptography::Signature* signature;

        proton::State state;
      } _data;
    };

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Object::Role const role);
  }
}

# include <nucleus/neutron/Object.hxx>

#endif
