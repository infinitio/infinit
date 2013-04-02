#include <etoile/nest/Nest.hh>
#include <etoile/gear/Transcript.hh>
#include <etoile/gear/Action.hh>
#include <etoile/depot/Depot.hh>
#include <etoile/Exception.hh>

#include <elle/finally.hh>
#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <cryptography/SecretKey.hh>

#include <nucleus/proton/Nest.hh>
#include <nucleus/proton/Handle.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Revision.hh>
#include <nucleus/proton/Contents.hh>

ELLE_LOG_COMPONENT("infinit.etoile.nest.Nest");

namespace etoile
{
  namespace nest
  {
    /*-------------.
    | Construction |
    `-------------*/

    Nest::Nest(elle::Natural32 const secret_length,
               nucleus::proton::Limits const& limits,
               nucleus::proton::Network const& network,
               cryptography::PublicKey const& agent_K,
               nucleus::proton::Extent const threshold):
      nucleus::proton::Nest(limits, network, agent_K),

      _secret_length(secret_length),
      _threshold(threshold),
      _size(0)
    {
    }

    Nest::~Nest()
    {
      for (auto& pair: this->_pods)
        {
          auto pod = pair.second;

          delete pod;
        }

      this->_pods.clear();
      this->_addresses.clear();
      this->_history.clear();
    }

    /*--------.
    | Methods |
    `--------*/

    gear::Transcript
    Nest::transcribe()
    {
      ELLE_TRACE_METHOD("");

      gear::Transcript transcript;

      for (auto& pair: this->_pods)
        {
          auto pod = pair.second;

          ELLE_ASSERT(pod->actors() == 0);
          ELLE_ASSERT(pod->mutex().locked() == false);
          ELLE_ASSERT(pod->mutex().write().locked() == false);

          // XXX make sure the pod's egg is no longer locked or wait for it.
          // XXX since we're going to give away the block.
          // XXX et unlock plus bas (attention au continue).
          // XXX utiliser un finally ou un Lock comme dans wall
          // XXX plutot assert si on ne peut pas lock? quoi que une operation de
          //     pre-publish pourrait etre en train de se terminer

          // Act depending on the pod's state.
          switch (pod->state())
            {
            case Pod::State::attached:
              {
                // This block is still attached. However, ignore non-present
                // blocks as these have been either unloaded from main memory
                // because not modified or pre-published in order to lighten
                // the nest.
                if (pod->egg()->block() == nullptr)
                  continue;

                // XXX[check that the block is not in the timestamps queue as it
                //     would mean someone is using it!]

                // Note that only dirty blocks need to be published onto the
                // storage layer.
                switch (pod->egg()->block()->state())
                  {
                  case nucleus::proton::State::clean:
                    {
                      // Nothing to do for clean blocks.

                      break;
                    }
                  case nucleus::proton::State::dirty:
                    throw Exception("dirty blocks should have been "
                                          "sealed");
                  case nucleus::proton::State::consistent:
                    {
                      // The address of the block has been recomputed during
                      // the sealing process so that the egg embeds the new
                      // address.

                      // However, the previous temporary versions of the block
                      // must first be removed from the storage layer. Indeed,
                      // should the nest have decided to pre-publish the block
                      // on the storage layer, this temporary version would need
                      // to be removed.
                      for (auto& clef: pod->egg()->annals())
                        transcript.record(
                          new gear::action::Wipe(clef->address()));

                      ELLE_ASSERT(pod->egg()->block()->bind() ==
                                  pod->egg()->address());

                      // Finally, push the final version of the block.
                      transcript.record(
                        new gear::action::Push(pod->egg()->address(),
                                               std::move(pod->egg()->block())));

                      break;
                    }
                  default:
                    throw elle::Exception(
                      elle::sprintf("unknown block state '%s'",
                                    pod->egg()->block()->state()));
                  }

                break;
              }
            case Pod::State::detached:
              {
                // Take a decision based on the type of block i.e
                // transient or permanent.
                //
                // Note that detached transient blocks should never lie in
                // the nest since they could be deleted without impacting the
                // consistency as nothing references them anymore.
                switch (pod->egg()->type())
                  {
                  case nucleus::proton::Egg::Type::transient:
                    throw Exception("the pod referencing this transient "
                                          "detached block should have been "
                                          "deleted");
                  case nucleus::proton::Egg::Type::permanent:
                    {
                      ELLE_ASSERT(pod->egg()->block() == nullptr);

                      // The block has been detached and should therefore be
                      // removed from the storage layer.
                      transcript.record(new gear::action::Wipe(
                        pod->egg()->address()));

                      break;
                    }
                  default:
                    throw Exception(elle::sprintf("unknown egg type '%s'",
                                                        pod->egg()->type()));
                  }

                break;
              }
            default:
              throw Exception(elle::sprintf("unknown pod state '%s'",
                                                  pod->state()));
            }
        }

      return (transcript);
    }

