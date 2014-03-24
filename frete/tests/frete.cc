#include <boost/filesystem/fstream.hpp>

#include <elle/test.hh>
#include <elle/Buffer.hh>

#include <reactor/Barrier.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>

#include <cryptography/KeyPair.hh>
#include <cryptography/Code.hh>
#include <cryptography/Output.hh>

#include <protocol/ChanneledStream.hh>
#include <protocol/Serializer.hh>

#include <frete/Frete.hh>
#include <frete/RPCFrete.hh>

#include <elle/finally.hh>
#include <elle/log.hh>
#include <elle/test.hh>


ELLE_LOG_COMPONENT("test.frete");

#ifdef VALGRIND
# include <valgrind/valgrind.h>
#else
# define RUNNING_ON_VALGRIND 0
#endif

static
bool
compare_files(boost::filesystem::path const& p1,
              boost::filesystem::path const& p2,
              bool must_have_same_name = true)
{
  ELLE_TRACE_FUNCTION(p1, p2, must_have_same_name);
  if (!(boost::filesystem::exists(p1) and boost::filesystem::exists(p2)))
  {
    if (!boost::filesystem::exists(p1))
      ELLE_ERR("file %s doesn't exist", p1);

    if (!boost::filesystem::exists(p2))
      ELLE_ERR("file %s doesn't exist", p2);

    return false;
  }

  if (must_have_same_name && (p1.filename() != p2.filename()))
  {
    ELLE_ERR("%s and %s don't have same name", p1, p2);
    return false;
  }

  if (boost::filesystem::file_size(p1) != boost::filesystem::file_size(p2))
  {
    ELLE_ERR("%s (%sO) and %s (%sO) don't have the same size",
             p1,
             boost::filesystem::file_size(p1),
             p2,
             boost::filesystem::file_size(p2));
    return false;
  }

  boost::filesystem::ifstream file1{p1};
  boost::filesystem::ifstream file2{p2};

  elle::Buffer file1content(boost::filesystem::file_size(p1));
  elle::Buffer file2content(boost::filesystem::file_size(p2));

  file1content.size(
    file1.readsome(reinterpret_cast<char*>(file1content.mutable_contents()),
                  elle::Buffer::max_size));
  if (file1content.size() == elle::Buffer::max_size)
    throw std::runtime_error("file too big to be compared");

  file2content.size(
    file2.readsome(reinterpret_cast<char*>(file2content.mutable_contents()),
                  elle::Buffer::max_size));
  if (file2content.size() == elle::Buffer::max_size)
    throw std::runtime_error("file too big to be compared");

  return file1content == file2content;
}

