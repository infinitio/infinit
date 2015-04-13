#include <boost/filesystem/fstream.hpp>

#include <elle/Buffer.hh>
#include <elle/filesystem/TemporaryFile.hh>
#include <elle/finally.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <reactor/Barrier.hh>
#include <reactor/Scope.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/scheduler.hh>

#include <cryptography/Code.hh>
#include <cryptography/KeyPair.hh>
#include <cryptography/Output.hh>

#include <protocol/ChanneledStream.hh>
#include <protocol/Serializer.hh>

#include <frete/Frete.hh>
#include <frete/RPCFrete.hh>

ELLE_LOG_COMPONENT("frete.tests");

// Frete path are normalized.
# define SEPARATOR "/"

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

class DummyHierarchy
{
public:
  DummyHierarchy():
    _root("frete/tests/fs-connection"),
    _empty(this->_root / "empty"),
    _content(this->_root / "content"),
    _filename_with_whitespace(this->_root / "filename with whitespace"),
    _filename_with_utf8(this->_root / "é.lol"),
    _dir(this->_root / "dir")
  {
    if (boost::filesystem::exists(this->_root))
      this->_clear_root();
    boost::filesystem::create_directory(this->_root);
    boost::filesystem::ofstream(this->_empty, std::ios::binary);
    {
      boost::filesystem::ofstream f(this->_content, std::ios::binary);
      f << "content\n";
    }
    boost::filesystem::create_directory(this->_dir);
    {
      boost::filesystem::ofstream f(this->_dir / "1", std::ios::binary);
      f << "1";
    }
    boost::filesystem::create_directory(this->_dir);
    {
      boost::filesystem::ofstream f(this->_dir / "2", std::ios::binary);
      f << "2";
    }
    {
      boost::filesystem::ofstream f(this->_filename_with_whitespace, std::ios::binary);
      f << "stuff\n";
    }
    {
      boost::filesystem::ofstream f(this->_filename_with_utf8, std::ios::binary);
      f << "stuff again\n";
    }
  }

  ~DummyHierarchy()
  {
    this->_clear_root();
  }

  ELLE_ATTRIBUTE_R(boost::filesystem::path, root);
  ELLE_ATTRIBUTE_R(boost::filesystem::path, empty);
  ELLE_ATTRIBUTE_R(boost::filesystem::path, content);
  ELLE_ATTRIBUTE_R(boost::filesystem::path, filename_with_whitespace);
  ELLE_ATTRIBUTE_R(boost::filesystem::path, filename_with_utf8);
  ELLE_ATTRIBUTE_R(boost::filesystem::path, dir);

private:
  void
  _clear_root()
  {
    try
    {
      boost::filesystem::remove_all(this->_root);
    }
    catch (...)
    {};
  }
};

