#ifndef NUCLEUS_PROTON_SEAM_HXX
# define NUCLEUS_PROTON_SEAM_HXX

# include <nucleus/Exception.hh>
# include <nucleus/proton/Ambit.hh>
# include <nucleus/proton/Nest.hh>
# include <nucleus/proton/Limits.hh>
# include <nucleus/proton/Statistics.hh>
# include <nucleus/proton/Contents.hh>

# include <elle/serialize/footprint.hh>

# include <elle/assert.hh>
# include <elle/log.hh>
# include <elle/finally.hh>

namespace nucleus
{
  namespace proton
  {
    /*----------.
    | Constants |
    `----------*/

    template <typename T>
    Nature const Seam<T>::Constants::nature = T::Constants::seam;

    /*-------------.
    | Construction |
    `-------------*/

    template <typename T>
    Seam<T>::Seam():
      Nodule<T>::Nodule(Nodule<T>::Type::seam)
    {
      static Footprint const initial = elle::serialize::footprint(*this);

      this->footprint(initial);
    }

    template <typename T>
    Seam<T>::~Seam()
    {
      auto iterator = this->_container.begin();
      auto end = this->_container.end();

      for (; iterator != end; ++iterator)
        delete iterator->second;

      this->_container.clear();
    }

//
// ---------- methods ---------------------------------------------------------
//

    template <typename T>
    void
    Seam<T>::insert(I* inlet)
    {
      std::pair<typename Seam<T>::Iterator::Forward,
                elle::Boolean> result;

      // check if this key has already been recorded.
      if (this->_container.find(inlet->key()) != this->_container.end())
        throw Exception(elle::sprintf("this key '%s' seems to have already been recorded",
                                      inlet->key()));

      // insert the inlet in the container.
      result = this->_container.insert(
                 std::pair<const typename T::K,
                           Seam<T>::I*>(inlet->key(), inlet));

      // check if the insertion was successful.
      if (result.second == false)
        throw Exception("unable to insert the inlet in the container");

      // set the state.
      this->state(State::dirty);

      // add the inlet footprint to the seam's.
      ELLE_ASSERT_NEQ(this->footprint(), 0u);
      ELLE_ASSERT_NEQ(inlet->footprint(), 0u);
      this->footprint(this->footprint() + inlet->footprint());
    }

    template <typename T>
    void
    Seam<T>::insert(typename T::K const& k,
                    Handle const& v)
    {
      // create an inlet.
      auto inlet =
        std::unique_ptr<typename Seam<T>::I>(
          new typename Seam<T>::I(k, v));

      // add the inlet to the seam.
      this->insert(inlet.get());

      // waive.
      inlet.release();
    }

    template <typename T>
    void
    Seam<T>::erase(typename Iterator::Forward& iterator)
    {
      Seam<T>::I* inlet;

      // retrieve the inlet.
      inlet = iterator->second;

      // set the state.
      this->state(State::dirty);

      // substract the inlet footprint to the seam's.
      ELLE_ASSERT_NEQ(this->footprint(), 0u);
      ELLE_ASSERT_NEQ(inlet->footprint(), 0u);
      this->footprint(this->footprint() - inlet->footprint());

      // delete the inlet.
      delete inlet;

      // finally, erase the entry.
      this->_container.erase(iterator);
    }

    template <typename T>
    void
    Seam<T>::erase(typename T::K const& k)
    {
      typename Seam<T>::Iterator::Forward iterator;

      // locate the inlet for the given key.
      iterator = this->locate_iterator(k);

      // delete the entry associated with the given iterator.
      this->erase(iterator);
    }

    template <typename T>
    void
    Seam<T>::refresh(typename Iterator::Forward& iterator,
                     typename T::K const& to)
    {
      Seam<T>::I* inlet;

      // retrieve the inlet.
      inlet = iterator->second;

      std::pair<typename Seam<T>::Iterator::Forward,
                elle::Boolean> result;

      // manually erase the entry.
      this->_container.erase(iterator);

      // update the inlet's key.
      inlet->key(to);

      // insert the inlet in the container.
      result = this->_container.insert(
        std::pair<const typename T::K,
                  Seam<T>::I*>(inlet->key(), inlet));

      // check if the insertion was successful.
      if (result.second == false)
        throw Exception("unable to insert the inlet in the container");

      // set the state.
      this->state(State::dirty);
    }

