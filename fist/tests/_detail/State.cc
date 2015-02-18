#include "State.hh"

#include <elle/os/path.hh>
#include <elle/system/home_directory.hh>
#include <fist/tests/server.hh>
#include <fist/tests/_detail/Authority.hh>

extern const std::vector<unsigned char> fingerprint;

namespace tests
{
  State::State(Server& server,
               boost::uuids::uuid device_id)
    : surface::gap::State(
      "http", "127.0.0.1", server.port(),
      std::move(device_id), fingerprint,
      elle::os::path::join(elle::system::home_directory().string(), "Downloads"))
  {
    this->s3_hostname(aws::URL{"http://",
          elle::sprintf("localhost:%s", server.port()),
          "/s3"});
  }
}

const std::vector<unsigned char> fingerprint =
{
  0x66, 0x84, 0x68, 0xEB, 0xBE, 0x83, 0xA0, 0x5C, 0x6A, 0x32,
  0xAD, 0xD2, 0x58, 0x62, 0x01, 0x31, 0x79, 0x96, 0x78, 0xB8
};
