#ifndef ELLE_IO_DIRECTORY_HH
# define ELLE_IO_DIRECTORY_HH

# include <elle/types.hh>

# include <elle/io/fwd.hh>

# include <list>

namespace elle
{
  namespace io
  {

    ///
    /// this class abstracts the local directory operations.
    ///
    class Directory
    {
    public:
      //
      // types
      //
      typedef std::list<String>                 Set;
      typedef Set::iterator                     Iterator;
      typedef Set::const_iterator               Scoutor;

      //
      // static methods
      //
      static Status     Create(const Path&);
      static Status     Remove(const Path&);
      static Boolean    Exist(const Path&);
      static Status     Dig(const Path&);
      static Status     Clear(const Path&);
      static Status     List(const Path&,
                             Set&);
    };

  }
}

#endif
