#include <surface/gap/LinkTransaction.hh>

namespace surface
{
  namespace gap
  {
    LinkTransaction::LinkTransaction(uint32_t id_,
                                     std::string name_,
                                     double mtime_,
                                     boost::optional<std::string> link_,
                                     uint32_t click_count_,
                                     gap_TransactionStatus status_)
      : id(id_)
      , name(std::move(name_))
      , mtime(mtime_)
      , link(std::move(link_))
      , click_count(click_count_)
      , status(status_)
    {}

    Notification::Type LinkTransaction::type = NotificationType_LinkUpdate;
  }
}
