#ifndef ETOILE_GEAR_CONTEXT_HH
# define ETOILE_GEAR_CONTEXT_HH

# include <elle/types.hh>
# include <elle/io/Dumpable.hh>

# include <etoile/gear/Transcript.hh>
# include <etoile/gear/Nature.hh>
# include <etoile/gear/Operation.hh>
# include <etoile/Etoile.hh>

namespace etoile
{
  namespace gear
  {

    ///
    /// an context contains the information related to a set of blocks
    /// linked together.
    ///
    /// automata are then triggered on these contexts in order to perform
    /// modifications.
    ///
    /// every context has a state indicating its process stage plus an
    /// operation specifying its state once being closed, in other words,
    /// the final operations. thus, a context being opened, destroyed and
    /// modified by another actor would end up with a Destroy operation
    /// because the most important operation always superseed the others.
    ///
    class Context:
      public elle::io::Dumpable
    {
    public:
      //
      // enumerations
      //
      enum State
        {
          StateUnknown,

          StateCreated,
          StateLoaded,

          StateModified,

          StateDiscarded,
          StateStored,
          StateDestroyed,

          StateJournaled,
        };

      //
      // constructors & destructors
      //
      Context(Etoile& etoile,
              const Nature);
      ~Context();

      /*--------.
      | Methods |
      `--------*/
    public:
      // XXX
      Transcript const&
      transcript() const;
      // XXX
      Transcript&
      transcript();
      // XXX
      Transcript*
      cede();

      //
      // interfaces
      //

      // dumpable
      elle::Status      Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
    private:
      ELLE_ATTRIBUTE_X(Etoile&, etoile);
    public:
      Nature                    nature;

      State                     state;
      Operation                 operation;

    private: // XXX
      Transcript* _transcript;
    };

  }
}

#endif
