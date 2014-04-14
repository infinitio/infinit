#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/PrivateKey.hh>
#include <cryptography/oneway.hh>

#include <papier/Authority.hh>
#include <boost/functional/hash.hpp>
#include <papier/Passport.hh>

namespace papier
{
  /*-------------.
  | Construction |
  `-------------*/

  Passport::Passport() {} //XXX to remove

  Passport::Passport(elle::String const& id,
                     elle::String const& name,
                     cryptography::PublicKey const& owner_K,
                     papier::Authority const& authority)
    : _id{id}
    , _name{name}
    , _owner_K{owner_K}
    , _signature{authority.k().sign(elle::serialize::make_tuple(id, owner_K))}
  {
    ELLE_ASSERT(id.size() > 0);
    ELLE_ASSERT(name.size() > 0);
    ELLE_ASSERT(this->validate(authority));
  }


  ///
  /// this method verifies the validity of the passport.
  ///
  bool
  Passport::validate(papier::Authority const& authority) const
  {
    return (authority.K().verify(this->_signature,
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

  bool
  Passport::operator !=(Passport const& passport) const
  {
    return !(*this == passport);
  }
}

namespace std
{
  std::size_t
  hash<papier::Passport>::operator()(papier::Passport const& s) const
  {
    // XXX: The id is definitely not unique.
    auto id = std::hash<std::string>()(s.id());
    auto k = std::hash<elle::ConstWeakBuffer>()(
      infinit::cryptography::oneway::hash(
        s.owner_K(), infinit::cryptography::oneway::Algorithm::sha1).buffer());
    size_t seed = 0;
    boost::hash_combine(seed, id);
    boost::hash_combine(seed, k);
    return seed;
  }
}
