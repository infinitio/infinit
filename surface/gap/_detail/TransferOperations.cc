#include <surface/gap/_detail/TransferOperations.hh>

#include <agent/Agent.hh>

#include <etoile/Etoile.hh>
#include <etoile/abstract/Object.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/wall/Access.hh>
#include <etoile/wall/Attributes.hh>
#include <etoile/wall/Directory.hh>
#include <etoile/wall/File.hh>
#include <etoile/wall/Group.hh>
#include <etoile/wall/Link.hh>
#include <etoile/wall/Object.hh>
#include <etoile/wall/Path.hh>

#include <nucleus/neutron/Entry.hh>
#include <nucleus/neutron/Permissions.hh>
#include <nucleus/neutron/Record.hh>
#include <nucleus/neutron/Subject.hh>
#include <nucleus/neutron/Trait.hh>

#include <lune/Descriptor.hh>

#include <elle/Exception.hh>
#include <elle/finally.hh>
#include <elle/log.hh>
#include <elle/system/system.hh>

#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap._detail.TransferOperations");

namespace surface
{
  namespace gap
  {
    namespace operation_detail
    {
      static
      etoile::gear::Identifier
      attach(etoile::Etoile& etoile,
             lune::Descriptor const& descriptor,
             nucleus::neutron::Subject const& subject,
             etoile::gear::Identifier const& object,
             std::string const& path)
      {
        ELLE_TRACE_SCOPE("%s: attach %s to %s", etoile, object, path);

        boost::filesystem::path p(path);
        std::string way = p.parent_path().generic_string();
        if (way.empty()) way = std::string(1, elle::system::path::separator);
        std::string name = p.filename().generic_string();

        ELLE_DEBUG("path: %s, way: %s, name: %s", p, way, name);

        // Resolve parent directory.
        etoile::path::Chemin chemin(etoile::wall::Path::resolve(etoile, way));

        // Load parent directory.
        etoile::gear::Identifier directory(
          etoile::wall::Directory::load(etoile, chemin));

        elle::Finally discard_directory{[&] ()
          {
            etoile::wall::Group::Discard(etoile, directory);
          }
        };

        // Retrieve the subject's permissions on the object.
        nucleus::neutron::Record record(
          etoile::wall::Access::lookup(etoile,
                                       directory,
                                       subject));

        ELLE_DEBUG("record: %s", record);
        // Check the record.
        if ((record == nucleus::neutron::Record::null()) ||
            ((record.permissions() & nucleus::neutron::permissions::write) !=
             nucleus::neutron::permissions::write))
          throw elle::Exception("the subject does not have the permission");

        // Grant permissions for the user itself.
        // Set default permissions: read and write.
        nucleus::neutron::Permissions permissions =
          nucleus::neutron::permissions::read |
          nucleus::neutron::permissions::write;

        // Set the owner permissions.
        etoile::wall::Access::Grant(etoile,
                                    object,
                                    subject,
                                    permissions);

        // Grant read permission for 'everybody' group.
        ELLE_ASSERT_EQ(descriptor.data().policy(), horizon::Policy::accessible);

        // grant the read permission to the 'everybody' group.
        etoile::wall::Access::Grant(etoile,
                                    object,
                                    descriptor.meta().everybody_subject(),
                                    nucleus::neutron::permissions::read);

        // Add object to parent directory.
        etoile::wall::Directory::add(etoile, directory, name, object);

        // Release the identifier tracking.
        discard_directory.abort();

        return (directory);
      }

      namespace to
      {
        static
        elle::Natural64
        symlink(etoile::Etoile& etoile,
                lune::Descriptor const& descriptor,
                nucleus::neutron::Subject const& subject,
                std::string const& source,
                std::string const& target)
        {
          ELLE_TRACE_SCOPE("%s: symlink from %s to %s", etoile, source, target);

          // Create link.
          etoile::gear::Identifier link(etoile::wall::Link::create(etoile));

          elle::Finally discard_link{[&] ()
            {
              etoile::wall::Link::Discard(etoile, link);
            }
          };

          // Attach the link to the hierarchy.
          etoile::gear::Identifier directory(attach(etoile,
                                                    descriptor,
                                                    subject,
                                                    link,
                                                    target));

          elle::Finally discard_directory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, directory);
            }
          };

