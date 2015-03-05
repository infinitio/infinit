#ifndef SURFACE_GAP_PEER_TRANSACTION_HH
# define SURFACE_GAP_PEER_TRANSACTION_HH

# include <list>
# include <stdint.h>
# include <string>

# include <elle/UUID.hh>
# include <elle/Printable.hh>

# include <infinit/oracles/PeerTransaction.hh>

# include <surface/gap/enums.hh>
# include <surface/gap/Notification.hh>

namespace surface
{
  namespace gap
  {
    /// This class translates transactions so that headers aren't leaked into
    /// the GUI.
    typedef infinit::oracles::TransactionCanceler TransactionCanceler;
    class PeerTransaction
      : public elle::Printable
      , public surface::gap::Notification
    {
    public:
      PeerTransaction() = default;
      PeerTransaction(uint32_t id,
                      gap_TransactionStatus status,
                      uint32_t sender_id,
                      elle::UUID const& sender_device_id,
                      uint32_t recipient_id,
                      elle::UUID const& recipient_device_id,
                      double mtime,
                      std::list<std::string> const& file_names,
                      int64_t total_size,
                      bool is_directory,
                      std::string const& message,
                      TransactionCanceler const& canceler,
                      std::string const& meta_id);
      ~PeerTransaction() noexcept(true);

      uint32_t id;
      gap_TransactionStatus status;
      uint32_t sender_id;
      std::string sender_device_id;
      uint32_t recipient_id;
      std::string recipient_device_id;
      double mtime;
      std::list<std::string> file_names;
      int64_t total_size;
      bool is_directory;
      std::string message;
      TransactionCanceler canceler;
      std::string meta_id;

      static Notification::Type type;

    private:
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
