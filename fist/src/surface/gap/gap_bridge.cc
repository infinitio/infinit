#include <boost/filesystem/fstream.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

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
  auto device_uuid = boost::uuids::nil_generator()();
  bool force_regenerate
    = !elle::os::getenv("INFINIT_FORCE_NEW_DEVICE_ID", "").empty();
  if (!force_regenerate
      && boost::filesystem::exists(common::infinit::device_id_path()))
  {
    ELLE_TRACE("%s: get device uuid from file", *this);
    boost::filesystem::ifstream file(common::infinit::device_id_path());
    std::string struuid;
    file >> struuid;
    device_uuid = boost::uuids::string_generator()(struuid);
  }
  else
  {
    ELLE_TRACE("%s: create device uuid", *this);
    boost::filesystem::create_directories(
      boost::filesystem::path(common::infinit::device_id_path())
      .parent_path());
    device_uuid = boost::uuids::random_generator()();
    std::ofstream file(common::infinit::device_id_path());
    if (!file.good())
      ELLE_ERR("%s: Failed to create device uuid file at %s", *this,
               common::infinit::device_id_path());
    file << device_uuid << std::endl;
  }
  this->_scheduler.mt_run<void>(
    "creating state",
    [&]
    {
      this->_state.reset(
        new surface::gap::State(this->_configuration.meta_protocol(),
                                this->_configuration.meta_host(),
                                this->_configuration.meta_port(),
                                std::move(device_uuid),
                                common::metrics(this->configuration())));
    });
}
