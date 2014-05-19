#include <surface/gap/LinkSendMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.LinkSendMachine");

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

    LinkSendMachine::LinkSendMachine(Transaction& transaction,
                                     uint32_t id,
                                     std::vector<std::string> files,
                                     std::shared_ptr<Data> data)
      : Super::Super(transaction, id, data)
      , Super(transaction, id, std::move(files), data)
      , _data(data)
    {
      this->_run(this->_create_transaction_state);
    }

    LinkSendMachine::~LinkSendMachine()
    {
      this->_stop();
    }

    /*---------------.
    | Implementation |
    `---------------*/

    float
    LinkSendMachine::progress() const
    {
      // FIXME
      return 0;
    }

    void
    LinkSendMachine::transaction_status_update(
      infinit::oracles::Transaction::Status status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);
    }

    void
    LinkSendMachine::_create_transaction()
    {
      infinit::oracles::LinkTransaction::FileList files;
      // FIXME: handle directories, non existing files, empty list and shit.
      for (auto const& file: this->files())
      {
        boost::filesystem::path path(file);
        files.emplace_back(path.filename().string(),
                           boost::filesystem::file_size(path));
      }
      auto response =
        this->state().meta().create_link(files, *this->files().begin());
    }
  }
}
