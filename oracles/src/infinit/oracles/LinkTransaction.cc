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

    /*-----------.
    | Properties |
    `-----------*/

    bool
    LinkTransaction::concern_user(std::string const& user_id) const
    {
      // FIXME
      return false;
    }

    bool
    LinkTransaction::concern_device(std::string const& user_id,
                                    std::string const& device_id) const
    {
      // FIXME
      return false;
    }

    /*--------------.
    | Serialization |
    `--------------*/

    LinkTransaction::LinkTransaction(elle::serialization::SerializerIn& s)
    {
      this->serialize(s);
    }

    void
    LinkTransaction::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("_id", this->id);
      s.serialize("sender_id", this->sender_id);
      s.serialize("sender_device_id", this->sender_device_id);
      s.serialize("ctime", this->ctime);
      s.serialize("mtime", this->mtime);
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