    template <typename T>
    void
    Seam<T>::refresh(typename T::K const& from,
                     typename T::K const& to)
    {
      typename Seam<T>::Iterator::Forward iterator;

      // locate the entry responsible for this key.
      iterator = this->locate_iterator(from);

      // update the seam.
      this->refresh(iterator, to);
    }

    template <typename T>
    elle::Boolean
    Seam<T>::exist(typename T::K const& k) const
    {
      return (this->_container.find(k) != this->_container.end());
    }

    /// XXX
    /// this method returns an iterator on the inlet responsible for the
    /// given key.
    ///
    /// note that contrary to Locate(), the Lookup() methods do not look
    /// for an exact match but instead return the inlet with the key
    /// immediately greater than the given key.
    ///
    template <typename T>
    typename Seam<T>::Scoutor::Forward
    Seam<T>::lookup_iterator(typename T::K const& k) const
    {
      auto end = this->_container.end();
      auto rbegin = this->_container.rbegin();
      typename Seam<T>::Scoutor::Forward scoutor;

      // go through the container.
      for (scoutor = this->_container.begin(); scoutor != end; ++scoutor)
        {
          Seam<T>::I* inlet = scoutor->second;

          // check if this inlet is responsible for the given key or
          // the end of the seam has been reached.
          if ((k <= scoutor->first) || (inlet == rbegin->second))
            return (scoutor);
        }

      throw Exception(elle::sprintf("unable to look up the entry responsible for "
                                    "the given key: '%s'", k));
    }

    template <typename T>
    typename Seam<T>::Iterator::Forward
    Seam<T>::lookup_iterator(typename T::K const& k)
    {
      typename Iterator::Forward iterator;
      auto end = this->_container.end();
      auto rbegin = this->_container.rbegin();

      // go through the container.
      for (iterator = this->_container.begin(); iterator != end; ++iterator)
        {
          Seam<T>::I* inlet = iterator->second;

          // check if this inlet is responsible for the given key or
          // the end of the seam has been reached.
          if ((k <= iterator->first) || (inlet == rbegin->second))
            return (iterator);
        }

      throw Exception(elle::sprintf("unable to look up the entry responsible "
                                    "for the given key: '%s'", k));
    }

    template <typename T>
    typename Seam<T>::I*
    Seam<T>::lookup_inlet(typename T::K const& k) const
    {
      auto scoutor = this->lookup_iterator(k);

      return (scoutor->second);
    }

    template <typename T>
    Handle
    Seam<T>::lookup_handle(typename T::K const& k) const
    {
      auto inlet = this->lookup_inlet(k);

      return (inlet->value());
    }

    template <typename T>
    typename Seam<T>::Scoutor::Forward
    Seam<T>::locate_iterator(typename T::K const& k) const
    {
      typename Seam<T>::Scoutor::Forward scoutor;

      // locate the given key.
      if ((scoutor = this->_container.find(k)) == this->_container.end())
        throw Exception(elle::sprintf("unable to locate the given key: '%s'", k));

      return (scoutor);
    }

    template <typename T>
    typename Seam<T>::Iterator::Forward
    Seam<T>::locate_iterator(typename T::K const& k)
    {
      typename Seam<T>::Iterator::Forward iterator;

      // locate the given key.
      if ((iterator = this->_container.find(k)) == this->_container.end())
        throw Exception(elle::sprintf("unable to locate the given key: '%s'", k));

      return (iterator);
    }

    template <typename T>
    typename Seam<T>::I*
    Seam<T>::locate_inlet(typename T::K const& k) const
    {
      typename Seam<T>::Scoutor::Forward scoutor;

      // locate the given key.
      scoutor = this->locate_iterator(k);

      return (scoutor->second);
    }

    template <typename T>
    Handle
    Seam<T>::locate_handle(typename T::K const& k) const
    {
      Seam<T>::I* inlet(this->locate_inlet(k));

      return (inlet->value());
    }

    /*-----.
    | Node |
    `-----*/

    template <typename T>
    elle::Boolean
    Seam<T>::eligible() const
    {
      for (auto const& pair: this->_container)
      {
        Seam<T>::I* inlet = pair.second;

        if (inlet->value().state() == State::dirty)
          return (false);
      }

      return (true);
    }

//
// ---------- nodule ----------------------------------------------------------
//

