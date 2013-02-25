#ifndef ELLE_IO_PATTERN_HH
# define ELLE_IO_PATTERN_HH

# include <elle/types.hh>
# include <elle/operator.hh>

namespace elle
{
  namespace io
  {

    ///
    /// this class represents a path pattern.
    ///
    /// although the common syntax considers the string %name% as
    /// representing a variable component, programmers are welcome to use
    /// the syntax they prefer.
    ///
    class Pattern
    {
    public:
      //
      // constructors & destructors
      //
      Pattern();
      Pattern(elle::String const& string);
      Pattern(Pattern const& other) = default;

      //
      // methods
      //
      Status            Create(const String&);

      //
      // interfaces
      //

      ELLE_OPERATOR_NO_ASSIGNMENT(Pattern);

      Boolean           operator==(const Pattern&) const;

      // dumpable
      Status            Dump(const Natural32 = 0) const;

      //
      // attributes
      //
      String            string;
    };

  }
}

#endif
