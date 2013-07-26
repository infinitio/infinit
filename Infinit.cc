#include <elle/utility/Time.hh>

#include <cryptography/PublicKey.hh>

#include <Infinit.hh>
#include <version.hh>

//
// ---------- definitions -----------------------------------------------------
//


///
/// this constant contains the version of the current Infinit software.
///
const elle::Version Infinit::version(INFINIT_VERSION_MAJOR,
                                     INFINIT_VERSION_MINOR);

///
/// this constant contains the copyright string.
///
const elle::String              Infinit::Copyright(
                                  "Infinit "
                                  INFINIT_VERSION
                                  " "
                                  "Copyright (c) 2013 "
                                  "infinit.io All rights reserved.");

// XXX[temporary: for cryptography]
using namespace infinit;

///
/// this variable contains the system configuration
///
lune::Configuration             Infinit::Configuration;

///
/// this variable contains the program's parser.
///
elle::utility::Parser*                   Infinit::Parser;

///
/// this variable holds the user name.
///
elle::String                    Infinit::User;

///
/// this variable holds the network name.
///
elle::String                    Infinit::Network;

///
/// this variable holds the mountpoint.
///
elle::String                    Infinit::Mountpoint;

//
// ---------- methods ---------------------------------------------------------
//

///
/// this method initializes Infinit.
///
elle::Status            Infinit::Initialize()
{
  // if the configuration file exists...
  if (lune::Configuration::exists(Infinit::User) == true)
    Infinit::Configuration.load(Infinit::User);

  // pull the parameters.
  if (Infinit::Configuration.Pull() == elle::Status::Error)
    throw elle::Exception("unable to pull the configuration parameters");

  return elle::Status::Ok;
}

///
/// this method cleans Infinit.
///
elle::Status            Infinit::Clean()
{
  // nothing to do.

  return elle::Status::Ok;
}
