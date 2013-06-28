#ifndef LUNE_PHRASE_HH
# define LUNE_PHRASE_HH

# include <elle/types.hh>
# include <elle/operator.hh>
# include <elle/concept/Fileable.hh>
# include <elle/network/Port.hh>
# include <elle/io/Dumpable.hh>
# include <elle/serialize/construct.hh>

# include <boost/noncopyable.hpp>

namespace lune
{

  ///
  /// this class represents a phrase i.e a string enabling applications
  /// run by the user having launched the software to communicate with
  /// Infinit and thus trigger additional functionalities.
  ///
  /// noteworthy is that a phrase is made specific to both a user and
  /// a network so that a single user can launch Infini twice or more, even
  /// with a different identity, without overwritting the phrase.
  ///
  /// the portal attribute represents the name of the local socket to
  /// connect to in order to issue requests to Infinit.
  ///
  class Phrase:
    public elle::concept::MakeFileable<Phrase>,
    public elle::io::Dumpable,
    private boost::noncopyable
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    Phrase() {} // XXX to remove along with Create().
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Phrase);

  public:
    //
    // methods
    //
    elle::Status        Create(const elle::network::Port,
                               const elle::String&);

  private:
    /// XXX
    static
    elle::io::Path
    _path(elle::String const& user,
          elle::String const& network,
          elle::String const& name);

    //
    // interfaces
    //
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Phrase);

    elle::Boolean       operator==(const Phrase&) const;

    // dumpable
    elle::Status        Dump(const elle::Natural32 = 0) const;

    // fileable
    ELLE_CONCEPT_FILEABLE_METHODS();

    void
    load(elle::String const& user,
         elle::String const& network,
         elle::String const& name);
    void
    store(elle::String const& user,
          elle::String const& network,
          elle::String const& name) const;
    static
    void
    erase(elle::String const& user,
          elle::String const& network,
          elle::String const& name);
    static
    elle::Boolean
    exists(elle::String const& user,
           elle::String const& network,
           elle::String const& name);

    //
    // attributes
    //
    elle::network::Port port;
    elle::String pass;
  };

}

#include <lune/Phrase.hxx>

#endif
