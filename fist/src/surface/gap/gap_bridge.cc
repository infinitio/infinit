#include <elle/os/environ.hh>

#include <surface/gap/gap_bridge.hh>

ELLE_LOG_COMPONENT("surface.gap.gap_State")

gap_State::gap_State(bool production)
  : gap_State(production, "", "", "", true, 0)
{}

gap_State::gap_State(bool production,
                     std::string const& download_dir,
                     std::string const& persistent_config_dir,
                     std::string const& non_persistent_config_dir,
                     bool enable_mirroring,
                     uint64_t max_mirroring_size):
  _configuration(production,
                 enable_mirroring,
                 max_mirroring_size,
                 download_dir,
                 persistent_config_dir,
                 non_persistent_config_dir),
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
        ELLE_ERR("exception escaped from State scheduler: %s",
                 elle::exception_string());
        this->_exception = std::current_exception();
        if (this->_critical_callback)
          this->_critical_callback();
      }
    }}
{
  this->_scheduler.mt_run<void>(
    "creating state",
    [&]
    {
      ELLE_TRACE("creating state from configuration %s", this->_configuration)
        this->_state.reset(new surface::gap::State(this->_configuration));
      if (this->_state == nullptr)
        ELLE_ERR("state wasn't created successfully");
    });
}
