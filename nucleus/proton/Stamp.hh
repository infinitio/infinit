#ifndef NUCLEUS_PROTON_HH
# define NUCLEUS_PROTON_HH

# include <elle/types.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>

# include <cryptography/fwd.hh>
# include <cryptography/Signature.hh>

# include <nucleus/proton/Location.hh>

// XXX[temporary: for cryptography]
using namespace infinit;


namespace nucleus
{
  namespace proton
  {

    ///
    /// this class represents a stamp issued by the oracle.
    ///
    /// a stamp states that a precise version of a mutable block---i.e the
    /// master---references another precise version of another mutable block---
    /// i.e the slave.
    ///
    /// although this construct may sound useless it actually contributes
    /// greatly in ensuring security. for example stamps are used for
    /// linking an Object with a Group so that every vassal updating the
    /// object would need to acquire such a stamp from the oracle and then
    /// prove that she is a member of the linked group.
    ///
    /// thanks to the very light mechanism, vassals cannot lie regarding
    /// their memberships or permissions.
    ///
    class Stamp:
      public elle::Printable
    {
      /*-------------.
      | Construction |
      `-------------*/
    public:
      Stamp();
      ~Stamp();

      //
      // methods
      //
      elle::Status      Create(const Location&,
                               const Location&);

      elle::Status      Seal(cryptography::PrivateKey const&);

      /* XXX
      elle::Status
      Validate(elle::Authority const& authority);
      */

      //
      // operators
      //
    public:
      elle::Boolean
      operator ==(Stamp const& other) const;
      ELLE_OPERATOR_NEQ(Stamp);
      ELLE_OPERATOR_ASSIGNMENT(Stamp); // XXX

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
      // serializable
      ELLE_SERIALIZE_FRIEND_FOR(Stamp);

      //
      // attributes
      //

      Location          master;
      Location          slave;
      ELLE_ATTRIBUTE(cryptography::Signature*, signature);
    };

  }
}

#include <nucleus/proton/Stamp.hxx>

#endif