    template <typename T>
    void
    Seam<T>::add(typename T::K const& k,
                 Handle const& v)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      Seam<T>::I* inlet;

      ELLE_TRACE_SCOPE("add(%s, %s)", k, v);

      // look up the entry responsible for this key.
      auto iterator = this->lookup_iterator(k);

      // retrieve the inlet.
      inlet = iterator->second;

      Ambit<Nodule<T>> current(this->nest(), inlet->value());

      // load the current child nodule.
      current.load();

      // add the key/value tuple recursively.
      current().add(k, v);

      // Make sure that child nodule is dirty since a key/value tuple
      // has been added. Also, update the nodule block's state.
      ELLE_ASSERT_EQ(current().state(), State::dirty);
      current.contents().state(current().state());

      // update the inlet's and seam's state.
      this->state(State::dirty);

      //
      // update the key.
      //
      {
        // update the seam if necessary.
        if (inlet->key() != current().mayor())
          this->refresh(iterator, current().mayor());
      }

      //
      // update the capacity.
      //
      {
        // make sure the operation is valid.
        ELLE_ASSERT_LTE(inlet->capacity(), current().capacity());

        // compute the capacity variation.
        Capacity variation = current().capacity() - inlet->capacity();

        // update the inlet's and seam's capacity by adding the difference.
        inlet->capacity(inlet->capacity() + variation);
        this->capacity(this->capacity() + variation);
      }

      // unload the current nodule.
      current.unload();

      // now, let us try to optimise the tree given the fact that its
      // content has been altered.
      Nodule<T>::optimize(*this, inlet->key());
    }

    template <typename T>
    void
    Seam<T>::remove(typename T::K const& k)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      Seam<T>::I* inlet;

      ELLE_TRACE_SCOPE("remove(%s)", k);

      // look up the entry responsible for this key.
      auto iterator = this->lookup_iterator(k);

      // retrieve the inlet.
      inlet = iterator->second;

      Ambit<Nodule<T>> current(this->nest(), inlet->value());

      // load the current child nodule.
      current.load();

      // remove the key entry recursively.
      current().remove(k);

      // Make sure that child nodule is dirty since a key/value tuple
      // has been removed. Also, update the nodule block's state.
      ELLE_ASSERT_EQ(current().state(), State::dirty);
      current.contents().state(current().state());

      // update the inlet's and seam's state.
      this->state(State::dirty);

      //
      // update the key, if possible
      //
      if (current().empty() == false)
        {
          // update the current seam so as to reference the new
          // mayor key, if necessary.
          if (inlet->key() != current().mayor())
            this->refresh(iterator, current().mayor());
        }

      //
      // update the capacity
      //
      {
        // make sure the operation is valid.
        ELLE_ASSERT_GTE(inlet->capacity(), current().capacity());

        // compute the capacity variation.
        Capacity variation = inlet->capacity() - current().capacity();

        // update the inlet's and seam's capacity by substracting
        // the difference.
        inlet->capacity(inlet->capacity() - variation);
        this->capacity(this->capacity() - variation);
      }

      // unload the current nodule.
      current.unload();