    elle::Boolean
    Nest::_exist(nucleus::proton::Address const& address) const
    {
      return (this->_addresses.find(address) != this->_addresses.end());
    }

    void
    Nest::_insert(Pod* pod)
    {
      // Insert the pod in the container.
      auto result =
        this->_pods.insert(
          std::pair<nucleus::proton::Egg*, Pod*>(pod->egg().get(), pod));

      // Check if the insertion was successful.
      if (result.second == false)
        throw Exception("unable to insert the pod into the container");
    }

    void
    Nest::_map(nucleus::proton::Address const& address,
               Pod* pod)
    {
      // Insert a tuple address/pod in the container.
      if (this->_addresses.insert(
            std::pair<nucleus::proton::Address const, Pod*>(
              address, pod)).second == false)
        throw Exception("unable to insert the address/pod tuple");
    }

    Pod*
    Nest::_lookup(std::shared_ptr<nucleus::proton::Egg> const& egg) const
    {
      auto scoutor = this->_pods.find(egg.get());

      if (scoutor == this->_pods.end())
        throw Exception(elle::sprintf("unable to locate the pod "
                                            "associated with the "
                                            "given egg '%s'", *egg));

      auto pod = scoutor->second;

      return (pod);
    }

    Pod*
    Nest::_lookup(nucleus::proton::Address const& address) const
    {
      auto scoutor = this->_addresses.find(address);

      if (scoutor == this->_addresses.end())
        throw Exception(elle::sprintf("unable to locate the pod "
                                            "associated with the "
                                            "given address '%s'", address));

      auto pod = scoutor->second;

      return (pod);
    }

    void
    Nest::_unmap(nucleus::proton::Address const& address)
    {
      this->_addresses.erase(address);
    }

    void
    Nest::_optimize()
    {
      ELLE_DEBUG_FUNCTION("");

//       // XXX[100 / 20]
//       elle::Natural32 const limit_high = 0;
//       elle::Natural32 const limit_low = 0;

//       ELLE_DEBUG_FUNCTION("");

//       if (this->_placements.size() <= high_limit) // XXX
//         {
//           ELLE_DEBUG("the nest does not need to be optimized");
//           return;
//         }

//       ELLE_DEBUG("look for unused pod so as to lighten the nest");

//       elle::Boolean traversed = false;

//       while (traversed == false)
//         {
//           traversed = true;

//           for (auto& pair: this->_placements)
//             {
//               auto& placement = pair.first;
//               auto& pod = pair.second;

//               // Handle volatile blocks (i.e just created) which have
//               // been detached.
//               if ((pod->nature == Pod::NatureVolatile) &&
//                   (pod->link == Pod::LinkDetached) &&
//                   (pod->counter == 0))
//                 {
//                   this->_delete(placement);

//                   traversed = false;

//                   break;
//                 }

//               // Ignore pod which are still in use.
//               if (pod->state != Pod::StateUnloaded)
//                 continue;

//               ELLE_ASSERT(pod->counter == 0);

//               // Ignore pod whose block has been unloaded on the storage layer.
//               if (pod->block == nullptr)
//                 continue;

//               // Bind the block.
//               // XXX pod->address = pod->block->bind();

//               // XXX
//             }
//         }
    }

