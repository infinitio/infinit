#include <infinit/oracles/LinkTransaction.hh>

namespace infinit
{
  namespace oracles
  {
    LinkTransaction::LinkTransaction():
      Transaction(),
      click_count(),
      cloud_location(),
      expiry_time(),
      hash(),
      name()
    {}

    std::ostream&
    operator <<(std::ostream& out, LinkTransaction const& t)
    {
      out << "LinkTransaction(" << t.id << ", " << t.status
          << " name: " << t.name << ")";
      return out;
    }
  }
}