      // now, let us try to optimise the tree given the fact that its
      // content has been altered.
      Nodule<T>::optimize(*this, inlet->key());
    }

    template <typename T>
    void
    Seam<T>::update(typename T::K const& k)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      Seam<T>::I* inlet;

      ELLE_TRACE_SCOPE("update(%s)", k);

      // look up the entry responsible for this key.
      auto iterator = this->lookup_iterator(k);

      // retrieve the inlet.
      inlet = iterator->second;

      Ambit<Nodule<T>> current(this->nest(), inlet->value());

      // load the current child nodule.
      current.load();

      // update the nodule recursively.
      current().update(k);

      // Make sure that child nodule is dirty since a key/value tuple
      // has been updated. Also update the block containing the node so
      // as to match the node's state.
      ELLE_ASSERT_EQ(current().state(), State::dirty);
      current.contents().state(current().state());

      // update the seam's state.
      this->state(State::dirty);

      //
      // update the key, if possible
      //
      if (current().empty() == false)
        {
          // update the seam if necessary.
          if (inlet->key() != current().mayor())
            this->refresh(iterator, current().mayor());
        }

      //
      // update the capacity.
      //
      {
        Capacity variation;

        // depending on the variation.
        if (current().capacity() > inlet->capacity())
          {
            // compute the capacity variation.
            variation = current().capacity() - inlet->capacity();

            // update the both the inlet and quill capacities.
            inlet->capacity(inlet->capacity() + variation);
            this->capacity(this->capacity() + variation);
          }
        else
          {
            // compute the capacity variation.
            variation = inlet->capacity() - current().capacity();

            // make sure the operation is valid.
            ELLE_ASSERT_LTE(variation, inlet->capacity());

            // update the both the inlet and quill capacities.
            inlet->capacity(inlet->capacity() - variation);
            this->capacity(this->capacity() - variation);
          }
      }

      // unload the current nodule.
      current.unload();

      // now, let us try to optimise the tree given the fact that its
      // content has been altered.
      Nodule<T>::optimize(*this, inlet->key());
    }

    template <typename T>
    Handle
    Seam<T>::split()
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");
      ELLE_TRACE_METHOD("");

      // Allocate a new seam.
      Contents* contents =
        new Contents{this->nest().network(),
                     this->nest().agent_K(),
                     new Seam<T>};
      Handle orphan{this->nest().attach(contents)};
      Ambit<Seam<T>> newright{this->nest(), orphan};

      // Load the new right nodule.
      newright.load();

      // Export the inlets from the current seam into the new seam.
      Nodule<T>::transfer_right(*this,
                                newright(),
                                this->nest().limits().extent() *
                                this->nest().limits().contention());

      // Set both seams' state as dirty.
      this->state(State::dirty);
      newright().state(State::dirty);

      // Unload the new right nodule.
      newright.unload();

      return (orphan);
    }

    template <typename T>
    void
    Seam<T>::merge(Handle& other)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      Ambit<Seam<T>> seam(this->nest(), other);

      ELLE_TRACE_SCOPE("merge(%s)", other);

      // load the given seam.
      seam.load();

      // check which nodule has the lowest keys.
      if (seam().mayor() < this->mayor())
        {
          // in this case, export the lower seam's inlets into the current's.
          Nodule<T>::transfer_right(seam(), *this, 0);
        }
      else
        {
          // otherwise, import the higher seam's inlets into the current's.
          Nodule<T>::transfer_left(*this, seam(), 0);
        }

      // set both seams' state as dirty.
      this->state(State::dirty);
      seam().state(State::dirty);

      // unload the given seam.
      seam.unload();
    }

    template <typename T>
    elle::Boolean
    Seam<T>::empty() const
    {
      return (this->_container.empty());
    }

    template <typename T>
    elle::Boolean
    Seam<T>::single() const
    {
      return (this->_container.size() == 1);
    }

    /// XXX
    /// this method returns the quill responsible for the given key.
    ///
    /// since the current nodule is not a quill, the request is forwarded
    /// to the child nodule which is responsible for the given key until
    /// the last level---i.e the quill---is reached.
    ///
    template <typename T>
    Handle
    Seam<T>::search(typename T::K const& k)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      Seam<T>::I* inlet;
      Handle v;

      ELLE_TRACE_SCOPE("search(%s)", k);

      // look up the entry responsible for this key.
      auto iterator = this->lookup_iterator(k);

      // retrieve the inlet.
      inlet = iterator->second;

      Ambit<Nodule<T>> current(this->nest(), inlet->value());

      // load the current nodule.
      current.load();

      // Is the child nodule a quill?
      if (current().type() == Nodule<T>::Type::quill)
        {
          current.unload();

          return (inlet->value());
        }
      else
        {
          // search in this nodule.
          Handle v{current().search(k)};

          // unload the current nodule.
          current.unload();

          return (v);
        }

      elle::unreachable();
    }

    template <typename T>
    Handle
    Seam<T>::find(typename T::K const& k,
                  Capacity& base)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      auto end = this->_container.end();
      auto iterator = this->_container.begin();
      auto rbegin = this->_container.rbegin();

      ELLE_TRACE_SCOPE("find(%s)", k);

      // go through the container.
      for (; iterator != end; ++iterator)
        {
          Seam<T>::I* inlet = iterator->second;

          // check if this inlet is responsible for the given key or
          // the end of the quill has been reached.
          if ((k <= inlet->key()) || (inlet == rbegin->second))
            {
              //
              // load the child nodule and explore it.
              //
              Ambit<Nodule<T>> current(this->nest(), inlet->value());

              // load the current nodule.
              current.load();

              // find the key in this nodule.
              Handle v{current().find(k, base)};

              // unload the current nodule.
              current.unload();

              return (v);
            }

          // increases the base.
          base += inlet->capacity();
        }

      elle::unreachable();
    }

    template <typename T>
    Handle
    Seam<T>::seek(Capacity const target,
                  Capacity& base)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      auto end = this->_container.end();
      auto iterator = this->_container.begin();

      ELLE_TRACE_SCOPE("seek(%s)", target);

      // go through the container.
      for (; iterator != end; ++iterator)
        {
          Seam<T>::I* inlet = iterator->second;

          // check whether the target lies in this child nodule.
          if (target < (base + inlet->capacity()))
            {
              //
              // load the child nodule and explore it.
              //
              Ambit<Nodule<T>> current(this->nest(), inlet->value());

              // load the current nodule.
              current.load();

              // seek the target in this nodule.
              Handle v{current().seek(target, base)};

              // unload the current nodule.
              current.unload();

              return (v);
            }

          // increases the base.
          base += inlet->capacity();
        }

      elle::unreachable();
    }

    template <typename T>
    void
    Seam<T>::check(Flags const flags)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      auto scoutor = this->_container.begin();
      auto end = this->_container.end();
      Capacity capacity(0);

      ELLE_TRACE_SCOPE("check(%s)", flags);

      // go through the inlets.
      for (; scoutor != end; ++scoutor)
        {
          Seam<T>::I* inlet = scoutor->second;
          Ambit<Nodule<T>> current(this->nest(), inlet->value());

          ELLE_DEBUG_SCOPE("checking inlet %s", inlet);

          // load the block.
          current.load();

          // check the address, if required.
          if (flags & flags::address)
            {
              ELLE_DEBUG_SCOPE("checking addresses");

              // bind the current block.
              Address address{current.contents().bind()};

              // compare the addresses.
              if (inlet->value().address() != address)
                throw Exception(elle::sprintf("invalid address: inlet(%s) versus bind(%s)",
                                              inlet->value().address(), address));
            }

          // check the keys, if required.
          if (flags & flags::key)
            {
              ELLE_DEBUG_SCOPE("checking keys");

              // check the key.
              if (inlet->key() != scoutor->first)
                throw Exception(elle::sprintf("invalid key: inlet(%s) versus container(%s)",
                                              inlet->key(), scoutor->first));

              // compare the mayor key with the inlet's reference.
              if (inlet->key() != current().mayor())
                throw Exception(elle::sprintf("the current nodule's mayor key differs from"
                                              "its reference: inlet(%s) versus nodule(%s)",
                                              inlet->key(), current().mayor()));
            }

          // trigger the check on the current nodule.
          if (flags & flags::recursive)
            current().check(flags);

          // check the capacities, if required.
          if (flags & flags::capacity)
            {
              ELLE_DEBUG_SCOPE("checking capacities");

              if (inlet->capacity() != current().capacity())
                throw Exception(elle::sprintf("invalid inlet capacity: inlet(%s) "
                                              "versus nodule(%s)",
                                              inlet->capacity(), current().capacity()));

              capacity += inlet->capacity();
            }

          // check the footprint.
          if (flags & flags::footprint)
            {
              ELLE_DEBUG_SCOPE("checking footprints");

              if (current().footprint() == 0)
                throw Exception("the footprint is null");

              if (current().footprint() !=
                  elle::serialize::footprint(current()))
                throw Exception(elle::sprintf("the recorded footprint does not match the "
                                              "instance's: nodule(%s) versus footprint(%s)",
                                              current().footprint(),
                                              elle::serialize::footprint(current())));

              if (current().footprint() > this->nest().limits().extent())
                throw Exception(elle::sprintf("the footprint '%s' exceeds the extent '%s",
                                              current().footprint(),
                                              this->nest().limits().extent()));
            }

          // check the state.
          if (flags & flags::state)
            {
              ELLE_DEBUG_SCOPE("checking states");

              if (current.contents().state() != current().state())
                throw Exception(elle::sprintf("invalid state: block(%s) versus nodule(%s)",
                                              current.contents().state(), current().state()));

              switch (this->state())
                {
                case State::clean:
                  {
                    if (current().state() != State::clean)
                      throw Exception(elle::sprintf("the nodule's state '%s' should "
                                                    "be clean", current().state()));

                    break;
                  }
                case State::dirty:
                  break;
                case State::consistent:
                  {
                    if ((current().state() != State::clean) &&
                        (current().state() != State::consistent))
                      throw Exception(elle::sprintf("the nodule's state '%s' should "
                                                    "be either clean or consistent",
                                                    current().state()));

                    break;
                  }
                }
            }

          // unload the block.
          current.unload();
        }

      // compare the seam capacity.
      if (flags & flags::capacity)
        {
          ELLE_DEBUG_SCOPE("checking capacities");

          if (this->capacity() != capacity)
            throw Exception(elle::sprintf("invalid capacity: this(%s) versus inlets(%s)",
                                          this->capacity(), capacity));
        }
    }

    ///
    /// XXX
    ///
    template <typename T>
    void
    Seam<T>::seal(cryptography::SecretKey const& secret)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");
      ELLE_TRACE_METHOD(secret);

      auto iterator = this->_container.begin();
      auto end = this->_container.end();

      // go through the container.
      for (; iterator != end; ++iterator)
        {
          Seam<T>::I* inlet = iterator->second;

          Ambit<Nodule<T>> current(this->nest(), inlet->value());

          // load the block.
          current.load(); // XXX load only if dirty i.e inlet->value().state()

          // ignore nodules which have not been created or modified
          // i.e is not dirty.
          switch (current().state())
            {
            case State::clean:
              {
                ELLE_DEBUG_SCOPE("State::clean");

                break;
              }
            case State::dirty:
              {
                ELLE_DEBUG_SCOPE("State::dirty");

                // seal recursively.
                current().seal(secret);

                // Encrypt and bind the block.
                current.contents().encrypt(secret);
                Address address{current.contents().bind()};

                current().state(State::consistent);
                current.contents().state(State::consistent);

                // Reset the inlet's value with the new address and secret.
                inlet->value().reset(address, secret);

                // set the current seam as dirty.
                this->state(State::dirty);

                break;
              }
            case State::consistent:
              {
                ELLE_DEBUG_SCOPE("State::consistent");

                throw Exception(elle::sprintf("unexpected state '%s'",
                                              current().state()));
              }
              // XXX unknown state[same for all switches]
            }

          // unload the block.
          current.unload();
        }
    }

    template <typename T>
    void
    Seam<T>::destroy()
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");
      ELLE_TRACE_METHOD("");

      for (auto& pair: this->_container)
        {
          auto& inlet = pair.second;
          Ambit<Nodule<T>> current(this->nest(), inlet->value());

          current.load();

          // Destroy recursively.
          current().destroy();

          // Detach the child nodule block from the nest.
          this->nest().detach(current.handle());

          current.unload();
        }

      // Update the state.
      this->state(State::dirty);
    }

    template <typename T>
    void
    Seam<T>::dump(elle::Natural32 const margin)
    {
      elle::String alignment(margin, ' ');
      auto iterator = this->_container.begin();
      auto end = this->_container.end();

      std::cout << alignment << "[Seam] " << this << std::endl;

      // dump the parent nodule.
      if (Nodule<T>::Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the parent nodule");

      // dump the inlets.
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Inlets] " << this->_container.size() << std::endl;

      // go through the inlets.
      for (; iterator != end; ++iterator)
        {
          Seam<T>::I* inlet = iterator->second;

          // dump the inlet.
          if (inlet->Dump(margin + 4) == elle::Status::Error)
            throw Exception("unable to dump the inlet");

          Ambit<Nodule<T>> current(this->nest(), inlet->value());

          // load the block.
          current.load();

          // walk through the nodule.
          current().dump(margin + 6);

          // unload the block.
          current.unload();
        }
    }

    template <typename T>
    void
    Seam<T>::statistics(Statistics& stats)
    {
      ELLE_LOG_COMPONENT("infinit.nucleus.proton.Seam");

      auto scoutor = this->_container.begin();
      auto end = this->_container.end();

      ELLE_TRACE_SCOPE("statistics()");

      for (; scoutor != end; ++scoutor)
        {
          Seam<T>::I* inlet = scoutor->second;
          Ambit<Nodule<T>> current(this->nest(), inlet->value());

          current.load();

          current().statistics(stats);

          stats.blocks_all(stats.blocks_all() + 1);

          switch (current().state())
            {
            case State::clean:
              {
                stats.blocks_clean(stats.blocks_clean() + 1);

                break;
              }
            case State::dirty:
              {
                stats.blocks_dirty(stats.blocks_dirty() + 1);

                break;
              }
            case State::consistent:
              {
                stats.blocks_consistent(stats.blocks_consistent() + 1);

                break;
              }
            }

          Footprint footprint = elle::serialize::footprint(current());

          stats.footprint_minimum(
            footprint < stats.footprint_minimum() ?
            footprint : stats.footprint_minimum());
          stats.footprint_average(
            (stats.footprint_average() + footprint) / 2);
          stats.footprint_maximum(
            footprint > stats.footprint_maximum() ?
            footprint : stats.footprint_maximum());

          current.unload();
        }
    }

    template <typename T>
    typename T::K const&
    Seam<T>::mayor() const
    {
      ELLE_ASSERT_EQ(this->_container.empty(), false);

      return (this->_container.rbegin()->first);
    }

    template <typename T>
    typename T::K const&
    Seam<T>::maiden() const
    {
      ELLE_ASSERT_EQ(this->_container.size(), 1u);

      return (this->_container.begin()->first);
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps the seam.
    ///
    template <typename T>
    elle::Status
    Seam<T>::Dump(const elle::Natural32 margin) const
    {
      elle::String alignment(margin, ' ');
      auto iterator = this->_container.begin();
      auto end = this->_container.end();

      std::cout << alignment << "[Seam] " << this << std::endl;

      // dump the parent nodule.
      if (Nodule<T>::Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the parent nodule");

      // dump the inlets.
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Inlets] " << this->_container.size() << std::endl;

      // go through the container.
      for (; iterator != end; ++iterator)
        {
          // dump the inlet.
          if (iterator->second->Dump(margin + 4) == elle::Status::Error)
            throw Exception("unable to dump the inlet");
        }

      return elle::Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    template <typename T>
    void
    Seam<T>::print(std::ostream& stream) const
    {
      stream << "seam(#" << this->_container.size() << ")";
    }
  }
}