ELLE_TEST_SCHEDULED(connection)
{
  auto recipient_key_pair = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);
  auto sender_key_pair = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);
  int port = 0;
  reactor::Barrier listening;

  // Create dummy test fs.
  DummyHierarchy hierarchy;
  auto root = hierarchy.root();

  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("server", [&]
                         {
      auto snap = root;
      snap += "server";
      try
      {
        boost::filesystem::remove(snap);
      }
      catch (...) {}
      reactor::network::TCPServer server{};
      server.listen();
      port = server.port();
      listening.open();
      std::unique_ptr<reactor::network::Socket> socket(server.accept());
      infinit::protocol::Serializer serializer(
        *reactor::Scheduler::scheduler(), *socket);
      infinit::protocol::ChanneledStream channels(
        *reactor::Scheduler::scheduler(), serializer);
      frete::Frete frete("suce",
                         sender_key_pair,
                         snap,
                         "",
                         false);
      frete.set_peer_key(recipient_key_pair.K());
      frete::RPCFrete rpcs(frete, channels);
      frete.add(hierarchy.empty());
      frete.add(hierarchy.content());
      frete.add(hierarchy.dir());
      frete.add(hierarchy.filename_with_whitespace());
      frete.add(hierarchy.filename_with_utf8());
      rpcs.run();
      reactor::sleep();
    });

    scope.run_background("client", [&]
    {
      auto snap = root;
      snap += "client";
      try
      {
        boost::filesystem::remove(snap);
      }
      catch (...) {}
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
      auto infos = rpcs.transfer_info();
      BOOST_CHECK_EQUAL(rpcs.count(), infos.count());
      BOOST_CHECK_EQUAL(rpcs.files_info(), infos.files_info());
      BOOST_CHECK_EQUAL(rpcs.full_size(), infos.full_size());
      BOOST_CHECK_EQUAL(rpcs.count(), 6);
      {
        ELLE_DEBUG("Check the name of the first file");
        BOOST_CHECK_EQUAL(rpcs.path(0), "empty");
        ELLE_DEBUG("Check the size of the first file");
        BOOST_CHECK_EQUAL(rpcs.file_size(0), 0);
        ELLE_DEBUG("read many time the same block")
          for (int i = 0; i < 3; ++i)
          {
            ELLE_DEBUG("read 1024 bytes from file 0 with an offset of 0");
            elle::Buffer buffer(
              key.legacy_decrypt_buffer(rpcs.encrypted_read(0, 0, 1024)));
            BOOST_CHECK_EQUAL(buffer.size(), 0);
          }
      }
      ELLE_DEBUG("check different parts of /content")
      {
        BOOST_CHECK_EQUAL(rpcs.path(1), "content");
        BOOST_CHECK_EQUAL(rpcs.file_size(1), 8);
        {
          auto buffer = key.legacy_decrypt_buffer(rpcs.encrypted_read(1, 0, 1024));
          BOOST_CHECK_EQUAL(buffer.size(), 8);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("content\n"));
        }
        {
          auto buffer = key.legacy_decrypt_buffer(rpcs.encrypted_read(1, 2, 2));
          BOOST_CHECK_EQUAL(buffer.size(), 2);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("nt"));
        }
        {
          auto buffer = key.legacy_decrypt_buffer(rpcs.encrypted_read(1, 7, 3));
          BOOST_CHECK_EQUAL(buffer.size(), 1);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("\n"));
        }
      }
      ELLE_DEBUG("check dir/1 and dir/2")
      {
        if (rpcs.path(2) == "dir" SEPARATOR "2")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(2), 1);
          BOOST_CHECK_EQUAL(key.legacy_decrypt_buffer(rpcs.encrypted_read(2, 0, 2)), elle::ConstWeakBuffer("2"));
        }
        else if (rpcs.path(2) == "dir" SEPARATOR "1")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(2), 1);
          BOOST_CHECK_EQUAL(key.legacy_decrypt_buffer(rpcs.encrypted_read(2, 0, 2)), elle::ConstWeakBuffer("1"));
        }
        else if (rpcs.path(3) == "dir" SEPARATOR "2")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(3), 1);
          BOOST_CHECK_EQUAL(key.legacy_decrypt_buffer(rpcs.encrypted_read(3, 0, 2)), elle::ConstWeakBuffer("2"));
        }
        else if (rpcs.path(3) == "dir" SEPARATOR "1")
        {
          BOOST_CHECK_EQUAL(rpcs.file_size(3), 1);
          BOOST_CHECK_EQUAL(key.legacy_decrypt_buffer(rpcs.encrypted_read(3, 0, 2)), elle::ConstWeakBuffer("1"));
        }
        else
        {
          std::cerr << rpcs.path(3) << std::endl;
          BOOST_FAIL("recipient files incorrect");
        }
      }
      ELLE_DEBUG("check if whitespaces work")
      {
        BOOST_CHECK_EQUAL(rpcs.path(4), "filename with whitespace");
        BOOST_CHECK_EQUAL(rpcs.file_size(4), 6);
        {
          auto buffer = key.legacy_decrypt_buffer(rpcs.encrypted_read(4, 0, 1024));
          BOOST_CHECK_EQUAL(buffer.size(), 6);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("stuff\n"));
        }
      }
      ELLE_DEBUG("check if utf8 work")
      {
        BOOST_CHECK_EQUAL(rpcs.path(5), "é.lol");
        BOOST_CHECK_EQUAL(rpcs.file_size(5), 12);
        {
          auto buffer = key.legacy_decrypt_buffer(rpcs.encrypted_read(5, 0, 1024));
          BOOST_CHECK_EQUAL(buffer.size(), 12);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("stuff again\n"));
        }
      }
      ELLE_DEBUG("check errors")
      {
        BOOST_CHECK_THROW(rpcs.path(6), std::runtime_error);
        BOOST_CHECK_THROW(rpcs.encrypted_read(6, 0, 1), std::runtime_error);
      }
      scope.terminate_now();
    });
    scope.wait();
  };
}

ELLE_TEST_SCHEDULED(invalid_snapshot)
{
  auto keys = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);
  elle::filesystem::TemporaryFile f("frete.snapshot");
  {
    boost::filesystem::ofstream output(f.path());
    output <<
      "{"
      "  \"transfers\": [],"
      "  \"count\": 0,"
      "  \"total_size\": 0,"
      "  \"progress\": 0,"
      "  \"key_code\": { \"data\": \"AAAAAFxGRlxGRlxGRlxGRmxvbGlsb2wK\" },"
      "  \"archived\": false"
      "}";

  }
  frete::Frete frete("password", keys, f.path(), "", false);
  auto peer_keys = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);
  frete.set_peer_key(peer_keys.K());
  frete.key_code();
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(20);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(connection), 0, timeout);
  suite.add(BOOST_TEST_CASE(invalid_snapshot), 0, timeout);
}