    void
    Nest::_load(Pod* pod)
    {
      ELLE_DEBUG_METHOD(pod);

      // First, let us lock the egg so that someone does not try to
      // use/change it while we are loading it.
      reactor::Lock lock(*reactor::Scheduler::scheduler(),
                         pod->mutex().write());

      // Nothing to do if the egg already holds the block.
      //
      // This check is necessary because we may have blocked for some time
      // by someone retrieving the block so that when we wake up, the block
      // is already here.
      if (pod->egg()->block() != nullptr)
        return;

      ELLE_ASSERT(pod->egg()->type() == nucleus::proton::Egg::Type::permanent);

      // Otherwise, load the block from the depot.
      auto contents =
        depot::Depot::pull<nucleus::proton::Contents>(
          pod->egg()->address(),
          nucleus::proton::Revision::Last);

      // Decrypt the contents with the egg's secret.
      contents->decrypt(pod->egg()->secret());

      // Sett the block in the egg.
      pod->egg()->block() = std::move(contents);

      // XXX[set the nest in the node]
      pod->egg()->block()->node().nest(*this);
    }

    void
    Nest::_queue(Pod* pod)
    {
      ELLE_DEBUG_METHOD(pod);

      ELLE_ASSERT(pod->position() == this->_history.end());

      // Insert the pod in the history, at the back.
      auto iterator = this->_history.insert(this->_history.end(), pod);

      // Update the pod's position for fast removal.
      pod->position(iterator);
      ELLE_ASSERT(pod->position() != this->_history.end());
    }

    void
    Nest::_unqueue(Pod* pod)
    {
      ELLE_DEBUG_METHOD(pod);

      ELLE_ASSERT(pod == *pod->position());

      // Remove the pod from the history.
      this->_history.erase(pod->position());

      // Reset the pod's position.
      pod->position(this->_history.end());
    }

    /*---------.
    | Dumpable |
    `---------*/

    elle::Status
    Nest::Dump(const elle::Natural32 margin) const
    {
      elle::String alignment(margin, ' ');

      std::cout << alignment << "[Nest] #" << this->_pods.size() << std::endl;

      for (auto& pair: this->_pods)
        {
          auto pod = pair.second;

          std::cout << alignment << elle::io::Dumpable::Shift
                    << *pod << std::endl;
        }

      return elle::Status::Ok;
    }

    /*-----.
    | Nest |
    `-----*/

    nucleus::proton::Handle
    Nest::attach(nucleus::proton::Contents* block)
    {
      ELLE_TRACE_METHOD(block);

      ELLE_FINALLY_ACTION_DELETE(block);

      // Compute a static temporary address which will be the same for every
      // block in the same nest since every such block belongs to the same
      // object hence within the same network.
      //
      // The idea behind this computation is to provide a temporary address
      // whose footprint (i.e size once serialized) is identical to the final
      // one. Therefore, it has to somewhat ressemble the final one without
      // being valid.
      static nucleus::proton::Address some(block->network(),
                                           block->family(),
                                           block->component());

      ELLE_ASSERT(block->network() == some.network());
      ELLE_ASSERT(block->family() == some.family());
      ELLE_ASSERT(block->component() == some.component());

      // Also allocate a temporary secret with the same length as the final
      // one.
      //
      // Note that the secret length has been provided in bits though the
      // string is calculated in characters.
      static cryptography::SecretKey secret{
        cryptography::cipher::Algorithm::aes256,
        elle::String(static_cast<size_t>(this->_secret_length /
                                         (sizeof(elle::Character) * 8)),
                     static_cast<char>('*'))};

      // Create an egg referencing the given block with a temporary address and
      // secret since the block is transient i.e does not live in the storage
      // layer yet.
      std::shared_ptr<nucleus::proton::Egg> egg{
        new nucleus::proton::Egg{block, some, secret}};

      ELLE_FINALLY_ABORT(block);

      // Allocate a pod for holding the egg.
      Pod* pod = new Pod{std::move(egg), this->_history.end()};

      ELLE_FINALLY_ACTION_DELETE(pod);

      // Insert the pod.
      this->_insert(pod);

      ELLE_FINALLY_ABORT(pod);

      // Construct a handle referencing the created egg.
      nucleus::proton::Handle handle{nucleus::proton::Handle{pod->egg()}};

      // XXX[set the nest in the node]
      pod->egg()->block()->node().nest(*this);

      ELLE_ASSERT(pod->state() == Pod::State::attached);
      ELLE_ASSERT(pod->actors() == 0);

      // Queue the pod, since not yet loaded.
      this->_queue(pod);

      // Increase the nest's size.
      this->_size += pod->egg()->block()->node().footprint();

      // Try to optimize the nest.
      if (this->_size >= this->_threshold)
        this->_optimize();

      ELLE_ASSERT(handle.state() == nucleus::proton::Handle::State::nested);

      return (handle);
    }

