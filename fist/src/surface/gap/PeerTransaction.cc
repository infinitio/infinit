#include <surface/gap/PeerTransaction.hh>

namespace surface
{
  namespace gap
  {
    PeerTransaction::PeerTransaction(
      uint32_t id_,
      gap_TransactionStatus status_,
      uint32_t sender_id_,
      elle::UUID const& sender_device_id_,
      uint32_t recipient_id_,
      elle::UUID const& recipient_device_id_,
      double mtime_,
      std::list<std::string> const& file_names_,
      int64_t total_size_,
      bool is_directory_,
      std::string const& message_,
      TransactionCanceler const& canceler_,
      std::string const & meta_id_)
        : id(id_)
        , status(status_)
        , sender_id(sender_id_)
        , sender_device_id(
            sender_device_id_.is_nil() ? "" : sender_device_id_.repr())
        , recipient_id(recipient_id_)
        , recipient_device_id(
            recipient_device_id_.is_nil() ? "" : recipient_device_id_.repr())
        , mtime(mtime_)
        , file_names(file_names_)
        , total_size(total_size_)
        , is_directory(is_directory_)
        , message(message_)
        , canceler(canceler_)
        , meta_id(meta_id_)
    {}

    PeerTransaction::~PeerTransaction() noexcept(true)
    {}

    void
    PeerTransaction::print(std::ostream& stream) const
    {
      stream << "PeerTransaction("
             << this->id << ", "
             << this->status
             << " from->to " << this->sender_id << "->" << this->recipient_id
             << ")";
    }

    Notification::Type PeerTransaction::type = NotificationType_PeerTransactionUpdate;
  }
}
