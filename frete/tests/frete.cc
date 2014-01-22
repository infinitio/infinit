#define BOOST_TEST_MODULE Frete
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/fstream.hpp>

#include <reactor/Barrier.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/scheduler.hh>

#include <cryptography/KeyPair.hh>

#include <protocol/Serializer.hh>
#include <protocol/ChanneledStream.hh>

# include <frete/Frete.hh>

# include <elle/log.hh>
# include <elle/finally.hh>

ELLE_LOG_COMPONENT("test.frete");

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

BOOST_AUTO_TEST_CASE(eligible_names)
{
  boost::filesystem::path root("frete/tests/eligible_name");
  auto clear_root =
    [&] {try { boost::filesystem::remove_all(root); } catch (...) {}};

  if (boost::filesystem::exists(root))
    clear_root();

  boost::filesystem::create_directory(root);
  elle::SafeFinally rm_root( [&] () { clear_root(); } );

  auto filename = root / "dest.tar.bz";

  static const size_t count = 32;

  bool first_pass = true;
  for (std::string  pattern: {" (%s)", "_%s", " - %s"})
  {
    for (size_t i = 0; i < count; ++i)
    {
      // Touch the file.
      boost::filesystem::ofstream{frete::Frete::eligible_name(filename, pattern)};
    }

    BOOST_CHECK(boost::filesystem::exists(filename));

    auto max = first_pass ? (count + 1) : (count + 2);
    for (size_t i = 2; i < max; ++i)
    {
      auto name = "frete/tests/eligible_name/dest" + pattern + ".tar.bz";
      BOOST_CHECK(
        boost::filesystem::exists(
          elle::sprintf(name, i)));
    }

    boost::filesystem::directory_iterator end;
    for (auto it = boost::filesystem::directory_iterator(root);
         it != end;
         ++it)
    {
      ELLE_DEBUG("- %s", *it);
    }

    ELLE_DEBUG("%s", std::string(32, '-'));

    first_pass = false;
  }
}

BOOST_AUTO_TEST_CASE(trim)
{
  boost::filesystem::path tmp{"/tmp"};
  boost::filesystem::path folder{"/tmp/folder"};
  boost::filesystem::path folder2{"/tmp/folder/folder"};
  boost::filesystem::path file{"/tmp/file.ext"};
  boost::filesystem::path file_into_folder {"/tmp/folder/file.ext"};
  boost::filesystem::path dotfolder {"/tmp/folder.com"};
  boost::filesystem::path file_into_dotfolder {"/tmp/folder.com/file.ext"};

  BOOST_CHECK_EQUAL(frete::Frete::trim(tmp, tmp), "");
  BOOST_CHECK_EQUAL(frete::Frete::trim(folder, tmp), "folder");
  BOOST_CHECK_EQUAL(frete::Frete::trim(folder2, tmp), "folder/folder");
  BOOST_CHECK_EQUAL(frete::Frete::trim(folder2, folder), "folder");
  BOOST_CHECK_EQUAL(frete::Frete::trim(file, tmp), "file.ext");
  BOOST_CHECK_EQUAL(frete::Frete::trim(file_into_folder, tmp),
                    "folder/file.ext");
  BOOST_CHECK_EQUAL(frete::Frete::trim(file_into_folder, folder), "file.ext");
  BOOST_CHECK_EQUAL(frete::Frete::trim(dotfolder, tmp), "folder.com");
  BOOST_CHECK_EQUAL(frete::Frete::trim(file_into_dotfolder, tmp),
                    "folder.com/file.ext");
  BOOST_CHECK_EQUAL(frete::Frete::trim(file_into_dotfolder, dotfolder),
                    "file.ext");

  BOOST_CHECK_THROW(
    frete::Frete::trim(file_into_folder, "folder_that_doesn_t_exist"),
    elle::Exception);

}