    void
    Nest::detach(nucleus::proton::Handle& handle)
    {
      ELLE_TRACE_METHOD(handle);

      ELLE_DEBUG("handle state's: %s", handle.state());

      switch (handle.state())
        {
        case nucleus::proton::Handle::State::unnested:
          {
            // Make the handle evolve so as to reference a newly
            // created egg.
            handle.evolve();

            // Create a new pod.
            Pod* pod = new Pod{handle.egg(), this->_history.end()};

            ELLE_ASSERT(pod->state() == Pod::State::attached);
            ELLE_ASSERT(pod->actors() == 0);

            ELLE_FINALLY_ACTION_DELETE(pod);

            // Insert the pod in the nest.
            this->_insert(pod);

            // Map the handle's address with the pod for other handles
            // to reference the same pod's egg.
            this->_map(handle.address(), pod);

            ELLE_FINALLY_ABORT(pod);

            // Finally, mark the pod as detached.
            pod->state(Pod::State::detached);

            break;
          }
        case nucleus::proton::Handle::State::nested:
          {
            // Retrieve the pod associated with this handle's egg.
            Pod* pod = this->_lookup(handle.egg());

            ELLE_ASSERT(handle.address() == pod->egg()->address());
            ELLE_ASSERT(handle.secret() == pod->egg()->secret());

            ELLE_ASSERT(pod->state() == Pod::State::attached);
            ELLE_ASSERT(pod->actors() == 0);

            // Release the block since no longer used.
            pod->egg()->block().reset();

            // Set the pod as detached.
            pod->state(Pod::State::detached);

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown handle state '%s'",
                                              handle.state()));
        }

      // Try to optimize the nest.
      if (this->_size >= this->_threshold)
        this->_optimize();
    }