ELLE_TEST_SCHEDULED(connection)
{
  auto recipient_key_pair = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);

  int port = 0;

  reactor::Barrier listening;

  // Create dummy test fs.
  boost::filesystem::path root("frete/tests/fs-connection");
  auto clear_root =
    [&] {try { boost::filesystem::remove_all(root); } catch (...) {}};

  if (boost::filesystem::exists(root))
    clear_root();

  boost::filesystem::create_directory(root);
  elle::SafeFinally rm_root( [&] () { clear_root(); } );

  boost::filesystem::path empty(root / "empty");
  boost::filesystem::ofstream{empty};

  boost::filesystem::path content(root / "content");
  {
    boost::filesystem::ofstream f(content);
    f << "content\n";
  }

  boost::filesystem::path dir(root / "dir");
  boost::filesystem::create_directory(dir);
  {
    boost::filesystem::ofstream f((dir / "1"));
    f << "1";
  }
  boost::filesystem::create_directory(dir);
  {
    boost::filesystem::ofstream f((dir / "2"));
    f << "2";
  }

  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("server", [&]
    {
      auto snap = root;
      snap += "server";

      reactor::network::TCPServer server{};
      server.listen();
      port = server.port();
      listening.open();
      std::unique_ptr<reactor::network::Socket> socket(server.accept());
      infinit::protocol::Serializer serializer(
        *reactor::Scheduler::scheduler(), *socket);
      infinit::protocol::ChanneledStream channels(
        *reactor::Scheduler::scheduler(), serializer);
      frete::Frete frete("suce", recipient_key_pair.K(), snap);
      frete::RPCFrete rpcs(frete, channels);

      frete.add(empty);
      frete.add(content);
      frete.add(dir);

      rpcs.run();

      reactor::Scheduler::scheduler()->current()->Waitable::wait();
    });

    scope.run_background("client", [&]
    {
      auto snap = root;
      snap += "client";

      reactor::wait(listening);
      reactor::network::TCPSocket socket("127.0.0.1", port);
      infinit::protocol::Serializer serializer(*reactor::Scheduler::scheduler(),
                                               socket);
      infinit::protocol::ChanneledStream channels(
        *reactor::Scheduler::scheduler(), serializer);
      frete::RPCFrete rpcs(channels);

      ELLE_DEBUG("get symmetric key for transaction and decrypt it");
      auto key = infinit::cryptography::SecretKey(
        recipient_key_pair.k().decrypt<infinit::cryptography::SecretKey>(rpcs.key_code()));

      ELLE_DEBUG("Read the number of transaction");
      BOOST_CHECK_EQUAL(rpcs.count(), 4);
      {
        ELLE_DEBUG("Check the name of the first file");
        BOOST_CHECK_EQUAL(rpcs.path(0), "empty");
        ELLE_DEBUG("Check the size of the first file");
        BOOST_CHECK_EQUAL(rpcs.file_size(0), 0);
        ELLE_DEBUG("Read many time the same block")
          for (int i = 0; i < 3; ++i)
          {
            ELLE_DEBUG("read 1024 bytes from file 0 with an offset of 0");
            elle::Buffer buffer(
              key.decrypt<elle::Buffer>(rpcs.encrypted_read(0, 0, 1024)));
            BOOST_CHECK_EQUAL(buffer.size(), 0);
          }
        {
          auto buffer = key.decrypt<elle::Buffer>(rpcs.encrypted_read(0, 1, 1024));
          BOOST_CHECK_EQUAL(buffer.size(), 0);
        }
      }
      {
        BOOST_CHECK_EQUAL(rpcs.path(1), "content");
        BOOST_CHECK_EQUAL(rpcs.file_size(1), 8);
        {
          auto buffer = key.decrypt<elle::Buffer>(rpcs.encrypted_read(1, 0, 1024));
          BOOST_CHECK_EQUAL(buffer.size(), 8);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("content\n"));
        }
        {
          auto buffer = key.decrypt<elle::Buffer>(rpcs.encrypted_read(1, 2, 2));
          BOOST_CHECK_EQUAL(buffer.size(), 2);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("nt"));
        }
        {
          auto buffer = key.decrypt<elle::Buffer>(rpcs.encrypted_read(1, 7, 3));
          BOOST_CHECK_EQUAL(buffer.size(), 1);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("\n"));
        }
      }
      {
        if (rpcs.path(2) == "dir/2")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(2), 1);
          BOOST_CHECK_EQUAL(key.decrypt<elle::Buffer>(rpcs.encrypted_read(2, 0, 2)), elle::ConstWeakBuffer("2"));
        }
        else if (rpcs.path(2) == "dir/1")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(2), 1);
          BOOST_CHECK_EQUAL(key.decrypt<elle::Buffer>(rpcs.encrypted_read(2, 0, 2)), elle::ConstWeakBuffer("1"));
        }
        else if (rpcs.path(3) == "dir/2")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(3), 1);
          BOOST_CHECK_EQUAL(key.decrypt<elle::Buffer>(rpcs.encrypted_read(3, 0, 2)), elle::ConstWeakBuffer("2"));
        }
        else if (rpcs.path(3) == "dir/1")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(3), 1);
          BOOST_CHECK_EQUAL(key.decrypt<elle::Buffer>(rpcs.encrypted_read(3, 0, 2)), elle::ConstWeakBuffer("1"));
        }
        else
        {
          BOOST_FAIL("recipient files incorrect");
        }
      }
      BOOST_CHECK_THROW(rpcs.path(4), std::runtime_error);
      BOOST_CHECK_THROW(rpcs.encrypted_read(4, 0, 1), std::runtime_error);
      scope.terminate_now();
    });
    scope.wait();
  };
}


ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 15 : 3;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(connection), 0, timeout);
}