BOOST_AUTO_TEST_CASE(connection)
{
  auto recipient_key_pair = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);
  reactor::Scheduler sched;

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

  reactor::Thread server(
    sched, "server",
    [&]
    {
      auto snap = root;
      snap += "server";

      reactor::network::TCPServer server(sched);
      server.listen();
      port = server.port();
      listening.open();
      std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
      infinit::protocol::Serializer serializer(sched, *socket);
      infinit::protocol::ChanneledStream channels(sched, serializer);
      frete::Frete frete(channels, "suce", recipient_key_pair.K(), snap);
      frete.add(empty);
      frete.add(content);
      frete.add(dir);

      frete.run();
      while (true)
        reactor::sleep(10_sec);
    });

  reactor::Thread client(
    sched, "client",
    [&]
    {
      auto snap = root;
      snap += "client";

      listening.wait();
      reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
      infinit::protocol::Serializer serializer(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, serializer);
      frete::Frete frete(channels, "suce", recipient_key_pair.k(), snap);
      BOOST_CHECK_EQUAL(frete.count(), 4);
      {
        BOOST_CHECK_EQUAL(frete.path(0), "empty");
        BOOST_CHECK_EQUAL(frete.file_size(0), 0);
        for (int i = 0; i < 3; ++i)
        {
          auto buffer = frete.read(0, 0, 1024);
          BOOST_CHECK_EQUAL(buffer.size(), 0);
        }
        {
          auto buffer = frete.read(0, 1, 1024);
          BOOST_CHECK_EQUAL(buffer.size(), 0);
        }
      }
      {
        BOOST_CHECK_EQUAL(frete.path(1), "content");
        BOOST_CHECK_EQUAL(frete.file_size(1), 8);
        {
          auto buffer = frete.read(1, 0, 1024);
          BOOST_CHECK_EQUAL(buffer.size(), 8);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("content\n"));
        }
        {
          auto buffer = frete.read(1, 2, 2);
          BOOST_CHECK_EQUAL(buffer.size(), 2);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("nt"));
        }
        {
          auto buffer = frete.read(1, 7, 3);
          BOOST_CHECK_EQUAL(buffer.size(), 1);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("\n"));
        }
      }
      {
        BOOST_CHECK_EQUAL(frete.path(2), "dir/2");
        BOOST_CHECK_EQUAL(frete.file_size(2), 1);
        BOOST_CHECK_EQUAL(frete.read(2, 0, 2), elle::ConstWeakBuffer("2"));
        BOOST_CHECK_EQUAL(frete.path(3), "dir/1");
        BOOST_CHECK_EQUAL(frete.file_size(3), 1);
        BOOST_CHECK_EQUAL(frete.read(3, 0, 2), elle::ConstWeakBuffer("1"));
      }
      BOOST_CHECK_THROW(frete.path(4), std::runtime_error);
      BOOST_CHECK_THROW(frete.read(4, 0, 1), std::runtime_error);
      server.terminate();
    });

  sched.run();
}

