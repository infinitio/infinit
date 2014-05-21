#ifndef SURFACE_GAP_LINK_TRANSACTION_HH
# define SURFACE_GAP_LINK_TRANSACTION_HH

# include <stdint.h>
# include <string>

# include <surface/gap/enums.hh>

namespace surface
{
  namespace gap
  {
    /// This class translates transactions so that headers aren't leaked into
    /// the GUI.
    class LinkTransaction
    {
    public:
      LinkTransaction() = default;
      LinkTransaction(uint32_t id,
                      std::string const& name,
                      double mtime,
                      std::string const& link,
                      uint32_t click_count,
                      gap_TransactionStatus status);

    public:
      uint32_t id;
      std::string name;
      double mtime;
      std::string link;
      uint32_t click_count;
      gap_TransactionStatus status;
    };
  }
}

#endif
