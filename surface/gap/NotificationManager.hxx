#ifndef NOTIFICATIONMANAGER_HXX
# define NOTIFICATIONMANAGER_HXX

namespace surface
{
  namespace gap
  {
    template<NotificationType type>
    void
    NotificationManager::detach(NotificationHandler const& to_remove)
    {
      auto callback_it = this->_notification_handlers(type);
      if (callback_it == this->_notification_handlers.end())
        return;

      (*callback_it).remove(to_remove);
    }
  }
}

#endif