BOOST_AUTO_TEST_CASE(one_function_get)
{
  auto recipient_key_pair = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);
  reactor::Scheduler sched;

  int port = 0;

  reactor::Barrier listening;

  // Create dummy test fs.
  boost::filesystem::path root("frete/tests/fs-get");
  boost::filesystem::path dest("frete/tests/fs-get-dest");

  auto clear_root =
    [&] {try { boost::filesystem::remove_all(root); } catch (...) {}};
  auto clear_dest =
    [&] {try { boost::filesystem::remove_all(dest); } catch (...) {}};

  if (boost::filesystem::exists(root))
    clear_root();

  boost::filesystem::create_directory(root);
  // elle::SafeFinally rm_root( [&] () { clear_root(); } );

  if (boost::filesystem::exists(dest))
    clear_dest();

  boost::filesystem::create_directory(dest);
  // elle::SafeFinally rm_dest( [&] () { clear_dest(); } );

  boost::filesystem::path empty(root / "empty");
  boost::filesystem::ofstream{empty};

  boost::filesystem::path content(root / "content");
  {
    boost::filesystem::ofstream f(content);
    f << std::string(52, 'b') << "\n" << "queue" << "\n" << std::flush;
  }

  boost::filesystem::path dir(root / "dir");
  boost::filesystem::create_directory(dir);
  {
    boost::filesystem::ofstream f((dir / "1"));
    f << "1" << std::flush;
  }
  boost::filesystem::create_directory(dir);
  {
    boost::filesystem::ofstream f((dir / "2"));
    f << "2" << std::flush;
  }
  boost::filesystem::path subdir(dir / "subdir");
  boost::filesystem::create_directory(subdir);
  {
    boost::filesystem::ofstream f((subdir / "1"));
    f << "1" << std::flush;
  }

  reactor::Thread server(
    sched, "server",
    [&]
    {
      auto snap = root;
      snap += "transfer";

      reactor::network::TCPServer server(sched);
      server.listen();
      port = server.port();
      listening.open();
      std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
      infinit::protocol::Serializer serializer(sched, *socket);
      infinit::protocol::ChanneledStream channels(sched, serializer);
      frete::Frete frete(channels, "suce", recipient_key_pair.K(), snap);
      frete.add(empty);
      frete.add(content);
      frete.add(dir); // Will add subdir.
      frete.run();
    });

  reactor::Thread client(
    sched, "client",
    [&]
    {
      auto snap = dest;
      snap += "transfer";

      listening.wait();
      reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
      infinit::protocol::Serializer serializer(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, serializer);
      frete::Frete frete(channels, "suce", recipient_key_pair.k(), snap);
      BOOST_CHECK_EQUAL(frete.count(), 5);
      {
        frete.get(dest);
      }

      BOOST_CHECK(boost::filesystem::exists(dest / "empty") &&
                  boost::filesystem::is_regular_file(dest / "empty"));
      BOOST_CHECK(boost::filesystem::exists(dest / "content") &&
                  boost::filesystem::is_regular_file(dest / "content"));
      BOOST_CHECK(boost::filesystem::exists(dest / "dir") &&
                  boost::filesystem::is_directory(dest / "dir"));
      BOOST_CHECK(
        boost::filesystem::exists(dest / "dir" / "1") &&
        boost::filesystem::is_regular_file(dest / "dir" / "1"));
      BOOST_CHECK(
        boost::filesystem::exists(dest / "dir" / "2") &&
        boost::filesystem::is_regular_file(dest / "dir" / "2"));
      BOOST_CHECK(
        boost::filesystem::exists(dest / "dir" / "subdir") &&
        boost::filesystem::is_directory(dest / "dir" / "subdir"));
      BOOST_CHECK(
        boost::filesystem::exists(dest / "dir" / "subdir" / "1") &&
        boost::filesystem::is_regular_file(dest / "dir" / "subdir" / "1"));

      BOOST_CHECK(compare_files(dest / "empty", root / "empty"));
      BOOST_CHECK(compare_files(dest / "content", root / "content"));
      BOOST_CHECK(compare_files(dest / "dir" / "1", root / "dir" / "1"));
      BOOST_CHECK(compare_files(dest / "dir" / "2", root / "dir" / "2"));
      BOOST_CHECK(compare_files(dest / "dir" / "subdir" / "1",
                                root / "dir" / "subdir" / "1"));

      server.terminate();
    });

  sched.run();
}

