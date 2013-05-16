#ifndef LUNE_LUNE_HH
# define LUNE_LUNE_HH

#include <elle/types.hh>
#include <elle/io/Pattern.hh>

///
/// this namespace contains everything necessary to manipulate information on
/// satellite nodes such as local keys, locally stored data and so on.
///
namespace lune
{

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
    static elle::io::Pattern Home;
    static elle::io::Pattern Authority;
    static elle::io::Pattern Users;
    static elle::io::Pattern User;
    static elle::io::Pattern Dictionary;
    static elle::io::Pattern Passport;
    static elle::io::Pattern Configuration;
    static elle::io::Pattern Networks;
    static elle::io::Pattern Network;
    static elle::io::Pattern Phrase;
    static elle::io::Pattern Set;
    static elle::io::Pattern Shelter;
  };

}

#endif
