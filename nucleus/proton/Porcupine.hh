#ifndef NUCLEUS_PROTON_PORCUPINE_HH
# define NUCLEUS_PROTON_PORCUPINE_HH

# include <elle/types.hh>
# include <elle/attribute.hh>

# include <cryptography/SecretKey.hh>
# include <cryptography/PublicKey.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Flags.hh>
# include <nucleus/proton/Strategy.hh>
# include <nucleus/proton/Handle.hh>
# include <nucleus/proton/Tree.hh>
# include <nucleus/proton/Radix.hh>
# include <nucleus/proton/Door.hh>

# include <boost/noncopyable.hpp>
# include <boost/preprocessor/cat.hpp>

/*----------------.
| Macro-functions |
`----------------*/

/// XXX[the porcupine should not be modified
# define NUCLEUS_PROTON_PORCUPINE_FOREACH(_porcupine_, _door_)

namespace nucleus
{
  namespace proton
  {
    /// XXX
    template <typename T>
    class Porcupine:
      public elle::Printable,
      private boost::noncopyable
    {
      /*-------------.
      | Construction |
      `-------------*/
    public:
      /// Construct an empty porcupine.
      Porcupine(Nest& nest);
      /// XXX[explain]
      Porcupine(Radix const& radix,
                cryptography::SecretKey const& secret,
                Nest& nest);
      ~Porcupine();

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Return true if the porcupine contains no element.
      elle::Boolean
      empty() const;
      /// Return true of the given key is associated with a value.
      elle::Boolean
      exist(typename T::K const& k) const;
      /// Return the value associated with the given key _k_.
      Door<T>
      lookup(typename T::K const& k) const;
      /// XXX
      std::pair<Door<T>, Capacity>
      find(typename T::K const& k) const;
      /// Take the target index capacity and return the value responsible
      /// for it along with its base capacity index i.e the capacity index
      /// of the first element in the returned value.
      ///
      /// This method enables one to look for elements based on an index
      /// rather than a key, mechanism which is useful in many cases like
      /// for directories whose entries are often retrieved according to
      /// a range [index, size].
      std::pair<Door<T>, Capacity>
      seek(Capacity const target) const;
      /// Insert the given element E in the value responsible for the
      /// given key _k_.
      ///
      /// Note that, should this method be used, the type T must provide
      /// a method complying with the following prototype:
      ///
      ///   void
      ///   insert(typename T::K const& k,
      ///          E* e);
      template <typename E>
      void
      insert(typename T::K const& k,
             E* e);
      /// Erase the element corresponding to the given key _k_ from the
      /// value holding it.
      ///
      /// Note that, should this method be used, the type T must provide
      /// a method complying with the following prototype:
      ///
      ///   void
      ///   erase(typename T::K const& k);
      void
      erase(typename T::K const& k);
      /// Make sure the porcupine is consistent following the modification of
      /// the value responsible for the given key _k_.
      void
      update(typename T::K const& k);
      /// Return a door on the very first node.
      Door<T>
      head() const;
      /// Return a door on the last node composing the content.
      Door<T>
      tail() const;
      /// Return the number of elements being stored in the porcupine.
      elle::Size
      size() const;
      /// Check that the porcupine is valid according to some points given by
      /// _flags_ such that the internal capacity corresponds to the actual
      /// number of elements being stored, that the block addresses are correct,
      /// that the major keys are indeed the highest in their value and so on.
      ///
      /// This method is obviously provided for debugging purpose and should not
      /// be used in production considering the amount of computing such a check
      /// takes: all the blocks are retrieved from the storage layer and loaded
      /// in memory for checking.
      void
      check(Flags const flags = flags::all) const;
      /// Return statistics on the porcupine such as the number of blocks
      /// composing it, the average footprint, minimum/maximum capacity etc.
      Statistics
      statistics() const;
      /// Display a detailed state of the porcupine.
      void
      dump(elle::Natural32 const margin = 0) const;
      /// Return the radix of the porcupine, once encrypted and sealed.
      ///
      /// The radix could then be serialized or used for instantiate a
      /// porcupine. However, one should be aware of the fact that, depending
      /// on the strategy, a radix alone is useless. Given a block or tree
      /// strategy, the constituing blocks are encrypted and stored in
      /// the nest. Therefore, while the radix represent the meta descriptor
      /// of the content, the blocks are actually located in the nest.
      ///
      /// Note that once sealed, no modifying operating should be carried
      /// out on the porcupine.
      Radix
      seal(cryptography::SecretKey const& secret);
      /// Detach from the nest all the blocks consituing the content.
      ///
      /// This method is useful whenever one want to prepare the removal
      /// of all the content blocks.
      ///
      /// Note that this call is final, as for seal(), meaning that the
      /// porcupine cannot be used once destroyed.
      ///
      /// This method is an alternative to erasing all the elements from
      /// the tree, in which case all the blocks would also be destroyed.
      /// This method is however straightforward and therefore far more
      /// efficient.
      void
      destroy();
      /// Return the state of the porcupine.
      State
      state() const;
      /// Return the embedded value, should the strategy comply.
      ///
      /// !WARNING! Do not use unless one knows exactly what he/she is doing.
      T const&
      value() const;
      /// Return the embedded value, should the strategy comply.
      ///
      /// !WARNING! Do not use unless one knows exactly what he/she is doing.
      T&
      value();
      /// Return the value block's handle, should the strategy comply.
      ///
      /// !WARNING! Do not use unless one knows exactly what he/she is doing.
      Handle const&
      block() const;
      /// Return the tree, should the strategy comply.
      ///
      /// !WARNING! Do not use unless one knows exactly what he/she is doing.
      Tree<T> const&
      tree() const;
      /// Return the tree, should the strategy comply.
      ///
      /// !WARNING! Do not use unless one knows exactly what he/she is doing.
      Tree<T>&
      tree();
    private:
      /// Transform an empty porcupine into a value-based porcupine so
      /// as to be able to return the caller a value on which to operate,
      /// for inserting or exploring for example.
      ///
      /// Note that this method is const because const-methods call it
      /// though only mutable attributes are being modified.
      void
      _create() const;
      /// Represent the key functionality of the porcupine abstraction. This
      /// method does one fundamental thing: it transforms content from one
      /// strategy to another e.g from a direct value to a block-based value
      /// or to a block to a tree.
      ///
      /// Such a decision is made depending on the limits associated with
      /// every strategy. For example, should the value block reach a footprint
      /// of 1024, it would be transformed into a tree. Likewise, should the
      /// value block reach a low limit of 256 bytes, it would be transformed
      /// into a direct value.
      ///
      /// This mechanism is crucial to transparently adapt the strategy of the
      /// content so as to be optimised according to some limits.
      void
      _optimize();

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // printable
      virtual
      void
      print(std::ostream& stream) const;

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      /// XXX
      ELLE_ATTRIBUTE_RP(Strategy, strategy, mutable);
      /// XXX[maybe use the ELLE_ATTRIBUTE_r etc.?]
      union
      {
        /// Represent the directly embedded value when in evolving in
        /// the strategy 'value'.
        mutable T* _value;
        /// When evolving in strategy 'block', this handle reference the
        /// block which actually holds the data.
        Handle* _handle;
        /// Represent the hierachical data structure used when evolving
        /// in strategy 'tree' so as to provide a flexible mechanism for
        /// handling large amount of data.
        Tree<T>* _tree;
      };
      /// XXX
      ELLE_ATTRIBUTE_P(elle::Boolean, emptied, mutable);
      ELLE_ATTRIBUTE_X(Nest&, nest);
    };
  }
}

# include <nucleus/proton/Porcupine.hxx>

#endif