          std::string way(boost::filesystem::read_symlink(source).string());

          // bind the link.
          etoile::wall::Link::bind(etoile, link, way);

          // Store link.
          etoile::wall::Link::store(etoile, link);

          // Release the identifier tracking.
          discard_link.abort();

          // Store parent directory.
          etoile::wall::Directory::store(etoile, directory);

          discard_directory.abort();

          return way.length();
        }

        static
        void
        store_size(etoile::Etoile& etoile,
                   elle::Natural64 const size)
        {
          ELLE_TRACE_SCOPE("%s: store size (%s)", etoile, size);

          std::string root(1, elle::system::path::separator);

          // Resolve the root directory.
          etoile::path::Chemin chemin(etoile::wall::Path::resolve(etoile,
                                                                  root));
          // Load the directory.
          etoile::gear::Identifier directory(
            etoile::wall::Directory::load(etoile, chemin));

          elle::Finally discard_directory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, directory);
            }
          };

          // Set the attribute.
          etoile::wall::Attributes::set(etoile,
                                        directory,
                                        "infinit:transfer:size",
                                        elle::sprint(size));

          // Store the directory.
          etoile::wall::Directory::store(etoile, directory);

          discard_directory.abort();
        }

        static
        elle::Natural64
        create(etoile::Etoile& etoile,
               lune::Descriptor const& descriptor,
               nucleus::neutron::Subject const& subject,
               std::string const& source,
               std::string const& target)
        {
          ELLE_TRACE_FUNCTION(source, target);

          // Create file.
          etoile::gear::Identifier file(etoile::wall::File::create(etoile));

          elle::Finally discard_file{[&] ()
            {
              etoile::wall::File::discard(etoile, file);
            }
          };

          // Attach the file to the hierarchy.
          etoile::gear::Identifier directory(attach(etoile,
                                                    descriptor,
                                                    subject,
                                                    file,
                                                    target));
          elle::Finally discard_directory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, directory);
            }
          };

          nucleus::neutron::Offset offset(0);

          // Write the source file's content into the Infinit file freshly created.
          std::streamsize N = 5242880;
          std::ifstream stream(source, std::ios::binary);
          elle::Buffer buffer(N);

          while (stream.good())
          {
            buffer.size(N);

            stream.read((char*)buffer.mutable_contents(), buffer.size());

            buffer.size(stream.gcount());

            etoile::wall::File::write(etoile, file, offset, buffer);

            offset += buffer.size();
          }

          stream.close();

          // Store file.
          etoile::wall::File::store(etoile, file);

          // Release the identifier tracking.
          discard_file.abort();

          // Store parent directory.
          etoile::wall::Directory::store(etoile, directory);

          discard_directory.abort();

          // Return the number of bytes composing the file having been copied.
          return (static_cast<elle::Natural64>(offset));
        }

        static
        elle::Natural64
        dig(etoile::Etoile& etoile,
            lune::Descriptor const& descriptor,
            nucleus::neutron::Subject const& subject,
            std::string const& path)
        {
          ELLE_TRACE_FUNCTION(path);

          // Create directory.
          etoile::gear::Identifier subdirectory(etoile::wall::Directory::create(etoile));

          elle::Finally discard_subdirectory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, subdirectory);
            }
          };

          // Attach the directory to the hierarchy.
          etoile::gear::Identifier directory(attach(etoile,
                                                    descriptor,
                                                    subject,
                                                    subdirectory,
                                                    path));

          elle::Finally discard_directory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, directory);
            }
          };

          // Store subdirectory.
          etoile::wall::Directory::store(etoile, subdirectory);

          // Release the identifier tracking.
          discard_subdirectory.abort();

          // Store parent directory.
          etoile::wall::Directory::store(etoile, directory);

          discard_directory.abort();

          // We consider that the directories do not account for the actual data but
          // for a single byte.
          return (1);
        }

        static
        elle::Natural64
        store(etoile::Etoile& etoile,
              lune::Descriptor const& descriptor,
              nucleus::neutron::Subject const& subject,
              std::string const& source)
        {
          ELLE_TRACE("%s: store %s on network", etoile, source);

          elle::Natural64 size = 0;

          boost::filesystem::path path(source);

          if (boost::filesystem::is_symlink(path) == true)
          {
            // Transfor a single link.
            std::string root(path.parent_path().string());
            std::string base(path.string().substr(root.length()));

            ELLE_DEBUG("root %s", root.c_str());
            ELLE_DEBUG("link %s", base.c_str());

            size += symlink(etoile, descriptor, subject,  source, base);
          }
          else if (boost::filesystem::is_directory(path) == true)
          {
            // Transfer a whole directory and its content.
            std::string root(path.parent_path().string());
            std::string base(path.string().substr(root.length()));

            ELLE_DEBUG("root %s", root.c_str());
            ELLE_DEBUG("base %s", base.c_str());

            boost::filesystem::recursive_directory_iterator iterator(source);
            boost::filesystem::recursive_directory_iterator end;

            size += dig(etoile, descriptor, subject, base);

            for (; iterator != end; ++iterator)
            {
              ELLE_DEBUG("path %s", iterator->path().string().c_str());

              if (boost::filesystem::is_symlink(iterator->path()) == true)
              {
                std::string link(
                  iterator->path().string().substr(root.length()));

                ELLE_DEBUG("link %s", link.c_str());

                size += symlink(etoile, descriptor, subject, iterator->path().string(), link);
              }
              else if (boost::filesystem::is_regular_file(
                         iterator->path()) == true)
              {
                std::string file(
                  iterator->path().string().substr(root.length()));

                ELLE_DEBUG("file %s", file.c_str());

                size += create(etoile, descriptor, subject, iterator->path().string(), file);
              }
              else if (boost::filesystem::is_directory(iterator->path()) == true)
              {
                std::string directory(
                  iterator->path().string().substr(root.length()));

                ELLE_DEBUG("directory %s", directory.c_str());

                size += dig(etoile, descriptor, subject, directory);
              }
              else
                throw elle::Exception("unknown object type");
            }
          }
          else if (boost::filesystem::is_regular_file(path) == true)
          {
            // Transfer a single file.
            std::string root(path.parent_path().string());
            std::string base(path.string().substr(root.length()));

            ELLE_DEBUG("root %s", root.c_str());
            ELLE_DEBUG("file %s", base.c_str());

            size += create(etoile, descriptor, subject, source, base);
          }
          else
            throw elle::Exception("unknown object type");

          return size;
        }

        void
        send(etoile::Etoile& etoile,
             lune::Descriptor const& descriptor,
             nucleus::neutron::Subject const& subject,
             std::unordered_set<std::string> items)
        {
          elle::Natural64 size = 0;
          for (auto const& item: items)
            size += store(etoile, descriptor, subject, boost::filesystem::canonical(item).string());

          store_size(etoile, size);
        }
      }

      namespace from
      {
        static
        elle::Natural64
        size(etoile::Etoile& etoile)
        {
          ELLE_TRACE_SCOPE("%s: get root size", etoile);
          // Resolve the path to the root directory.
          etoile::path::Chemin chemin(
            etoile::wall::Path::resolve(etoile,
                                        std::string(1, elle::system::path::separator)));

          // Load the root directory.
          etoile::gear::Identifier directory(
            etoile::wall::Directory::load(etoile, chemin));

          elle::Finally discard_directory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, directory);
            }
          };

          // Then, retrieve the size of the transfer.
          nucleus::neutron::Trait size_trait(
            etoile::wall::Attributes::get(etoile,
                                          directory,
                                          "infinit:transfer:size"));

          if (size_trait == nucleus::neutron::Trait::null())
            throw elle::Exception("no transfer size attribute present");

          // Set the size variable.
          elle::Natural64 _size =
            boost::lexical_cast<elle::Natural64>(size_trait.value());
          ELLE_ASSERT_NEQ(_size, 0u);

          ELLE_DEBUG("the 'size' attribute is '%s'", _size);

          // Discard the directory since unchanged.
          etoile::wall::Directory::discard(etoile, directory);

          discard_directory.abort();

          ELLE_TRACE("%s: size is %s", etoile, _size);
          return _size;
        }

        static
        etoile::path::Chemin
        setup(etoile::Etoile& etoile,
              lune::Descriptor const& descriptor,
              nucleus::neutron::Subject const& subject)
        {
          ELLE_TRACE_SCOPE("%s: setup", etoile);

          // The way to the progress-specific file. Note that this file does not
          // contain the progress in its data but in a specific attribute. This
          // has been done so as to speed up the process of updating the progress.
          //
          // Indeed, by setting an attribute, only the metadata of this file needs
          // to be retrieved while with the progress in the data, another block
          // would need to be retrieved.
          //
          // Note that the progress attribute could not be set in the root directory
          // because the root directory belongs to the user having transferred the
          // data. The user retrieving it has to create an object he owns so as to
          // set an attribute.
          std::string root(std::string(1, elle::system::path::separator) +
                           ".progress");

          // Create a file for the progress.
          etoile::gear::Identifier file(etoile::wall::File::create(etoile));

          elle::Finally discard_file{[&] ()
            {
              etoile::wall::File::discard(etoile, file);
            }
          };

          // Attach the file to the hierarchy.
          etoile::gear::Identifier directory(attach(etoile, descriptor, subject, file, root));

          elle::Finally discard_directory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, directory);
            }
          };

          // Set the initial attribute.
          etoile::wall::Attributes::set(etoile,file,
                                        "infinit:transfer:progress",
                                        "0");
          // XXX[Antony]: ???
          // // Set the progress to zero.
          // _progress = 0;

          // Store the file.
          etoile::wall::File::store(etoile, file);

          discard_file.abort();

          // Store parent directory.
          etoile::wall::Directory::store(etoile, directory);

          discard_directory.abort();

          return (etoile::wall::Path::resolve(etoile,root));
        }

        static
        elle::Natural64
        progress(etoile::Etoile& etoile,
                 lune::Descriptor const& descriptor,
                 etoile::path::Chemin const& chemin,
                 elle::Natural64 size,
                 elle::Natural64 _progress,
                 elle::Natural64 increment)
        {
          ELLE_TRACE_SCOPE("%s: progress (+%s)", etoile, increment);

          ELLE_ASSERT_NEQ(size, 0u);

          // The difference between the current progress and the last
          // one which has been pushed in the attributes. Once this
          // difference is reached, the attributes are updated.
          //
          // This is required so as to limit the number of updates while
          // ensuring a smooth progress.
          const elle::Real DIFFERENCE = 0.5;

          elle::Natural64 stale(_progress);

          // Increment the progress counter.
          _progress += increment;

          ELLE_DEBUG("size: %s, stale %s, increment %s, progress: %s (stale + increment)",
                     size, stale, increment, _progress);

          // Compute the increment in terms of pourcentage of progress.
          elle::Real difference = (_progress - stale) * 100 / (float) size;

          ELLE_DEBUG("difference %s", difference);

          // If the difference is large enough, update the progress in the root
          // directory's attribtues.
          if ((difference > DIFFERENCE) || (_progress == size))
          {
            // Load the progress file.
            etoile::gear::Identifier identifier(
              etoile::wall::File::load(etoile, chemin));

            elle::Finally discard_file{[&] ()
              {
                etoile::wall::File::discard(etoile, identifier);
              }
            };

            std::string string =
              boost::lexical_cast<std::string>(_progress);

            // Update the progress attribute.
            etoile::wall::Attributes::set(etoile,
                                          identifier,
                                          "infinit:transfer:progress",
                                          string);

            ELLE_DEBUG("update progress to '%s'", string);

            // Store the modifications.
            etoile::wall::File::store(etoile, identifier);

            discard_file.abort();

            // Update the stale progress which now is up-to-date.
            stale = _progress;
          }

          return _progress;
        }

        static
        void
        traverse(etoile::Etoile& etoile,
                 lune::Descriptor const& descriptor,
                 nucleus::neutron::Subject const& subject,
                 std::string const& source,
                 std::string const& target)
        {
          ELLE_TRACE_SCOPE("%s: traverse %s, %s", etoile, source, target);

          // Before everything else, force the creation of the progress file.
          //
          // XXX[note that this call could be removed if etoile auto-publish
          //     blocks which have remained for quite some time main memory]

          etoile::path::Chemin root_chemin(setup(etoile, descriptor, subject));

          // XXX: Chemin is the same of all progress.
          // Resolve the directory.
          etoile::path::Chemin chemin(etoile::wall::Path::resolve(etoile, source));
          elle::Natural64 _size(size(etoile));

          // XXX[Antony]: A bit strange, but to avoid keeping a static for
          // progress, we keep in track the previous value. Let's see if it works =).
          elle::Natural64 _progress = 0;
          _progress = progress(etoile,
                               descriptor,
                               root_chemin,
                               _size,
                               _progress,
                               0);

          // Load the directory.
          etoile::gear::Identifier directory(etoile::wall::Directory::load(etoile, chemin));

          elle::Finally discard_directory{[&] ()
            {
              etoile::wall::Directory::discard(etoile, directory);
            }
          };

          // Consult the directory.
          nucleus::neutron::Range<nucleus::neutron::Entry> entries(
            etoile::wall::Directory::consult(
              etoile,
              directory,
              0,
              std::numeric_limits<nucleus::neutron::Index>::max()));

          // Go through the entries.
          for (auto entry: entries)
          {
            std::string _source(
              source + elle::system::path::separator + entry->name());

            ELLE_DEBUG("source %s", _source);

            // Resolve the child.
            etoile::path::Chemin chemin(etoile::wall::Path::resolve(etoile, _source));

            // Load the child.
            etoile::gear::Identifier child(etoile::wall::Object::load(etoile, chemin));

            elle::Finally discard_child{[&] ()
              {
                etoile::wall::Object::discard(etoile, child);
              }
            };

            // Retrieve information on the child.
            etoile::abstract::Object abstract(
              etoile::wall::Object::information(etoile, child));

            std::string path(target + _source);

            switch (abstract.genre)
            {
              case nucleus::neutron::Genre::file:
              {
                // 1MB seems large enough for the performance to remain
                // good while ensuring a smooth progress i.e no jump from
                // 4% to 38% for reasonable large files.
                std::streamsize N = 1048576;
                nucleus::neutron::Offset offset(0);

                std::ofstream stream(path, std::ios::binary);

                ELLE_DEBUG("file %s", path.c_str());

                // Copy the file.
                while (offset < abstract.size)
                {
                  elle::Buffer data(
                    etoile::wall::File::read(etoile, child, offset, N));

                  stream.write((const char*)data.contents(),
                               static_cast<std::streamsize>(data.size()));

                  offset += data.size();

                  // Set the progress.
                  _progress = progress(etoile,
                                       descriptor,
                                       root_chemin,
                                       _size,
                                       _progress,
                                       data.size());
                }

                // Make sure the right amount has been copied.
                assert(offset == abstract.size);

                stream.close();

                // Discard the child.
                etoile::wall::Object::discard(etoile, child);

                discard_child.abort();

                break;
              }
              case nucleus::neutron::Genre::directory:
              {
                ELLE_DEBUG("directory %s", path.c_str());

                // Create the directory.
                if (boost::filesystem::create_directory(path) == false)
                  throw elle::Exception("unable to create the directory");

                // Set the progress.
                _progress = progress(etoile,
                                     descriptor,
                                     root_chemin,
                                     _size,
                                     _progress,
                                     1);

                // Recursively explore the Infinit network.
                traverse(etoile,
                         descriptor,
                         subject,
                         _source + elle::system::path::separator,
                         target);

                // Discard the child.
                etoile::wall::Object::discard(etoile, child);

                discard_child.abort();

                break;
              }
              case nucleus::neutron::Genre::link:
              {
                ELLE_DEBUG("link %s", path.c_str());

                // Resolve the link.
                std::string way(etoile::wall::Link::resolve(etoile, child));

                // Create the link.
                boost::filesystem::create_symlink(way, path);

                // Set the progress.
                _progress = progress(etoile,
                                     descriptor,
                                     root_chemin,
                                     _size,
                                     _progress,
                                     way.length());

                // Discard the child.
                etoile::wall::Object::discard(etoile, child);

                discard_child.abort();

                break;
              }
            }
          }

          // Discard the directory since no longer necessary.
          etoile::wall::Directory::discard(etoile, directory);

          discard_directory.abort();
        }

        void
        receive(etoile::Etoile& etoile,
                lune::Descriptor const& descriptor,
                nucleus::neutron::Subject const& subject,
                std::string const& target)
        {
          ELLE_TRACE_SCOPE("%s: receiving %s", etoile, target);

          // Traverse the Infinit network from the root.
          traverse(etoile,
                   descriptor,
                   subject,
                   std::string(1, elle::system::path::separator),
                   target);
        }
      }

      namespace progress
      {
        float
        progress(etoile::Etoile& etoile)
        {
          ELLE_TRACE("%s: progress", etoile);

          elle::Natural64 _size(0);
          elle::Natural64 _progress(0);

          // (1) Get the transfer size from the root directory.
          {
            // Resolve the path to the root directory.
            etoile::path::Chemin chemin(
              etoile::wall::Path::resolve(
                etoile,
                std::string(1, elle::system::path::separator)));

            // Load the root directory.
            etoile::gear::Identifier directory(
              etoile::wall::Directory::load(etoile, chemin));

            // Discard the guard at the end of the scope, don't abort this one.
            elle::Finally discard_directory{[&] ()
              {
                etoile::wall::Directory::discard(etoile, directory);
              }
            };

            // Then, retrieve the size of the transfer.
            nucleus::neutron::Trait size(
              etoile::wall::Attributes::get(etoile,
                                            directory,
                                            "infinit:transfer:size"));

            if (size == nucleus::neutron::Trait::null())
            {
              ELLE_DEBUG("no 'size' attribute present");

              throw elle::Exception("no transfer size attribute present");
            }

            ELLE_DEBUG("'size' attribute retrieved: %s", size.value());

            // Set the size variable.
            _size = boost::lexical_cast<elle::Natural64>(size.value());
          }

          // (2) Get the progress attribute from the specific file.
          {
            elle::String root(elle::String(1, elle::system::path::separator) +
                              ".progress");

            std::unique_ptr<etoile::path::Chemin> chemin{};

            try
            {
              // Resolve the file.
              chemin.reset(
                new etoile::path::Chemin(
                  etoile::wall::Path::resolve(etoile, root)));
            }
            catch (...)
            {
              ELLE_WARN("%s: no '.progress' file present", etoile);
              return 0.0f;
            }

            // Load the progress file.
            etoile::gear::Identifier identifier(
              etoile::wall::File::load(etoile, *chemin));

            // Discard the guard a the end of the scope, don't abort this one.
            elle::Finally discard_file{[&] ()
              {
                etoile::wall::File::discard(etoile, identifier);
              }
            };

            // Retrieve the progress and size attributes.
            nucleus::neutron::Trait progress(
              etoile::wall::Attributes::get(etoile,
                                            identifier,
                                            "infinit:transfer:progress"));

            if (progress == nucleus::neutron::Trait::null())
            {
              ELLE_DEBUG("no 'progress' attribute retrieved");

              return 0.0f;
            }

            ELLE_DEBUG("'progress' attribute retrieved: %s", progress.value());

            // Set the progress variable.
            _progress = boost::lexical_cast<elle::Natural64>(progress.value());
          }

          float ratio = ((float) _progress / (float) _size);
          ELLE_TRACE("%s: progress is %s", etoile, ratio);
          return ratio;
        }
      }
    }
  }
}
