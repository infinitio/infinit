#ifndef INFINIT_HH
#define INFINIT_HH

#include <elle/Version.hh>
#include <elle/types.hh>
#include <elle/utility/fwd.hh>

#include <hole/Authority.hh>
#include <lune/Configuration.hh>

class Infinit
{
public:
  //
  // constants
  //
  static elle::String const Key;
  static const elle::Version version;
  static const elle::String             Copyright;

  //
  // globals
  //
  static elle::Authority authority();
  static lune::Configuration            Configuration;

  static elle::utility::Parser*         Parser;

  static elle::String                   User;
  static elle::String                   Network;
  static elle::String                   Mountpoint;

  //
  // methods
  //
  static elle::Status           Initialize();
  static elle::Status           Clean();
};

#endif
