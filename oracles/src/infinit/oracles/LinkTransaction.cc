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
      expiry_time(),
      hash(),
      name()
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
      s.serialize("expiry_time", this->expiry_time);
      s.serialize("hash", this->hash);
      s.serialize("link", this->link);
      s.serialize("mtime", this->mtime);
      s.serialize("name", this->name);
      s.serialize("sender_device_id", this->sender_device_id);
      s.serialize("sender_id", this->sender_id);
      s.serialize("share_link", this->share_link);
      s.serialize("status", this->status, elle::serialization::as<int>());
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