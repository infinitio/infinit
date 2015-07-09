#include <surface/gap/LinkTransaction.hh>

namespace surface
{
  namespace gap
  {
    LinkTransaction::LinkTransaction(uint32_t id_,
                                     std::string name_,
                                     double mtime_,
                                     boost::optional<std::string> hash_,
                                     boost::optional<std::string> link_,
                                     uint32_t click_count_,
                                     uint64_t size_,
                                     gap_TransactionStatus status_,
                                     elle::UUID const& sender_device_id_,
                                     std::string const& message_,
                                     std::string const& meta_id_,
                                     bool screenshot_)
      : id(id_)
      , name(std::move(name_))
      , mtime(mtime_)
      , hash()
      , link()
      , click_count(click_count_)
      , size(size_)
      , status(status_)
      , sender_device_id(sender_device_id_.repr())
      , message(message_)
      , meta_id(meta_id_)
      , screenshot(screenshot_)
    {
      // FIXME: for now the server always return the share link, as an empty
      // string if not ready. Remove when we remove the old serialization,
      // can handle optionals and not return an empty string.
      if (link_ && !link_.get().empty())
        this->link = std::move(link_);
      if (hash_ && !hash_.get().empty())
        this->hash = std::move(hash_);
    }

    LinkTransaction::~LinkTransaction() noexcept(true)
    {}

    void
    LinkTransaction::print(std::ostream& stream) const
    {
      stream << "LinkTransaction("
             << this->id << ", "
             << (this->hash ? this->hash.get() : "<no hash>") << ", "
             << this->status << " clicked("
             << this->click_count << " time"
             << (this->click_count == 1 ? "" : "s") << ")";
    }

    Notification::Type LinkTransaction::type = NotificationType_LinkTransactionUpdate;
  }
}
