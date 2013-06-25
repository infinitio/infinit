#include <satellites/satellite.hh>

#include <Infinit.hh>

#include <infinit/Identity.hh>
#include <infinit/Authority.hh>
#include <infinit/Certificate.hh>

#include <elle/io/Console.hh>
#include <elle/io/Path.hh>
#include <elle/io/Piece.hh>
#include <elle/utility/Parser.hh>

#include <cryptography/random.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

#include <hole/Hole.hh>
#include <hole/Passport.hh>

#include <lune/Lune.hh>

#include <common/common.hh>

#include <Program.hh>

#include <boost/filesystem.hpp>

#include <elle/serialize/insert.hh>
#include <elle/serialize/extract.hh>

namespace satellite
{
  void
  Passport(elle::Natural32 argc,
           elle::Character* argv[])
  {
    // initialize the Lune library.
    if (lune::Lune::Initialize() == elle::Status::Error)
      throw elle::Exception("unable to initialize Lune");

    // Example for Antony: infinit-passport-create
    {
      elle::String authority_path("XXX");
      elle::String authority_passphrase("XXX");

      elle::String identity_path("XXX");
      elle::String identity_passphrase("XXX");

      // By default we could use gethostname() to retrieve the device's name.
      elle::String description("XXX");

      elle::String passport_path("XXX");

      // ---

      infinit::Authority authority(elle::serialize::from_file(authority_path));
      // XXX validate
      cryptography::PrivateKey authority_k =
        authority.decrypt(authority_passphrase);

      infinit::Identity identity(elle::serialize::from_file(identity_path));
      // XXX validate
      cryptography::PrivateKey identity_k =
        identity.decrypt(identity_passphrase);

      hole::Passport passport(authority.K(),
                              identity.subject_K(),
                              description,
                              identity_k,
                              authority_k);

      elle::serialize::to_file(passport_path) << passport;
    }

    // clean Lune
    if (lune::Lune::Clean() == elle::Status::Error)
      throw elle::Exception("unable to clean Lune");
  }
}

int                     main(int                                argc,
                             char**                             argv)
{
  return satellite_main("8passport", [&] {
                          satellite::Passport(argc, argv);
                        });
}
