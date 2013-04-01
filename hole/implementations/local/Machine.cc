#include <hole/implementations/local/Machine.hh>
#include <hole/Hole.hh>
#include <hole/Exception.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/ImmutableBlock.hh>
#include <nucleus/proton/MutableBlock.hh>
#include <nucleus/proton/Revision.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Access.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.hole.local.Machine");

namespace hole
{
  namespace implementations
  {
    namespace local
    {

      Machine::Machine(hole::Hole& hole)
        : _hole(hole)
      {}

//
// ---------- holeable --------------------------------------------------------
//
      void
      Machine::put(nucleus::proton::Address const& address,
                   nucleus::proton::ImmutableBlock const& block)
      {
        ELLE_TRACE_METHOD(address, block);

        this->_hole.storage().store(address, block);
      }

      void
      Machine::put(const nucleus::proton::Address& address,
                   const nucleus::proton::MutableBlock& block)
      {
        ELLE_TRACE_METHOD(address, block);

        // validate the block, depending on its component.
        //
        // indeed, the Object component requires as additional block for
        // being validated.
        switch (address.component())
          {
          case nucleus::neutron::ComponentObject:
            {
              /* XXX[need to change the way validation works by relying
                     on a callback]
              const nucleus::neutron::Object* object =
                static_cast<const nucleus::neutron::Object*>(&block);
              assert(dynamic_cast<const nucleus::neutron::Object*>(&block) != nullptr);

              // validate the object according to the presence of
              // a referenced access block.
              if (object->access() != nucleus::proton::Address::null())
                {
                  ELLE_TRACE(
                    "Put nucleus::Object MutableBlock %p"
                    " with a referenced access block",
                    this
                    ) {
                    // Load the access block.
                    std::unique_ptr<nucleus::proton::Block> block
                      (this->_hole.storage().load(object->access(), nucleus::proton::Revision::Last));
                      std::unique_ptr<nucleus::neutron::Access> access
                      (dynamic_cast<nucleus::neutron::Access*>(block.release()));
                    if (access == nullptr)
                      throw Exception("Expected an access block.");

                    object->validate(address, access.get());
                  }
                }
              else
                {
                  ELLE_TRACE(
                      "Put nucleus::Object MutableBlock %p"
                      " with a Null access block",
                      this)
                    {
                      // validate the object.
                      object->validate(address, nullptr);
                    }
                }
              */

              break;
            }
          default:
            {
              ELLE_TRACE("Put common MutableBlock %p", &block)
                {
                  // validate the block through the common interface.
                  block.validate(address);
                }

              break;
            }
          case nucleus::neutron::ComponentUnknown:
            {
              throw Exception(
                elle::sprintf("Unknown component '%u'.", address.component())
                );
            }
          }

        this->_hole.storage().store(address, block);
      }

      std::unique_ptr<nucleus::proton::Block>
      Machine::get(const nucleus::proton::Address& address)
      {
        ELLE_TRACE_METHOD(address);

        std::unique_ptr<nucleus::proton::Block> block =
          this->_hole.storage().load(address);

        block->validate(address);

        return block;
      }

      std::unique_ptr<nucleus::proton::Block>
      Machine::get(const nucleus::proton::Address& address,
                   const nucleus::proton::Revision& revision)
      {
        ELLE_TRACE_METHOD(address, revision);

         // load the block.
         std::unique_ptr<nucleus::proton::Block> block =
           this->_hole.storage().load(address, revision);

        // validate the block, depending on its component.
        //
        // indeed, the Object component requires as additional block for
        // being validated.
        switch (address.component())
          {
          case nucleus::neutron::ComponentObject:
            {
              /* XXX[need to change the way validation works by relying
                     on a callback]
              const nucleus::neutron::Object* object =
                static_cast<const nucleus::neutron::Object*>(block.get());
              assert(dynamic_cast<const nucleus::neutron::Object*>(block.get()) != nullptr);

              // validate the object according to the presence of
              // a referenced access block.
              if (object->access() != nucleus::proton::Address::null())
                {
                  // Load the access block.
                  std::unique_ptr<nucleus::proton::Block> accesBlock
                    = this->_hole.storage().load(object->access(),
                                                 nucleus::proton::Revision::Last);
                  std::unique_ptr<nucleus::neutron::Access> access
                    (dynamic_cast<nucleus::neutron::Access*>(accesBlock.release()));
                  if (access == nullptr)
                    throw Exception("Expected an access block.");

                  // Validate the object.
                  object->validate(address, access.get());
                }
              else
                {
                  // validate the object.
                  object->validate(address, nullptr);
                }
              */

              break;
            }
          case nucleus::neutron::ComponentUnknown:
            {
              throw Exception(elle::sprintf("unknown component '%u'.",
                                                  address.component()));
            }
          default:
            {
              // validate the block through the common interface.
              block->validate(address);

              break;
            }
          }

        return block;
      }

      void
      Machine::wipe(const nucleus::proton::Address& address)
      {
        ELLE_TRACE_METHOD(address);

        // treat the request depending on the nature of the block which
        // the addres indicates.
        // FIXME: why a switch if we call the same method in both case.
        switch (address.family())
          {
          case nucleus::proton::Family::content_hash_block:
            {
              // erase the immutable block.
              this->_hole.storage().erase(address);

              break;
            }
          case nucleus::proton::Family::public_key_block:
          case nucleus::proton::Family::owner_key_block:
          case nucleus::proton::Family::imprint_block:
            {
              // erase the mutable block.
              this->_hole.storage().erase(address);

              break;
            }
          default:
            {
              throw Exception("Unknown block family.");
            }
          }
      }

//
// ---------- dumpable --------------------------------------------------------
//

      ///
      /// this method dumps the implementation.
      ///
      elle::Status      Machine::Dump(const elle::Natural32     margin) const
      {
        elle::String    alignment(margin, ' ');

        std::cout << alignment << "[Machine]" << std::endl;

        return elle::Status::Ok;
      }

    }
  }
}
