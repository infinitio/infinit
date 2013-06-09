#include <boost/foreach.hpp>

#include <elle/log.hh>

#include <etoile/journal/Journal.hh>
#include <etoile/depot/Depot.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/gear/Transcript.hh>
#include <etoile/gear/Action.hh>
#include <etoile/Exception.hh>

#include <nucleus/proton/Block.hh>

#include <Infinit.hh>

ELLE_LOG_COMPONENT("infinit.etoile.journal.Journal");

/// XXX[to remove when file system I/Os while be async]
#undef ETOILE_JOURNAL_THREAD

namespace etoile
{
  namespace journal
  {
    /*------------------.
    | Static Attributes |
    `------------------*/

#ifdef ETOILE_JOURNAL_THREAD
    std::set<std::unique_ptr<gear::Transcript>> Journal::_queue;
#endif

    /*---------------.
    | Static Methods |
    `---------------*/

    void
    Journal::record(std::unique_ptr<gear::Transcript>&& transcript)
    {
      ELLE_TRACE_FUNCTION(transcript);

      ELLE_ASSERT_NEQ(transcript, nullptr);

      // Ignore empty transcripts.
      if (transcript->empty() == true)
        {
          ELLE_DEBUG("ignore this empty transcript");
          return;
        }

#ifdef ETOILE_JOURNAL_THREAD
      try
       {
         // Insert the transcript in the journal's queue.
         Journal::_queue.insert(transcript);

         // Spawn a thread and do not wait for it to complete since
         // we want the processing to occur in the background as it
         // may take some time.
         new reactor::Thread(*reactor::Scheduler::scheduler(),
                             "journal process",
                             boost::bind(&Journal::_process,
                                         std::move(transcript)),
                             true);

       }
      catch (std::exception const& err)
        {
          // Remove the transcript since something went wrong.
          Journal::_queue.erase(transcript);

          throw Exception("unable to spawn a new thread: '%s'", err.what());
        }
#else
      Journal::_process(std::move(transcript));
#endif
    }

    elle::Status
    Journal::Record(gear::Scope*            scope)
    {
      ELLE_TRACE_SCOPE("Journal::Record(%s)", *scope);

      ELLE_FINALLY_ACTION_DELETE(scope);

      ELLE_ASSERT_EQ(scope->actors.empty(), true);

      // Ignore empty scope' transcripts.
      if (scope->context->transcript().empty() == true)
        {
          ELLE_DEBUG("ignore this scope because it has an empty transcript");
          return elle::Status::Ok;
        }

      // Retrieve the transcript from the context.
      std::unique_ptr<gear::Transcript> transcript{scope->context->cede()};

      // Record the transcript for processing.
      Journal::record(std::move(transcript));

      // Update the context's state.
      scope->context->state = gear::Context::StateJournaled;

      ELLE_FINALLY_ABORT(scope);

      // Finally, delete the scope.
      delete scope;

      return elle::Status::Ok;
    }

    std::unique_ptr<nucleus::proton::Block>
    Journal::retrieve(nucleus::proton::Address const& address,
                      nucleus::proton::Revision const& revision)
    {
      ELLE_TRACE_FUNCTION(address, revision);

#ifdef ETOILE_JOURNAL_THREAD
      for (auto transcript: Journal::_queue)
        {
          ELLE_DEBUG_SCOPE("exploring transcript");

          for (auto action: *transcript)
            {
              switch (action->type())
                {
                case gear::Action::Type::push:
                  {
                    ELLE_ASSERT(
                      dynamic_cast<gear::action::Push const*>(action) !=
                      nullptr);
                    auto _action =
                      static_cast<gear::action::Push const*>(action);

                    // Ignore non-matching addresses.
                    if (_action->address() != address)
                      continue;

                    if (revision == nucleus::proton::Revision::Any)
                      {
                        ELLE_DEBUG("cloning the block associated with the "
                                   "action %s", *_action);

                        return (Journal::_clone(address.component(),
                                                _action->block()));
                      }
                    else
                      {
                        auto _block =
                          dynamic_cast<nucleus::proton::MutableBlock const*>(
                            &_action->block());

                        // Ignore non-mutable-blocks and non-matching revisions.
                        if ((_block == nullptr) ||
                            (_block->revision() != revision))
                          continue;

                        ELLE_DEBUG("cloning the mutable block associated "
                                   "with the action %s", *_action);

                        return (Journal::_clone(address.component(),
                                                _action->block()));
                      }

                    break;
                  }
                case gear::Action::Type::wipe:
                  {
                    ELLE_ASSERT(
                      dynamic_cast<gear::action::Wipe const*>(action) !=
                      nullptr);
                    auto _action =
                      static_cast<gear::action::Wipe const*>(action);

                    // Ignore non-matching addresses.
                    if (_action->address() != address)
                      continue;

                    // If the requested block is about to be wiped,
                    // throw an error.
                    throw Exception("this block has been scheduled "
                                          "for deletion");
                  }
                }
            }
        }
#endif

      ELLE_DEBUG("the requested block is not present in the journal");

      return (std::unique_ptr<nucleus::proton::Block>(nullptr));
    }

    void
    Journal::_process(std::unique_ptr<gear::Transcript>&& transcript)
    {
      ELLE_TRACE_FUNCTION(transcript);

      ELLE_DEBUG("pushing blocks");

      // XXX[to improve in the future]
      // Go through the blocks which needs to be pushed.
      for (auto action: *transcript)
        {
          switch (action->type())
            {
            case gear::Action::Type::push:
              {
                action->apply<depot::Depot>();
                break;
              }
            case gear::Action::Type::wipe:
              break;
            }
        }

      ELLE_DEBUG("wiping blocks");

      // Then, process the blocks to wipe.
      for (auto action: *transcript)
        {
          switch (action->type())
            {
            case gear::Action::Type::push:
              break;
            case gear::Action::Type::wipe:
              {
                action->apply<depot::Depot>();
                break;
              }
            }
        }

#ifdef ETOILE_JOURNAL_THREAD
      // Remove the transcript from the queue.
      Journal::_queue.erase(transcript);
#endif

      ELLE_DEBUG("transcript processed successfully");
    }

    std::unique_ptr<nucleus::proton::Block>
    Journal::_clone(nucleus::neutron::Component const component,
                    nucleus::proton::Block const& block)
    {
      ELLE_TRACE_FUNCTION(component, block);

      // XXX[the method below is temporary since expensive:
      //     the block is serialized and deserialized instead
      //     of copying the block]
      // XXX[note however that it is also good because temporary
      //     values (which are not serialized such as state) are
      //     thus reinitialized]

      std::stringstream stream(std::ios_base::in |
                               std::ios_base::out |
                               std::ios_base::binary);

      // Serialize the block.
      // XXX[to improve: contact Raphael or cf. hole/storage/]
      static_cast<
        elle::serialize::Serializable<
          elle::serialize::BinaryArchive>
        const&>(block).serialize(stream);

      // Allocate a new block.
      auto const& factory = nucleus::proton::block::factory<>();

      nucleus::proton::Block* _block =
        factory.allocate<nucleus::proton::Block>(component);

      ELLE_FINALLY_ACTION_DELETE(_block);

      // Deserialize the archive into a new block.
      // XXX[to improve: contact Raphael or cf. hole/storage/]
      static_cast<
        elle::serialize::Serializable<
          elle::serialize::BinaryArchive>
        *>(_block)->deserialize(stream);

      ELLE_FINALLY_ABORT(_block);

      return (std::unique_ptr<nucleus::proton::Block>(_block));
    }
  }
}
