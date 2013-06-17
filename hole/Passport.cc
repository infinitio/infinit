#include <elle/io/File.hh>
#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/PrivateKey.hh>

#include <hole/Passport.hh>

namespace elle
{

  /*-------------.
  | Construction |
  `-------------*/

  Passport::Passport() {} //XXX to remove

  Passport::Passport(elle::String const& id,
                     elle::String const& name,
                     cryptography::PublicKey const& owner_K,
                     cryptography::PrivateKey const& authority)
    : _id{id}
    , _name{name}
    , _owner_K{owner_K}
    , _signature{authority.sign(elle::serialize::make_tuple(id, owner_K))}
  {
    ELLE_ASSERT(id.size() > 0);
    ELLE_ASSERT(name.size() > 0);
  }


  ///
  /// this method verifies the validity of the passport.
  ///
  bool
  Passport::validate(cryptography::PublicKey const& authority) const
  {
    return (authority.verify(this->_signature,
                             elle::serialize::make_tuple(_id, _owner_K)));
  }

//
// ---------- dumpable --------------------------------------------------------
//

  ///
  /// this method dumps a passport.
  ///
  void
  Passport::dump(const elle::Natural32    margin) const
  {
    elle::String        alignment(margin, ' ');

    std::cout << alignment << "[Passport]" << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift << "[Id] "
              << this->_id << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift << "[Name] "
              << this->_name << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Owner K] " << this->_owner_K << std::endl;

    std::cout << alignment << elle::io::Dumpable::Shift
              << "[Signature] " << this->_signature << std::endl;
  }

  void
  Passport::print(std::ostream& stream) const
  {
    stream << "Passport(";
    std::string u;
    this->Save(u);
    if (u.size() < 16)
      stream << u;
    else
    {
      stream << u.substr(0, 8);
      stream << "...";
      stream << u.substr(u.size() - 8, std::string::npos);
    }
    stream << ")";
  }

  bool
  Passport::operator ==(Passport const& passport) const
  {
    elle::io::Unique theirs;
    elle::io::Unique ours;

    this->Save(ours);
    passport.Save(theirs);
    return ours == theirs;
  }
}
