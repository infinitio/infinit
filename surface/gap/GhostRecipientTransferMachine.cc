#include <surface/gap/GhostRecipientTransferMachine.hh>
#include <surface/gap/S3TransferBufferer.hh>

#include <elle/containers.hh>

#include <surface/gap/TransactionMachine.hh>
#include <surface/gap/ReceiveMachine.hh>


ELLE_LOG_COMPONENT("surface.gap.GhostRecipientTransferMachine");

namespace surface
{
  namespace gap
  {

    GhostRecipientTransferMachine::GhostRecipientTransferMachine(
      TransactionMachine& owner)
    : BaseTransferer(owner)
    {
    }

    void
    GhostRecipientTransferMachine::print(std::ostream& os) const
    {
      os << "GhostRecipientTransferMachine";
    }

    void
    GhostRecipientTransferMachine::peer_available(Endpoints const& endpoints)
    {
    }

    void
    GhostRecipientTransferMachine::peer_unavailable()
    {
    }

    void
    GhostRecipientTransferMachine::run()
    {
      ELLE_DEBUG("%s: run", *this);
      // We should wait for the file to be there, ie status ghost_uploaded
      ELLE_TRACE("%s: waiting for ghost_uploaded", *this);
      _owner.ghost_uploaded().wait();
      ELLE_TRACE("%s: fetching raw file from cloud", *this);
      auto archive_info = _owner.archive_info();
      std::string name = archive_info.first;
      S3TransferBufferer s3_handler(*_owner.data(),
        name,
        _owner.make_aws_credentials_getter());
      dynamic_cast<ReceiveMachine*>(&_owner)->get((TransferBufferer&)s3_handler,
        EncryptionLevel_None, " (%s)", elle::Version());
       ELLE_TRACE("%s: transfer finished", *this);
      _owner.gap_state(gap_transaction_finished);
      _owner.finished().open();
      // FIXME: untar the archive?
      // bool is_archive = archive_info.second;
    }
}}