    void
    Nest::load(nucleus::proton::Handle& handle)
    {
      ELLE_TRACE_METHOD(handle);

      ELLE_DEBUG("handle state's: %s", handle.state());

      // Act depending on the handle's state.
      switch (handle.state())
        {
        case nucleus::proton::Handle::State::unnested:
          {
            // In this case, this is the first time this handle
            // instance is actually loaded.

            // Note that this does not mean that the nest is not
            // already tracking the associated block. Indeed, a
            // copy of the given handle may have been previously
            // loaded. In this case, try to retrieve the pod associated
            // with the handle address so as to make it track the
            // appropriate egg.
            if (this->_exist(handle.address()) == true)
              {
                // Retrieve the existing pod.
                Pod* pod = this->_lookup(handle.address());

                ELLE_ASSERT(handle.address() == pod->egg()->address());
                ELLE_ASSERT(handle.secret() == pod->egg()->secret());

                ELLE_ASSERT(pod->state() == Pod::State::attached);

                // And make the handle track the block's existing egg.
                handle.place(pod->egg());

                // Make sure the block is loaded in the egg.
                if (pod->egg()->block() == nullptr)
                  this->_load(pod);

                // Make sure the pod no longer resides in the queue since
                // is is being used by at least one actor.
                //
                // In other words, remove it from the queue if nobody was
                // using it before.
                if (pod->actors() == 0)
                  this->_unqueue(pod);

                // Increase the number of actors on the pod.
                pod->actors(pod->actors() + 1);

                // Lock the pod so as to make sure nobody else unloads it
                // on the storage layer while being used.
                reactor::Scheduler::scheduler()->current()->wait(pod->mutex());
              }
            else
              {
                // In this case, no egg exists. One must therefore create
                // an egg, encapsulte it in a pod which must be tracked by
                // the nest.

                // Make the handle evolve so as to reference a newly
                // created egg.
                handle.evolve();

                // Create a new pod.
                Pod* pod = new Pod{handle.egg(), this->_history.end()};

                ELLE_ASSERT(pod->state() == Pod::State::attached);
                ELLE_ASSERT(pod->actors() == 0);

                ELLE_FINALLY_ACTION_DELETE(pod);

                // Insert the pod in the nest.
                this->_insert(pod);

                // Map the handle's address with the pod for other handles
                // to reference the same pod's egg.
                this->_map(handle.address(), pod);

                ELLE_FINALLY_ABORT(pod);

                // Actually load the block from the storage layer.
                ELLE_ASSERT(pod->egg()->block() == nullptr);
                this->_load(pod);

                // Noteworthy that there is no need to unqueue since the pod
                // since it has just been created.

                // Increase the number of actors on the pod.
                pod->actors(pod->actors() + 1);

                // Lock the pod so as to make sure nobody else unloads it
                // on the storage layer while being used.
                reactor::Scheduler::scheduler()->current()->wait(pod->mutex());
              }

            break;
          }
        case nucleus::proton::Handle::State::nested:
          {
            // Retrieve the existing pod.
            Pod* pod = this->_lookup(handle.egg());

            ELLE_ASSERT(handle.address() == pod->egg()->address());
            ELLE_ASSERT(handle.secret() == pod->egg()->secret());

            ELLE_ASSERT(pod->state() == Pod::State::attached);

            // Even though the handle is nested, i.e an egg lies in the nest,
            // one must make sure the block is actually loaded.
            if (pod->egg()->block() == nullptr)
              this->_load(pod);

            // Unqueue the pod, if necessary.
            if (pod->actors() == 0)
              this->_unqueue(pod);

            // Increase the number of actors on the pod.
            pod->actors(pod->actors() + 1);

            // Lock the pod so as to make sure nobody else unloads it
            // on the storage layer while being used.
            reactor::Scheduler::scheduler()->current()->wait(pod->mutex());

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown handle state '%s'",
                                              handle.state()));
        }

      // Try to optimize the nest.
      if (this->_size >= this->_threshold)
        this->_optimize();

      ELLE_ASSERT(handle.state() == nucleus::proton::Handle::State::nested);
      ELLE_ASSERT(handle.egg()->block() != nullptr);
    }

    void
    Nest::unload(nucleus::proton::Handle& handle)
    {
      ELLE_TRACE_METHOD(handle);

      ELLE_DEBUG("handle state's: %s", handle.state());

      switch (handle.state())
        {
        case nucleus::proton::Handle::State::unnested:
          throw Exception(elle::sprintf("unable to unload an unested --- "
                                              "i.e non-loaded --- handle '%s'",
                                              handle));
        case nucleus::proton::Handle::State::nested:
          {
            Pod* pod = this->_lookup(handle.egg());

            ELLE_ASSERT(handle.address() == pod->egg()->address());
            ELLE_ASSERT(handle.secret() == pod->egg()->secret());

            ELLE_ASSERT(pod->state() == Pod::State::attached);

            // Release the pod's lock.
            pod->mutex().release();

            // Decrease the number of actors on the pod.
            ELLE_ASSERT(pod->actors() > 0);
            pod->actors(pod->actors() - 1);

            // Queue the pod, assuming it is no longer used.
            if (pod->actors() == 0)
              this->_queue(pod);

            break;
          }
        default:
          throw Exception(elle::sprintf("unknown handle state '%s'",
                                              handle.state()));
        }

      // Try to optimize the nest.
      if (this->_size >= this->_threshold)
        this->_optimize();

      ELLE_ASSERT(handle.state() == nucleus::proton::Handle::State::nested);
    }
  }
}
