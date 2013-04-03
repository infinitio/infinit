#ifndef ETOILE_NEST_POD_HH
# define ETOILE_NEST_POD_HH

# include <reactor/rw-mutex.hh>

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>

# include <nucleus/proton/fwd.hh>

# include <boost/noncopyable.hpp>

# include <list>

namespace etoile
{
  namespace nest
  {
    /// Provide a overlay on top of an egg, especially by holding the
    /// state of the egg's block to know whether it has been detached
    /// from the nest or if it is still attached.
    class Pod:
      public elle::Printable,
      private boost::noncopyable
    {
      /*-------------.
      | Enumerations |
      `-------------*/
    public:
      /// Define whether the block is attached to the nest.
      enum class State
      {
        attached,
        detached
      };

      /*-------------.
      | Construction |
      `-------------*/
    public:
      /// Construct a pod from the given egg whose ownership is lost
      /// in favor of the pod.
      Pod(std::shared_ptr<nucleus::proton::Egg>& egg,
          std::list<Pod*>::iterator const& position);
      /// Likewise but through a rvalue.
      Pod(std::shared_ptr<nucleus::proton::Egg>&& egg,
          std::list<Pod*>::iterator const& position);

      /*----------.
      | Operators |
      `----------*/
    public:
      ELLE_OPERATOR_NO_ASSIGNMENT(Pod);

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
      ELLE_ATTRIBUTE_RW(State, state);
      /// The number of actors operating on the pod.
      ELLE_ATTRIBUTE_RW(elle::Natural32, actors);
      /// The egg containing the block and its information.
      ELLE_ATTRIBUTE_RX(std::shared_ptr<nucleus::proton::Egg>, egg);
      /// The position in the queue.
      // XXX[should not be here]
      ELLE_ATTRIBUTE_RW(std::list<Pod*>::iterator, position);
      /// A mutex so as to control whether moving the block, loading it or
      /// just accessing it does not impact the other.
      ELLE_ATTRIBUTE_RX(reactor::RWMutex, mutex);
      /// The footprint of the associated block. Note that this footprint
      /// is an approximation because the nest can keep track of it only
      /// when loaded/unloaded while its actual footprint may changed between
      /// these calls.
      ELLE_ATTRIBUTE_RW(nucleus::proton::Footprint, footprint);
    };

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Pod::State const state);
  }
}

#endif
