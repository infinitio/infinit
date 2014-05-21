#include <elle/serialization/Serializer.hh>
#include <elle/serialization/SerializerIn.hh>

#include <infinit/oracles/PeerTransaction.hh>

namespace infinit
{
  namespace oracles
  {
    /*-------------.
    | Construction |
    `-------------*/

    PeerTransaction::PeerTransaction():
      Transaction(),
      files(),
      is_directory(),
      files_count(),
      message(),
      recipient_id(),
      recipient_fullname(),
      recipient_device_id(),
      recipient_device_name(),
      sender_fullname(),
      total_size()
    {}

    PeerTransaction::PeerTransaction(std::string sender_id,
                                     std::string sender_fullname,
                                     std::string sender_device_id,
                                     std::string recipient_id):
      Transaction(std::move(sender_id), std::move(sender_device_id)),
      files(),
      is_directory(),
      files_count(),
      message(),
      recipient_id(std::move(recipient_id)),
      recipient_fullname(),
      recipient_device_id(),
      recipient_device_name(),
      sender_fullname(std::move(sender_fullname)),
      total_size()
    {}

    PeerTransaction::~PeerTransaction() noexcept(true)
    {}

    /*--------------.
    | Serialization |
    `--------------*/

    PeerTransaction::PeerTransaction(elle::serialization::SerializerIn& s)
    {
      this->serialize(s);
    }

    void
    PeerTransaction::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("_id", this->id);
      s.serialize("sender_id", this->sender_id);
      s.serialize("sender_fullname", this->sender_fullname);
      s.serialize("sender_device_id", this->sender_device_id);
      s.serialize("recipient_id", this->recipient_id);
      s.serialize("recipient_fullname", this->recipient_fullname);
      s.serialize("recipient_device_id", this->recipient_device_id);
      s.serialize("recipient_device_name", this->recipient_device_name);
      s.serialize("message", this->message);
      s.serialize("files", this->files);
      s.serialize("files_count", this->files_count);
      s.serialize("total_size", this->total_size);
      s.serialize("ctime", this->ctime);
      s.serialize("mtime", this->mtime);
      s.serialize("is_directory", this->is_directory);
      s.serialize("status", this->status, elle::serialization::as<int>());
    }

    using elle::serialization::Hierarchy;
    static Hierarchy<Transaction>::Register<PeerTransaction> _register;

    /*----------.
    | Printable |
    `----------*/

    void
    PeerTransaction::print(std::ostream& out) const
    {
      out << "PeerTransaction(" << this->id << ", " << this->status
          << " sender_fullname: " << this->sender_fullname
          << " recipient_fullname: " << this->recipient_fullname << ")";
    }
  }
}
