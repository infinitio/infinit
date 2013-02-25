#ifndef ELLE_CONTAINER_TIMELINE_BUCKET_HH
# define ELLE_CONTAINER_TIMELINE_BUCKET_HH

# include <elle/types.hh>

# include <vector>

namespace elle
{
  namespace container
  {
    namespace timeline
    {

      ///
      /// this class represents a set of timeline objects grouped together
      /// because they have the exact same timestamp.
      ///
      /// note that a very simple data structure is being used in this
      /// class because no more than a dozen buckets should be grouped in
      /// the same bucket.
      ///
      template <typename T>
      class Bucket
      {
      public:
        //
        // types
        //
        typedef std::vector<T>                          Container;
        typedef typename Container::iterator            Iterator;
        typedef typename Container::const_iterator      Scoutor;

        //
        // methods
        //
        Status          Add(T const&);
        Status          Remove(T const&);

        //
        // interfaces
        //

        // dumpable
        Status          Dump(const Natural32 = 0) const;

        //
        // attributes
        //
        Container       container;
      };

    }
  }
}

#include <elle/container/timeline/Bucket.hxx>

#endif
