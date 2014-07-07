#include <elle/os/environ.hh>

#include <surface/gap/gap_bridge.hh>

ELLE_LOG_COMPONENT("surface.gap.gap_State")

gap_State::gap_State(bool production):
  _configuration(production),
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
        new surface::gap::State(this->_configuration.meta_protocol(),
                                this->_configuration.meta_host(),
                                this->_configuration.meta_port(),
                                this->_configuration.device_id(),
                                this->_configuration.trophonius_fingerprint(),
                                common::metrics(this->configuration())));
    });
}
