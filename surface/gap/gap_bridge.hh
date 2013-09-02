#ifndef GAPBRIDE_HH
# define GAPBRIDE_HH

#include <surface/gap/State.hh>

#include <reactor/scheduler.hh>
#include <reactor/thread.hh>
#include <elle/attribute.hh>
#include <elle/log.hh>

#include <exception>
#include <thread>
#include <memory>
#include <stdint.h>

extern "C"
{

  /// - Utils -----------------------------------------------------------------
  struct gap_State
  {
    ELLE_ATTRIBUTE_X(reactor::Scheduler, scheduler);
    ELLE_ATTRIBUTE_R(reactor::Thread, keep_alive);
    ELLE_ATTRIBUTE_R(std::thread, scheduler_thread);
    ELLE_ATTRIBUTE(std::unique_ptr<surface::gap::State>, state);
    ELLE_ATTRIBUTE_R(std::exception_ptr, exception);
    ELLE_ATTRIBUTE_R(std::function<void (std::string const&)>, critical_callback);

    gap_Status
    gap_critical_callback(gap_State* state,
                          gap_critical_callback_t cb)
    {
      this->_critical_callback = [&] (std::string const& error)
        {
          cb(error.c_str());
        };
      return gap_ok;
    }

  public:
    // reactor::Scheduler&
    // scheduler() const
    // {
    //   return this->_scheduler;
    // }

    surface::gap::State&
    state()
    {
      return *this->_state;
    }

  public:
    gap_State(char const* meta_host,
              unsigned short meta_port,
              char const* trophonius_host,
              unsigned short trophonius_port,
              char const* apertus_host,
              unsigned short apertus_port):
      _scheduler{},
      _keep_alive{this->_scheduler, "State keep alive",
                  [this]
                  {
                    while (true)
                    {
                      auto& current = *this->_scheduler.current();
                      current.sleep(boost::posix_time::seconds(60));
                    }
                  }},
      _scheduler_thread{
        [&]
        {
          try
          {
            this->_scheduler.run();
          }
          catch (...)
          {
            ELLE_LOG_COMPONENT("surface.gap.bridge");
            ELLE_ERR("exception escaped from State scheduler: %s",
                     elle::exception_string());
            this->_exception = std::current_exception();
            if (this->_critical_callback)
              this->_critical_callback(elle::exception_string());

          }
        }}
    {
      this->_scheduler.mt_run<void>(
        "creating state",
        [&]
        {
          this->_state.reset(
            new surface::gap::State{
              meta_host, meta_port, trophonius_host, trophonius_port,
                apertus_host, apertus_port});
        });
    }

    gap_State():
      _scheduler{},
      _keep_alive{this->_scheduler, "State keep alive",
                  []
                  {
                    while (true)
                    {
                      auto& current = *reactor::Scheduler::scheduler()->current();
                      current.sleep(boost::posix_time::seconds(60));
                    }
                  }},
      _scheduler_thread{
        [&]
        {
          try
          {
            this->_scheduler.run();
          }
          catch (...)
          {
            ELLE_LOG_COMPONENT("surface.gap.bridge");
            ELLE_ERR("exception escaped from State scheduler: %s",
                     elle::exception_string());
            this->_exception = std::current_exception();
            if (this->_critical_callback)
              this->_critical_callback(elle::exception_string());
          }
        }}
    {
      this->_scheduler.mt_run<void>(
        "creating state",
        [&]
        {
          this->_state.reset(new surface::gap::State{});
        });
    }

    ~gap_State()
    {
      elle::Finally sched_destruction{
        [&] ()
        {
          this->_scheduler.mt_run<void>(
            "destroying sched",
            []
            {
              auto& scheduler = *reactor::Scheduler::scheduler();
              scheduler.terminate_now();
            });

          this->_scheduler_thread.join();
        }
      };

      elle::Finally state_destruction{
        [&] ()
        {
          this->_scheduler.mt_run<void>(
            "destroying state",
            [&] () -> void
            {
              this->_state.reset();
            });
        }
      };
    }
  };
}

class _Ret
{
public:
  virtual
  gap_Status
  status() const = 0;
};

template <typename Type>
class Ret: public _Ret
{
public:
  template <typename... Args>
  Ret(Args&&... args):
    _value{std::forward<Args>(args)...}
  {}

  Type
  value() const
  {
    return this->_value.second;
  }

  gap_Status
  status() const
  {
    return this->_value.first;
  }

  operator gap_Status() const
  {
    return this->status();
  }

  operator Type() const
  {
    return this->value();
  }

private:
  std::pair<gap_Status, Type> _value;
};

template <>
class Ret<gap_Status>
{
public:
  template <typename... Args>
  Ret(gap_Status status, Args&&... args):
    _status{status}
  {}

  operator gap_Status() const
  {
    return this->status();
  }

  ELLE_ATTRIBUTE_R(gap_Status, status);
};

template <typename Type>
Ret<Type>
run(gap_State* state,
    std::string const& name,
    std::function<Type (surface::gap::State&)> const& function)
{
  ELLE_LOG_COMPONENT("surface.gap.bridge");
  assert(state != nullptr);

  gap_Status ret = gap_ok;
  try
  {
    reactor::Scheduler& scheduler = state->scheduler();

    return Ret<Type>(
      ret,
      scheduler.mt_run<Type>
      (
        name,
        [&] () { return function(state->state()); }
        )
      );
  }
  catch (elle::HTTPException const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    if (err.code == elle::ResponseCode::error)
      ret = gap_network_error;
    else if (err.code == elle::ResponseCode::internal_server_error)
      ret = gap_api_error;
    else
      ret = gap_internal_error;
  }
  catch (plasma::meta::Exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = (gap_Status) err.err;
  }
  catch (surface::gap::Exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = err.code;
  }
  catch (elle::Exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = gap_internal_error;
  }
  catch (std::exception const& err)
  {
    ELLE_ERR("%s: error: %s", name, err.what());
    ret = gap_internal_error;
  }
  catch (...)
  {
    ELLE_ERR("%s: unknown error type", name);
    ret = gap_internal_error;
  }
  return Ret<Type>{ret, Type{}};
}

#endif
