#ifndef PLASMA_TROPHONIUS_FWD_HH
# define PLASMA_TROPHONIUS_FWD_HH

namespace plasma
{
  namespace trophonius
  {
    enum class NotificationType;

    struct Notification;
    struct NewSwaggerNotification;
    struct DeletedSwaggerNotification;
    struct DeletedFavoriteNotification;
    struct UserStatusNotification;
    struct NetworkUpdateNotification;
    struct PeerConnectionUpdateNotification;
    struct MessageNotification;
  }
}

#endif
