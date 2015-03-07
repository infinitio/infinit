#ifndef SURFACE_GAP_STATE_HXX
# define SURFACE_GAP_STATE_HXX

# include <elle/log.hh>

namespace surface
{
  namespace gap
  {
    template <typename T>
    State::Runner<T>::Runner(State::Runner<T>::Callback cb,
                             T notif):
      _cb(std::move(cb)),
      _notification(std::move(notif))
    {}

    template <typename T>
    void
    State::Runner<T>::operator () () const
    {
      ELLE_LOG_COMPONENT("infinit.surface.gap.Runner");
      ELLE_DUMP("%s: run callback: %s", *this,
                elle::demangle(typeid(this->_notification).name()));
      this->_cb(this->_notification);
    }

    template <typename T>
    void
    State::attach_callback(std::function<void (T const&)> cb) const
    {
      auto fn = [cb] (Notification const& notif) -> void
        {
          return cb(static_cast<T const&>(notif));
        };
      this->_callbacks[T::type].emplace_back(fn);
    }

    template <typename T>
    void
    State::enqueue(T const& notif) const
    {
      ELLE_LOG_COMPONENT("surface.gap.State");
      try
      {
        ELLE_DEBUG("%s: enqueue callback: %s", *this, notif);
        std::lock_guard<std::mutex> lock(this->_poll_lock);
        for (auto const& cb: this->_callbacks.at(T::type))
        {
          this->_runners.emplace(new Runner<T>(cb, notif));
        }
      }
      catch (std::out_of_range const&)
      {
        ELLE_DEBUG("%s: no runner for %s", *this, notif);
      }
    }
  }
}

#endif
