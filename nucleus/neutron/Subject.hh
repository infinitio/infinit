#ifndef NUCLEUS_NEUTRON_SUBJECT_HH
# define NUCLEUS_NEUTRON_SUBJECT_HH

# include <elle/types.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/neutron/User.hh>
# include <nucleus/neutron/Group.hh>

# include <utility>
ELLE_OPERATOR_RELATIONALS();

namespace nucleus
{
  namespace neutron
  {

    /// This class is used to represent a subject i.e an entity which
    /// can be granted access such as a user or a group.
    class Subject:
      public elle::Printable
    {
      //
      // enumerations
      //
    public:
      enum Type
        {
          TypeUnknown,

          TypeUser,
          TypeGroup,

          Types
        };

      //
      // structures
      //
    public:
      struct Descriptor
      {
        Type type;
        elle::String name;
      };

      //
      // static methods
      //
    public:
      /// Converts a string into a subject type.
      static
      elle::Status
      Convert(elle::String const&,
              Type&);
      /// Converts a subject type into a string.
      static
      elle::Status
      Convert(Type const,
              elle::String&);

      //
      // static attributes
      //
    private:
      /// This table maintains a mapping between subject types and their
      /// respective human-readable representations.
      static const Descriptor _descriptors[Types];

      //
      // construction
      //
    public:
      Subject();
      Subject(User::Identity const& identity);
      Subject(Group::Identity const& identity);
      Subject(Subject const& other);
      virtual
      ~Subject();

      //
      // methods
      //
    public:
      /// XXX[to remove in favour of the constructor]
      elle::Status
      Create(User::Identity const& identity);
      /// XXX[to remove in favour of the constructor]
      elle::Status
      Create(Group::Identity const& identity);
      /// Returns the subject type: user or group.
      Type
      type() const;
      /// Returns the user's identity i.e public key.
      User::Identity const&
      user() const;
      /// Returns the group's identity i.e address.
      Group::Identity const&
      group() const;

      //
      // operators
      //
    public:
      elle::Boolean
      operator ==(Subject const& other) const;
      ELLE_OPERATOR_NEQ(Subject);
      elle::Boolean
      operator <(Subject const& other) const;
      ELLE_OPERATOR_ASSIGNMENT(Subject); // XXX

      //
      // interfaces
      //
    public:
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // printable
      virtual
      void
      print(std::ostream& stream) const;
      // serialize
      ELLE_SERIALIZE_FRIEND_FOR(Subject);

      //
      // attributes
      //
    private:
      Type _type;

      union
      {
        User::Identity* _user;
        Group::Identity* _group;
      };
    };

  }
}

# include <nucleus/neutron/Subject.hxx>

#endif
