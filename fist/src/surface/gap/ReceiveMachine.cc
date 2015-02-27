#include <surface/gap/ReceiveMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine");

namespace surface
{
  namespace gap
  {
    using TransactionStatus = infinit::oracles::Transaction::Status;

    /*-------------.
    | Construction |
    `-------------*/
    ReceiveMachine::ReceiveMachine(Transaction& transaction,
                                   uint32_t id,
                                   std::shared_ptr<Data> data)
      : Super(transaction, id, std::move(data))
      , _wait_for_decision_state(
        this->_machine.state_make(
          "wait for decision", std::bind(&ReceiveMachine::_wait_for_decision,
                                         this)))
      , _accept_state(
        this->_machine.state_make(
          "accept", std::bind(&ReceiveMachine::_accept, this)))
      , _accepted("accepted")
      , _accepted_elsewhere("accepted elsewhere")
    {
      this->_machine.transition_add(
        this->_transfer_state,
        this->_finish_state);
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_accept_state,
                                    reactor::Waitables{&this->_accepted});
      this->_machine.transition_add(this->_accept_state,
                                    this->_transfer_state);
      // Another device way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_another_device_state,
                                    reactor::Waitables{&this->_accepted_elsewhere});
      // Reject way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_reject_state,
                                    reactor::Waitables{&this->rejected()});
      // Cancel.
      this->_machine.transition_add(_wait_for_decision_state,
                                    _cancel_state,
                                    reactor::Waitables{&this->canceled()},
                                    true);

      // Due to a bug revealed by the 0.9.26, some users have finished
      // transaction snapshots but with fsm in the "wait_for_decision" state.
      // Because meta is king, just go the finish state.
      // XXX: metrics will be resent.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_finish_state,
                                    reactor::Waitables{&this->finished()},
                                    true);
      this->_machine.transition_add(
        _accept_state,
        _cancel_state,
        reactor::Waitables{&this->canceled()},
        true);
      this->_machine.transition_add(
        _reject_state,
        _cancel_state,
        reactor::Waitables{&this->canceled()},
        true);

      // Exception.
      auto action = [this] ( std::exception_ptr e)
        {
          if (this->transaction().failure_reason().empty())
            this->transaction().failure_reason(elle::exception_string(e));
        };
      this->_machine.transition_add_catch(_wait_for_decision_state, _fail_state)
        .action_exception(action);
      this->_machine.transition_add_catch(_accept_state, _fail_state)
        .action_exception(action);
      this->_machine.transition_add_catch(_reject_state, _fail_state)
        .action_exception(action);
    }

    bool
    ReceiveMachine::concerns_this_device()
    {
      using infinit::oracles::PeerTransaction;
      auto peer_data = std::dynamic_pointer_cast<PeerTransaction>(this->data());
      if (this->state().device().id == peer_data->recipient_device_id)
        return true;
      else
        return false;
    }

    ReceiveMachine::~ReceiveMachine()
    {}

    void
    ReceiveMachine::transaction_status_update(TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      switch (status)
      {
        case TransactionStatus::canceled:
          ELLE_DEBUG("%s: open canceled barrier", *this)
            this->canceled().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_canceled);
          break;
        case TransactionStatus::failed:
          if (this->transaction().failure_reason().empty())
            this->transaction().failure_reason("peer failure");
          ELLE_DEBUG("%s: open failed barrier", *this)
            this->failed().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_failed);
          break;
        case TransactionStatus::finished:
          ELLE_DEBUG("%s: open finished barrier", *this)
            this->finished().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_finished);
          break;
        case TransactionStatus::accepted:
          if (!this->concerns_this_device())
          {
            ELLE_DEBUG("%s: accepted on another device", *this)
              this->_accepted_elsewhere.open();
            break;
          }
        case TransactionStatus::rejected:
          ELLE_DEBUG("%s: rejected on another device", *this)
            this->rejected().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_rejected);
          break;
        case TransactionStatus::initialized:
        case TransactionStatus::cloud_buffered:
          ELLE_DEBUG("%s: ignore status %s", *this, status);
          break;
        case TransactionStatus::ghost_uploaded: // intercepted by inherited machine
        case TransactionStatus::created:
        case TransactionStatus::started:
        case TransactionStatus::none:
        case TransactionStatus::deleted:
          elle::unreachable();
      }
    }

    void
    ReceiveMachine::accept(
      boost::optional<std::string const&> relative_output_dir)
    {
      if (relative_output_dir && !relative_output_dir.get().empty())
        this->_relative_output_dir = relative_output_dir.get();
      else
        this->_relative_output_dir = "";
      ELLE_TRACE_SCOPE("%s: open accept barrier %s",
                       *this, this->transaction_id());
      this->_accepted.open();
    }

    void
    ReceiveMachine::_accept()
    {
      ELLE_TRACE_SCOPE("%s: accepted %s", *this, this->transaction_id());
      this->gap_status(gap_transaction_waiting_accept);
    }

    void
    ReceiveMachine::reject()
    {
      ELLE_TRACE_SCOPE("%s: open reject barrier %s",
                       *this, this->transaction_id());
      this->rejected().open();
    }

    void
    ReceiveMachine::_wait_for_decision()
    {
      ELLE_TRACE_SCOPE("%s: waiting for decision %s", *this, this->transaction_id());
      this->gap_status(gap_transaction_waiting_accept);
    }

    boost::filesystem::path
    ReceiveMachine::eligible_name(boost::filesystem::path start_point,
                                  boost::filesystem::path const path_,
                                  std::string const& name_policy,
                                  std::map<boost::filesystem::path, boost::filesystem::path>& mapping)
    {
      // Turn plateform specific reserved characters from path to hyphens.
      static const std::unordered_set<char> characters_forbidden_in_filenames{
#ifdef INFINIT_WINDOWS
        '|', '<', '>', '"', '?', '*', ':',
#endif
      };
      boost::filesystem::path path = characters_forbidden_in_filenames.empty()
        ? path_
        : [&path_]
          {
            std::string path_as_string = path_.string();
            std::transform(
              path_as_string.begin(),
              path_as_string.end(),
              path_as_string.begin(),
              [] (char a) -> char
              {
                if (characters_forbidden_in_filenames.find(a) !=
                    characters_forbidden_in_filenames.end())
                  return '-';
                return a;
              });
            return boost::filesystem::path(path_as_string);
          }();

      boost::filesystem::path first = *path.begin();
      // Take care of toplevel files with no directory information, we can't
      // add that to the mapping.
      bool toplevel_file = (first == path);
      bool exists = boost::filesystem::exists(start_point / first);
      ELLE_DEBUG("Looking for a replacment name for %s, firstcomp=%s, exists=%s", path, first, exists);
      if (! exists)
      { // we will create the path along the way so we must add itself into mapping
        if (!toplevel_file)
          mapping[first] = first;
        return start_point / path;
      }
      auto it = mapping.find(first);
      ELLE_DEBUG("Looking for %s in mapping", first);
      if (it != mapping.end())
      {
        ELLE_DEBUG("Found it in mapping");
        boost::filesystem::path result = it->second;
        auto it = path.begin();
        ++it;
        for (; it != path.end(); ++it)
          result /= *it;
        ELLE_DEBUG("Returning final path: %s", result);
        return start_point / result;
      }

      // Remove the extensions, add the name_policy and set the extension.
      std::string extensions;
      boost::filesystem::path pattern = first;
      for (; !pattern.extension().empty(); pattern = pattern.stem())
        extensions = pattern.extension().string() + extensions;

      // Ugly.
      for (size_t i = 2; i < std::numeric_limits<size_t>::max(); ++i)
      {
        boost::filesystem::path replace = pattern;
        replace += elle::sprintf(name_policy, i);
        replace += extensions;
        if (!boost::filesystem::exists(start_point / replace))
        {
          if (!toplevel_file)
            mapping[first] = replace;
          ELLE_DEBUG("Adding in mapping: %s -> %s", first, replace);
          boost::filesystem::path result = replace;
          auto it = path.begin();
          ++it;
          for (; it != path.end(); ++it)
            result /= *it;
          ELLE_DEBUG("Found %s", result);
          return start_point / result;
        }
      }


      throw elle::Exception(
        elle::sprintf("unable to find a suitable name that matches %s", first));
    }

    boost::filesystem::path
    ReceiveMachine::trim(boost::filesystem::path const& item,
                         boost::filesystem::path const& root)
    {
      if (item == root)
        return "";

      auto it = item.begin();
      boost::filesystem::path rel;
      for(; rel != root && it != item.end(); ++it)
        rel /= *it;
      if (it == item.end())
        throw elle::Exception(
          elle::sprintf("%s is not the root of %s", root, item));

      boost::filesystem::path trimed;
      for (; it != item.end(); ++it)
        trimed /= *it;

      return trimed;
    }
  }
}
