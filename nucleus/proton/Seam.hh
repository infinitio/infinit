#ifndef NUCLEUS_PROTON_SEAM_HH
# define NUCLEUS_PROTON_SEAM_HH

# include <elle/types.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Nodule.hh>
# include <nucleus/proton/Inlet.hh>
# include <nucleus/proton/Flags.hh>
# include <nucleus/proton/Porcupine.hh>

# include <map>

namespace nucleus
{
  namespace proton
  {

    ///
    /// this class represents an internal tree node. such a node contains
    /// a data structure containing inlets which reference sub-nodules.
    ///
    /// note that since the block size of the nodules can be configured,
    /// the internal data structure is hierachical in order to handle
    /// blocks with thousand entries.
    ///
    template <typename T>
    class Seam:
      public Nodule<T>,
      public elle::serialize::SerializableMixin<Seam<T>>,
      public elle::concept::UniquableMixin<Seam<T>>
    {
      friend class Nodule<T>;

      //
      // types
      //
    public:
      /// XXX
      typedef Nodule<T> V;
      /// XXX
      typedef Inlet<typename T::K> I;

      //
      // constants
      //
    public:
      struct Constants
      {
        static Nature const nature;
      };

      //
      // types
      //
    public:
      typedef std::map<typename T::K const, I*> Container;

      struct Iterator
      {
        typedef typename Container::iterator Forward;
        typedef typename Container::reverse_iterator Backward;
      };

      struct Scoutor
      {
        typedef typename Container::const_iterator Forward;
        typedef typename Container::const_reverse_iterator Backward;
      };

      //
      // construction
      //
    public:
      /// XXX
      Seam();
      virtual
      ~Seam();

      //
      // methods
      //
    public:
      /// XXX
      void
      insert(I* inlet);
      /// XXX
      void
      insert(typename T::K const& k,
             Handle const& v);
      /// XXX
      void
      erase(typename Iterator::Forward& iterator);
      /// XXX
      void
      erase(Handle const& handle);
      /// XXX
      void
      erase(typename T::K const& k);
      /// XXX
      void
      refresh(typename Iterator::Forward& iterator,
              typename T::K const& to);
      /// XXX
      void
      refresh(typename T::K const& from,
              typename T::K const& to);
      /// XXX
      elle::Boolean
      exist(typename T::K const& k) const;
      /// XXX
      typename Scoutor::Forward
      lookup_iterator(typename T::K const& k) const;
      /// XXX
      typename Iterator::Forward
      lookup_iterator(typename T::K const& k);
      /// XXX
      I*
      lookup_inlet(typename T::K const& k) const;
      /// XXX
      Handle
      lookup_handle(typename T::K const& k) const;
      /// XXX
      typename Scoutor::Forward
      locate_iterator(typename T::K const& k) const;
      /// XXX
      typename Iterator::Forward
      locate_iterator(const typename T::K& k);
      /// XXX
      I*
      locate_inlet(typename T::K const& k) const;
      /// XXX
      Handle
      locate_handle(typename T::K const& k) const;

      //
      // interfaces
      //
    public:
      // node
      virtual
      elle::Boolean
      eligible() const;
      // nodule
      void
      add(typename T::K const& k,
          Handle const& v);
      void
      remove(typename T::K const& k);
      void
      update(typename T::K const& k);
      Handle
      split();
      void
      merge(Handle& other);
      elle::Boolean
      empty() const;
      elle::Boolean
      single() const;
      Handle
      search(typename T::K const& k);
      Handle
      find(typename T::K const& k,
           Capacity& base);
      Handle
      seek(Capacity const target,
           Capacity& base);
      void
      check(Flags const flags = flags::all);
      void
      seal(cryptography::SecretKey const& secret);
      void
      destroy();
      void
      dump(elle::Natural32 const margin = 0);
      void
      statistics(Statistics& stats);
      typename T::K const&
      mayor() const;
      typename T::K const&
      maiden() const;
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // printable
      virtual
      void
      print(std::ostream& stream) const;
      // serialize
      ELLE_SERIALIZE_FRIEND_FOR(Seam<T>);

      ELLE_SERIALIZE_SERIALIZABLE_METHODS(Seam<T>);

      //
      // attributes
      //
    private:
      Container _container;
    };

  }
}

#include <nucleus/proton/Seam.hxx>

#endif
