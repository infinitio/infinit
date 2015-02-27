#ifndef SURFACE_GAP_LINK_TRANSACTION_HH
# define SURFACE_GAP_LINK_TRANSACTION_HH

# include <stdint.h>
# include <string>

# include <boost/optional.hpp>

# include <elle/Printable.hh>

# include <surface/gap/enums.hh>
# include <surface/gap/Notification.hh>

namespace surface
{
  namespace gap
  {
    /// This class translates transactions so that headers aren't leaked into
    /// the GUI.
    class LinkTransaction
      : public surface::gap::Notification
    {
    public:
      LinkTransaction() = default;
      LinkTransaction(uint32_t id,
                      std::string name,
                      double mtime,
                      boost::optional<std::string> link,
                      uint32_t click_count,
                      gap_TransactionStatus status,
                      std::string const& sender_device_id,
                      std::string const& message,
                      std::string const& meta_id);
      ~LinkTransaction() noexcept(true);

      uint32_t id;
      std::string name;
      double mtime;
      boost::optional<std::string> link;
      uint32_t click_count;
      gap_TransactionStatus status;
      std::string sender_device_id;
      std::string message;
      std::string meta_id;

      static Notification::Type type;

    private:
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
