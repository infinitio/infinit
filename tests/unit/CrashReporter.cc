#define BOOST_TEST_MODULE crash_reporter

#include <boost/test/unit_test.hpp>

#include <CrashReporter.hh>
#include <elle/system/Process.hh>
#include <elle/print.hh>
#include <fstream>

class meta
{
  std::unique_ptr<elle::system::Process> p;
  int port;
public:
  meta()
  {
    std::string tmp_dirpath = elle::system::check_output("mktemp", "-d");
    tmp_dirpath = tmp_dirpath.substr(0, tmp_dirpath.size() - 2);
    this->p.reset(new elle::system::Process{"./bin/meta-server", {
      "--spawn-db",
      "--port-file", elle::sprintf("%s/meta.port", tmp_dirpath),
      "--port", "0",
    }});
    sleep(2);
    std::ifstream data{tmp_dirpath};
    if (data.good())
    {
      bool found_port = false;
      std::string prefix = "meta_port";
      while (found_port == false)
      {
        std::string line;
        std::getline(data, line);
        std::cout << line << std::endl;
        if (line.compare(0, prefix.size(), prefix))
        {
          std::string port = line.substr(prefix.size());
          std::cout << port << std::endl;
          found_port = true;
        }
      }
    }
  }
  ~meta()
  {
    sleep(1);
    p->interrupt();
  }
};

meta meta{};

BOOST_AUTO_TEST_CASE(report)
{
  elle::crash::report("localhost", 12345, "unit_test");
}
