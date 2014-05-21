#include <surface/gap/LinkTransaction.hh>

namespace surface
{
  namespace gap
  {
    LinkTransaction::LinkTransaction(uint32_t id,
                                     std::string const& name,
                                     double mtime,
                                     std::string const& link,
                                     uint32_t click_count,
                                     gap_TransactionStatus status):
      id(id),
      name(name),
      mtime(mtime),
      link(link),
      click_count(click_count),
      status(status)
    {}
  }
}