//
// ---------- serialize -------------------------------------------------------
//

# include <elle/serialize/Serializer.hh>

ELLE_SERIALIZE_SPLIT_T1(nucleus::proton::Seam);

ELLE_SERIALIZE_SPLIT_T1_LOAD(nucleus::proton::Seam,
                             archive,
                             value,
                             version)
{
  elle::Natural32 size;
  elle::Natural32 i;

  enforce(version == 0);

  archive >> base_class<nucleus::proton::Nodule<T1>>(value);

  archive >> size;

  for (i = 0; i< size; i++)
    {
      auto inlet =
        archive.template Construct<
          typename nucleus::proton::Seam<T1>::I>();

      // Compute the inlet's footprint because the inlet proper
      // constructor has not been called. Instead, the default
      // constructor has been used before deserializing the inlet.
      inlet->footprint(elle::serialize::footprint(*inlet));

      std::pair<typename nucleus::proton::Seam<T1>::Iterator::Forward,
                elle::Boolean> result;

      if (value._container.find(inlet->key()) != value._container.end())
        throw nucleus::Exception(elle::sprintf("this key '%s' seems to have already "
                                               "been recorded", inlet->key()));

      result =
        value._container.insert(
          std::pair<const typename T1::K,
                    typename nucleus::proton::Seam<T1>::I*>(
                      inlet->key(), inlet.get()));

      if (result.second == false)
        throw nucleus::Exception("unable to insert the inlet in the container");

      // Update the seam's footprint.
      value.footprint(value.footprint() + inlet->footprint());

      inlet.release();
    }
}

ELLE_SERIALIZE_SPLIT_T1_SAVE(nucleus::proton::Seam,
                             archive,
                             value,
                             version)
{
  enforce(version == 0);

  archive << base_class<nucleus::proton::Nodule<T1>>(value);

  archive << static_cast<elle::Natural32>(value._container.size());

  auto it = value._container.begin();
  auto end = value._container.end();

  for (; it != end; ++it)
    archive << *(it->second);
}

#endif
