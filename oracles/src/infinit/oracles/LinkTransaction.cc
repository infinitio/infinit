#include <infinit/oracles/LinkTransaction.hh>

#include <elle/serialization/SerializerIn.hh>

namespace infinit
{
  namespace oracles
  {
    /*-------------.
    | Construction |
    `-------------*/

    LinkTransaction::LinkTransaction():
      Transaction(),
      click_count(),
      cloud_location(),
      message(),
      name(),
      share_link()
    {}

    /*--------------.
    | Serialization |
    `--------------*/

    LinkTransaction::LinkTransaction(elle::serialization::SerializerIn& s)
    {
      this->serialize(s);
    }

    LinkTransaction::~LinkTransaction() noexcept(true)
    {}

    void
    LinkTransaction::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("id", this->id);
      s.serialize("click_count", this->click_count);
      s.serialize("ctime", this->ctime);
      s.serialize("files", this->file_list);
      s.serialize("message", this->message);
      s.serialize("mtime", this->mtime);
      s.serialize("name", this->name);
      s.serialize("sender_device_id", this->sender_device_id);
      s.serialize("sender_id", this->sender_id);
      s.serialize("share_link", this->share_link);
      s.serialize("status", this->status, elle::serialization::as<int>());
    }

    uint64_t
    LinkTransaction::size() const
    {
      uint64_t sum = 0;
      std::for_each(this->file_list.begin(),
                    this->file_list.end(),
                    [&] (FileNameSizePair const& p)
                    {
                      sum += p.second;
                    });
      return sum;
    }

    using elle::serialization::Hierarchy;
    static Hierarchy<Transaction>::Register<LinkTransaction> _register;

    /*----------.
    | Printable |
    `----------*/

    void
    LinkTransaction::print(std::ostream& out) const
    {
      out << "LinkTransaction(" << this->id << ", " << this->status
          << " name: " << this->name << ")";
    }
  }
}
