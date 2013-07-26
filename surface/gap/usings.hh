#ifndef USINGS_HH
# define USINGS_HH

# include <plasma/trophonius/Client.hh>

namespace surface
{
  namespace gap
  {
    using ::plasma::Transaction;
    using ::plasma::trophonius::Notification;
    using ::plasma::trophonius::TransactionNotification;
    using ::plasma::trophonius::NewSwaggerNotification;
    using ::plasma::trophonius::UserStatusNotification;
    using ::plasma::trophonius::MessageNotification;
    using ::plasma::trophonius::NetworkUpdateNotification;
    using ::plasma::trophonius::PeerConnectionUpdateNotification;
    using ::plasma::trophonius::NotificationType;
  }
}

#endif
