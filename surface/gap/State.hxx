#ifndef SURFACE_GAP_STATE_HXX
# define SURFACE_GAP_STATE_HXX

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
      this->_cb(this->_notification);
    }

    template <typename T>
    void
    State::attach_callback(std::function<void (T const&)> cb) const
    {
      ELLE_LOG_COMPONENT("surface.gap.State");
      ELLE_LOG("attach callback");
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
        ELLE_LOG("run cb");
        for (auto const& cb: this->_callbacks.at(T::type))
          this->_runners.emplace(new Runner<T>(cb, notif));
      }
      catch (std::out_of_range const&)
      {
        ELLE_WARN("%s: No runner for %s", *this, notif);
      }
    }

  }
}

#endif
