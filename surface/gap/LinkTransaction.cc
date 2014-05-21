#include <surface/gap/LinkTransaction.hh>

namespace surface
{
  namespace gap
  {
    LinkTransaction::LinkTransaction(uint32_t id_,
                                     std::string const& name_,
                                     double mtime_,
                                     std::string const& link_,
                                     uint32_t click_count_,
                                     gap_TransactionStatus status_)
      : id(id_)
      , name(name_)
      , mtime(mtime_)
      , link(link_)
      , click_count(click_count_)
      , status(status_)
    {}
  }
}
