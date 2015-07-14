#ifdef INFINIT_WINDOWS
# include <cmath>
#endif
#include <boost/python.hpp>

#include <papier/Authority.hh>
#include <cryptography/rsa/PublicKey.hh>
#include <papier/Identity.hh>
#include <papier/Passport.hh>

#include <string>

using namespace boost::python;
typedef boost::python::return_value_policy<boost::python::return_by_value>
  by_value;

static
std::string
passport(std::string const& id,
         std::string const& name,
         std::string const& public_key,
         std::string const& authority_file,
         std::string const& authority_password)
{
  // Load the authority file.
  papier::Authority authority{elle::io::Path{authority_file}};

  // decrypt the authority.
  if (authority.Decrypt(authority_password) == elle::Status::Error)
    throw std::runtime_error("unable to decrypt the authority");

  infinit::cryptography::rsa::PublicKey pubkey{};
  pubkey.Restore(public_key);

  papier::Passport passport{id, name, pubkey, authority};

  std::string passport_string;
  if (passport.Save(passport_string) != elle::Status::Error)
    return passport_string;

  throw std::runtime_error("unabled to save the passport");
}

static
//std::tuple<std::string, std::string>
boost::python::tuple
identity(std::string const& id,
         std::string const& description,
         std::string const& password,
         std::string const& authority_file,
         std::string const& authority_password)
{
  infinit::cryptography::rsa::KeyPair pair =
    infinit::cryptography::rsa::keypair::generate(
      papier::Identity::keypair_length);

  elle::io::Path authority_path;
  papier::Identity identity;

  if (authority_path.Create(authority_file) == elle::Status::Error)
    throw std::runtime_error("unable to create authority path");

  // Load the authority file.
  papier::Authority authority(authority_path);

  // decrypt the authority.
  if (authority.Decrypt(authority_password) == elle::Status::Error)
    throw std::runtime_error("unable to decrypt the authority");

  // create the identity.
  if (identity.Create(id, description, pair) == elle::Status::Error)
    throw std::runtime_error("unable to create the identity");

  // encrypt the identity.
  if (identity.Encrypt(password) == elle::Status::Error)
    throw std::runtime_error("unable to encrypt the identity");

  // seal the identity.
  if (identity.Seal(authority) == elle::Status::Error)
    throw std::runtime_error("unable to seal the identity");

  std::string all, pub;
  if (identity.Save(all) != elle::Status::Error &&
      identity.pair().K().Save(pub) != elle::Status::Error)
    return boost::python::make_tuple(all, pub);

  throw std::runtime_error("unable to save public key and identity");
}

void export_passport();
void export_passport()
{
  def("generate_passport", &passport, by_value());
}

void export_identity();
void export_identity()
{
  def("generate_identity", &identity, by_value());
}

extern "C"
{
  PyObject* PyInit_papier();
}

BOOST_PYTHON_MODULE(papier)
{
  export_identity();
  export_passport();
}
