#include <elle/os/environ.hh>

#include <surface/gap/gap_bridge.hh>

ELLE_LOG_COMPONENT("surface.gap.gap_State")

gap_State::gap_State(bool production)
  : gap_State(production, "", "", "", "", true)
{}

gap_State::gap_State(bool production,
                     std::string const& download_dir,
                     std::string const& persistent_config_dir,
                     std::string const& non_persistent_config_dir,
                     std::string const& temp_storage_dir,
                     bool enable_mirroring):
  _configuration(production,
                 enable_mirroring,
                 download_dir,
                 persistent_config_dir,
                 non_persistent_config_dir,
                 temp_storage_dir),
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
      this->_state.reset(new surface::gap::State(this->_configuration));
    });
}
