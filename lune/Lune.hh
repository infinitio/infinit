//
// ---------- header ----------------------------------------------------------
//
// project       lune
//
// license       infinit
//
// author        julien quintard   [fri apr 30 12:55:13 2010]
//

//
// ---------- includes --------------------------------------------------------
//

#include <elle/Elle.hh>

#ifndef LUNE_LUNE_HH
#define LUNE_LUNE_HH

///
/// this namespace contains everything necessary to manipulate information on
/// satellite nodes such as local keys, locally stored data and so on.
///
namespace lune
{

//
// ---------- classes ---------------------------------------------------------
//

  ///
  /// this class provides functionalities for manipulating the lune
  /// library.
  ///
  /// more specifically, this class contains all the path patterns
  /// related to the lune files.
  ///
  class Lune
  {
  public:
    //
    // static methods
    //
    static elle::Status         Initialize();
    static elle::Status         Clean();

    //
    // static attributes
    //
    static elle::Pattern        Home;

    static elle::Pattern        Authority;
    static elle::Pattern        Passport;

    static elle::Pattern        Configuration;

    static elle::Pattern        Identity;
    static elle::Pattern        Dictionary;

    static elle::Pattern        Users;

    struct                      User
    {
      static elle::Pattern      Root;
      static elle::Pattern      Identity;
      static elle::Pattern      Dictionary;
    };

    static elle::Pattern        Networks;

    struct                      Network
    {
      static elle::Pattern      Root;
      static elle::Pattern      Descriptor;
      static elle::Pattern      Set;
      static elle::Pattern      Phrase;
      static elle::Pattern      Log;

      struct                    Reserve
      {
        static elle::Pattern    Root;

        static elle::Pattern    ImmutableBlock;
        static elle::Pattern    MutableBlock;

        static elle::Pattern    History;
      };

      struct                    Shelter
      {
        static elle::Pattern    Root;

        static elle::Pattern    ImmutableBlock;
        static elle::Pattern    MutableBlock;

        static elle::Pattern    History;
      };
    };
  };

}

//
// ---------- includes --------------------------------------------------------
//

#include <lune/Authority.hh>
#include <lune/Configuration.hh>
#include <lune/Descriptor.hh>
#include <lune/Dictionary.hh>
#include <lune/Identity.hh>
#include <lune/Log.hh>
#include <lune/Passport.hh>
#include <lune/Phrase.hh>
#include <lune/Set.hh>

#endif