BOOST_AUTO_TEST_CASE(recipient_disconnection)
{
  auto recipient_key_pair = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);
  reactor::Scheduler sched;

  int port = 0;

  reactor::Barrier listening;

  // Create dummy test fs.
  boost::filesystem::path root("frete/tests/fs-recipient-disconnection");
  boost::filesystem::path dest("frete/tests/fs-recipient-disconnection-dest");

  auto clear_root =
    [&] { try { boost::filesystem::remove_all(root); } catch (...) {} };
  auto clear_dest =
    [&] { try { boost::filesystem::remove_all(dest); } catch (...) {} };

  if (boost::filesystem::exists(root))
    clear_root();

  boost::filesystem::create_directory(root);
  elle::SafeFinally rm_root( [&] () { clear_root(); } );

  if (boost::filesystem::exists(dest))
    clear_dest();

  boost::filesystem::create_directory(dest);
  elle::SafeFinally rm_dest( [&] () { clear_dest(); } );

  size_t block_size = 512 * 1024;
  size_t block_count = 8;

  boost::filesystem::path file(root / "file");
  {
    boost::filesystem::ofstream ofile(file);
    ofile << std::string(block_count * block_size, 'b');
  }
  boost::filesystem::path empty(root / "empty");
  {
    boost::filesystem::ofstream ofile(empty);
  }
  boost::filesystem::path folder = root / "folder";
  boost::filesystem::create_directories(folder);

  boost::filesystem::path empty2(folder / "empty");
  {
    boost::filesystem::ofstream ofile(empty2);
  }

  boost::filesystem::path file2(folder / "file");
  {
    boost::filesystem::ofstream ofile(file2);
    ofile << std::string(block_count * block_size, 'b');
  }

  reactor::Thread server(
    sched, "server",
    [&]
    {
      reactor::network::TCPServer server(sched);

      auto snap = root;
      snap /= ".transfer";

      while (true)
      {
        try
        {
          server.listen();
          port = server.port();

          listening.open();
          std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
          infinit::protocol::Serializer serializer(sched, *socket);
          infinit::protocol::ChanneledStream channels(sched, serializer);
          frete::Frete frete(channels, "suce", recipient_key_pair.K(), snap);

          elle::SafeFinally progress([&]
                                     {
                                       ELLE_DEBUG("progress %s", frete.progress());
                                     });

          frete.add(file);
          // frete.add(empty);
          // frete.add(folder);
          frete.add(file2);

          frete.run();
        }
        catch (reactor::Terminate const&)
        {
          throw;
        }
        catch (elle::AssertError const&)
        {
          ELLE_ERR("assert");
          throw;
        }
        catch (std::exception const&)
        {
          listening.close();
        }
        catch (...)
        {
          ELLE_ERR("%s", elle::exception_string());
          throw;
        }

      }
    });

  reactor::Thread client(
    sched, "clients",
    [&]
    {
      auto snap = dest;
      snap /= ".transfer";

      elle::SafeFinally compare{
        [&]
        {
          BOOST_CHECK(compare_files(dest / "file",
                                    root / "file"));
          BOOST_CHECK(compare_files(dest / "file (2)",
                                    root / "folder" / "file",
                                    false));
          // BOOST_CHECK(compare_files(dest / "empty",
          //                           root / "empty"));
          // BOOST_CHECK(compare_files(dest / "folder" / "empty",
          //                           root / "folder" / "empty"));
          // BOOST_CHECK(compare_files(dest / "folder" / "file2",
          //                           root / "folder" / "file2"));
          // BOOST_CHECK(compare_files(dest / "folder" / "file2 (2)",
          //                           root / "folder" / "file2"));

        }
      };

      elle::SafeFinally kill_server{ [&] { server.terminate(); } };

      while (true)
      {
        listening.wait();

        bool finished{false};
        try
        {
          reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
          infinit::protocol::Serializer serializer(sched, socket);
          infinit::protocol::ChanneledStream channels(sched, serializer);
          frete::Frete frete(channels, "suce", recipient_key_pair.k(), snap);

          elle::SafeFinally progress([&]
                                     {
                                       ELLE_WARN("progress %s", frete.progress());
                                     });

          elle::SafeFinally close_listening([&] { listening.close(); });

          elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
          {
            scope.run_background(
              "get",
              [&]
              {
                try
                {
                  frete.get(dest);
                  finished = true;
                  ELLE_DEBUG("%s", "transfer finished");
                  frete.progress_changed().signal();
                }
                catch (reactor::Terminate const&)
                {
                  throw;
                }
                catch (elle::AssertError const&)
                {
                  ELLE_ERR("Assert");
                  throw;
                }
                catch (...)
                {
                  ELLE_ERR("%s", elle::exception_string());
                  throw;
                }
              });

            scope.run_background(
              "client run",
              [&]
              {
                frete.run();
              });

            frete.progress_changed().wait();
          };
        }
        catch (reactor::Terminate const&)
        {
          throw;
        }
        catch (elle::AssertError const&)
        {
          ELLE_ERR("Assert");
          throw;
        }
        catch (std::exception const&)
        {
        }

        if (finished)
          break;
      }
    });

  sched.run();
}

