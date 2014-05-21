#ifndef SURFACE_GAP_LINK_TRANSACTION_HH
# define SURFACE_GAP_LINK_TRANSACTION_HH

# include <stdint.h>
# include <string>

# include <boost/optional.hpp>

# include <surface/gap/enums.hh>
# include <surface/gap/Notification.hh>

namespace surface
{
  namespace gap
  {
    /// This class translates transactions so that headers aren't leaked into
    /// the GUI.
    class LinkTransaction:
      public surface::gap::Notification
    {
    public:
      LinkTransaction() = default;
      LinkTransaction(uint32_t id,
                      std::string name,
                      double mtime,
                      boost::optional<std::string> link,
                      uint32_t click_count,
                      gap_TransactionStatus status);
      uint32_t id;
      std::string name;
      double mtime;
      boost::optional<std::string> link;
      uint32_t click_count;
      gap_TransactionStatus status;

      static Notification::Type type;
    };
  }
}

#endif
