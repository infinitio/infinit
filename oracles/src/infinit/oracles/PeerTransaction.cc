#include <elle/serialization/Serializer.hh>
#include <elle/serialization/SerializerIn.hh>

#include <infinit/oracles/PeerTransaction.hh>

namespace infinit
{
  namespace oracles
  {
    TransactionCanceler::TransactionCanceler()
      : user_id()
      , device_id()
    {}

    TransactionCanceler::TransactionCanceler(std::string const& user_id,
                                             std::string const& device_id)
      : user_id(user_id)
      , device_id(device_id)
    {}

    TransactionCanceler::TransactionCanceler(
      elle::serialization::SerializerIn& s)
    {
      this->serialize(s);
    }

    void
    TransactionCanceler::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("user", this->user_id);
      s.serialize("device", this->device_id);
    }

    /*-------------.
    | Construction |
    `-------------*/

    PeerTransaction::PeerTransaction()
      : Transaction()
      , files()
      , is_directory()
      , files_count()
      , message()
      , recipient_id()
      , recipient_fullname()
      , recipient_device_id()
      , recipient_device_name()
      , sender_fullname()
      , total_size()
      , canceler()
    {}

    PeerTransaction::PeerTransaction(std::string sender_id,
                                     std::string sender_fullname,
                                     std::string sender_device_id,
                                     std::string recipient_id)
      : Transaction(std::move(sender_id), std::move(sender_device_id))
      , files()
      , is_directory()
      , files_count()
      , message()
      , recipient_id(std::move(recipient_id))
      , recipient_fullname()
      , recipient_device_id()
      , recipient_device_name()
      , sender_fullname(std::move(sender_fullname))
      , total_size()
      , canceler()
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
      // FIXME: this is backward compatibility for old snapshots (< 0.9.18), to
      // be removed in a few releases.
      try
      {
        s.serialize("is_ghost", this->is_ghost);
        if (this->is_ghost)
          s.serialize("download_link", this->download_link);
      }
      catch (elle::serialization::Error const&)
      {
        ELLE_ASSERT(s.in());
        this->is_ghost = false;
      }
      // Added in 0.9.20.
      try
      {
        s.serialize("canceler", this->canceler);
      }
      catch (elle::serialization::Error const&)
      {
        this->canceler.user_id = "";
        this->canceler.device_id = "";
      }
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