BOOST_AUTO_TEST_CASE(sender_disconnection)
{
  auto recipient_key_pair = infinit::cryptography::KeyPair::generate(
    infinit::cryptography::Cryptosystem::rsa, 2048);

  reactor::Scheduler sched;

  int port = 0;

  reactor::Barrier listening;

  // Create dummy test fs.
  boost::filesystem::path root("frete/tests/fs-sender-disconnection");
  boost::filesystem::path dest("frete/tests/fs-sender-disconnection-dest");

  auto clear_root =
    [&] {try { boost::filesystem::remove_all(root); } catch (...) {}};
  auto clear_dest =
    [&] {try { boost::filesystem::remove_all(dest); } catch (...) {}};

  if (boost::filesystem::exists(root))
    clear_root();

  boost::filesystem::create_directory(root);
  elle::SafeFinally rm_root( [&] () { clear_root(); } );

  if (boost::filesystem::exists(dest))
    clear_dest();

  boost::filesystem::create_directory(dest);
  elle::SafeFinally rm_dest( [&] () { clear_dest(); } );

  size_t block_size = 512 * 1024;
  size_t block_count = 8;

  boost::filesystem::path file(root / "file");
  {
    boost::filesystem::ofstream ofile(file);
    ofile << std::string(block_count * block_size, 'b');
  }
  boost::filesystem::path empty(root / "empty");
  {
    boost::filesystem::ofstream ofile(empty);
  }
  boost::filesystem::path folder = root / "folder";
  boost::filesystem::create_directories(folder);

  boost::filesystem::path empty2(folder / "empty");
  {
    boost::filesystem::ofstream ofile(empty2);
  }

  boost::filesystem::path file2(folder / "file2");
  {
    boost::filesystem::ofstream ofile(file2);
    ofile << std::string(block_count * block_size, 'b');
  }

  reactor::Thread server(
    sched, "server",
    [&]
    {
      reactor::network::TCPServer server(sched);

      auto snap = root;
      snap += "transfer";

      while (true)
      {
        try
        {
          server.listen();
          port = server.port();
          listening.open();
          std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
          infinit::protocol::Serializer serializer(sched, *socket);
          infinit::protocol::ChanneledStream channels(sched, serializer);
          frete::Frete frete(channels, "suce", recipient_key_pair.K(), snap);

          frete.add(file);
          frete.add(empty);
          frete.add(folder);

          elle::SafeFinally progress([&]
                                     {
                                       ELLE_WARN("sen: %s", frete.progress());
                                     });

          elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
          {
            scope.run_background("server run", [&] { frete.run(); frete.progress_changed().signal(); });

            frete.progress_changed().wait();
            listening.close();
          };
        }
        catch (reactor::Terminate const&)
        {
          throw;
        }
        catch (elle::AssertError const&)
        {
          ELLE_ERR("Assert");
          throw;
        }
        catch (std::exception const&)
        {
          ELLE_DEBUG("client disconnected");
          listening.close();
        }
      }
    });

  reactor::Thread client(
    sched, "clients",
    [&]
    {
      auto snap = dest;
      snap += "transfer";

      elle::SafeFinally compare{
        [&]
        {
          BOOST_CHECK(compare_files(dest / "file",
                                    root / "file"));
          BOOST_CHECK(compare_files(dest / "empty",
                                    root / "empty"));
          BOOST_CHECK(compare_files(dest / "folder" / "empty",
                                    root / "folder" / "empty"));
          BOOST_CHECK(compare_files(dest / "folder" / "file2",
                                    root / "folder" / "file2"));
        }
      };

      elle::SafeFinally kill_server{ [&] { server.terminate(); } };

      while (true)
      {
        listening.wait();

        bool finished{false};
        try
        {
          reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
          infinit::protocol::Serializer serializer(sched, socket);
          infinit::protocol::ChanneledStream channels(sched, serializer);
          frete::Frete frete(channels, "suce", recipient_key_pair.k(), snap);

          elle::SafeFinally progress([&]
                                     {
                                       ELLE_WARN("rec: %s", frete.progress());
                                     });

          try
          {
            elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
            {
              scope.run_background(
                "get",
                [&]
                {
                  ELLE_ERR("start getting");
                  frete.get(dest);
                  if (frete.finished().opened())
                    throw elle::Exception("finished");
                });

              scope.run_background(
                "client run",
                [&]
                {
                  frete.run();
                });

              scope.wait();
            };
          }
          catch (reactor::Terminate const&)
          {
            throw;
          }
          catch (std::exception const&)
          {
          }

          finished = frete.finished().opened();
        }
        catch (reactor::Terminate const&)
        {
          throw;
        }
        catch (elle::AssertError const&)
        {
          ELLE_ERR("Assert");
          throw;
        }
        catch (std::exception const&)
        {
        }

        if (finished)
          break;
      }
    });

  sched.run();
}
