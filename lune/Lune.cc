#include <elle/io/Path.hh>
#include <elle/io/Piece.hh>
#include <elle/system/system.hh>

#include <common/common.hh>

#include <hole/Passport.hh>

#include <lune/Lune.hh>
#include <lune/Dictionary.hh>
#include <lune/Set.hh>
#include <lune/Phrase.hh>

#include <nucleus/proton/History.hh>
#include <nucleus/proton/Block.hh>

#include <Infinit.hh>

namespace lune
{

//
// ---------- definitions -----------------------------------------------------
//

  elle::io::Pattern Lune::Home;
  elle::io::Pattern Lune::Users;
  elle::io::Pattern Lune::User;
  elle::io::Pattern Lune::Dictionary;
  elle::io::Pattern Lune::Passport;
  elle::io::Pattern Lune::Networks;
  elle::io::Pattern Lune::Network;
  elle::io::Pattern Lune::Phrase;
  elle::io::Pattern Lune::Set;
  elle::io::Pattern Lune::Shelter;

//
// ---------- static methods --------------------------------------------------
//

  ///
  /// this method initializes the system definitions.
  ///
  elle::Status          Lune::Initialize()
  {
    elle::String        home = common::infinit::home();

    //
    // create the paths.
    //
    {
      if (Lune::Home.Create(home) == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Users.Create(
            home +
            elle::system::path::separator +
            "users") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::User.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Dictionary.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%" +
            elle::system::path::separator +
            "%USER%.dic") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Passport.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%" +
            elle::system::path::separator +
            "%USER%.ppt") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Networks.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%" +
            elle::system::path::separator +
            "networks") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Network.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%" +
            elle::system::path::separator +
            "networks" +
            elle::system::path::separator +
            "%NETWORK%") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Phrase.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%" +
            elle::system::path::separator +
            "networks" +
            elle::system::path::separator +
            "%NETWORK%" +
            elle::system::path::separator +
            "%NAME%.phr") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Set.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%" +
            elle::system::path::separator +
            "networks" +
            elle::system::path::separator +
            "%NETWORK%" +
            elle::system::path::separator +
            "%NETWORK%.set") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");

      if (Lune::Shelter.Create(
            home +
            elle::system::path::separator +
            "users" +
            elle::system::path::separator +
            "%USER%" +
            elle::system::path::separator +
            "networks" +
            elle::system::path::separator +
            "%NETWORK%" +
            elle::system::path::separator +
            "shelter") == elle::Status::Error)
        throw elle::Exception("unable to create the pattern");
    }

    return elle::Status::Ok;
  }

  elle::Status          Lune::Clean()
  {
    // nothing to do.

    return elle::Status::Ok;
  }

}
