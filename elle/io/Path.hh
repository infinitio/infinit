//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// file          /home/mycure/infinit/elle/io/Path.hh
//
// created       julien quintard   [mon apr 25 10:59:05 2011]
// updated       julien quintard   [sat sep  3 21:45:08 2011]
//

#ifndef ELLE_IO_PATH_HH
#define ELLE_IO_PATH_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/core/String.hh>

#include <elle/radix/Status.hh>
#include <elle/radix/Object.hh>

#include <elle/io/Pattern.hh>

namespace elle
{
  using namespace core;
  using namespace radix;

  namespace io
  {

//
// ---------- classes ---------------------------------------------------------
//

    ///
    /// this class abstracts a path representation.
    ///
    /// note that a path may be completed since its syntax considers
    /// the pattern %name% as representing a component to be provided later.
    ///
    class Path:
      public Object
    {
    public:
      //
      // methods
      //
      Status		Create(const String&);
      Status		Create(const Pattern&);

      template <typename T>
      Status		Complete(T);
      template <typename T,
		typename... TT>
      Status		Complete(T,
				TT...);
      Status		Complete(const String&,
				 const String&);

      //
      // interfaces
      //

      // object
      declare(Path);
      Boolean		operator==(const Path&) const;

      // dumpable
      Status		Dump(const Natural32 = 0) const;

      // archivable: nothing

      //
      // attributes
      //
      String		string;
    };

  }
}

//
// ---------- templates -------------------------------------------------------
//

#include <elle/io/Path.hxx>

#endif
