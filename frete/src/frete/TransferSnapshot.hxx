#ifndef FRETE_TRANSFERSNAPSHOT_HXX
# define FRETE_TRANSFERSNAPSHOT_HXX

# include <elle/serialize/MapSerializer.hxx>
# include <elle/serialize/construct.hh>
# include <elle/serialize/extract.hh>

# include <frete/TransferSnapshot.hh>

ELLE_SERIALIZE_SIMPLE(frete::TransferSnapshot::File,
                      ar,
                      res,
                      version)
{
  enforce(version == 0);
  ar & named("file_id", res._file_id);
  ar & named("root", res._root);
  ar & named("path", res._path);
  ar & named("file_size", res._size);
  if (ar.mode == ArchiveMode::input)
    res._full_path = boost::filesystem::path(res._root) / res._path;
  ar & named("progress", res._progress);
}

ELLE_SERIALIZE_STATIC_FORMAT(frete::TransferSnapshot , 1);


ELLE_SERIALIZE_SIMPLE(frete::TransferSnapshot,
                      ar,
                      res,
                      version)
{
  ar & named("transfers", res._files);
  ar & named("count", res._count);
  ar & named("total_size", res._total_size);
  ar & named("progress", res._progress);
  if (version >= 1)
  {
    if (ar.mode == ArchiveMode::output)
    {
      infinit::cryptography::Code empty;
      ar & named("key_code", res._key_code? *res._key_code : empty);
    }
    else
    {
      infinit::cryptography::Code key_code;
      ar & named("key_code", key_code);
      res._key_code.reset(new infinit::cryptography::Code(std::move(key_code)));
    }
  }
  // Progress is redundant data, don't trust it
  if (ar.mode == ArchiveMode::input)
    res._recompute_progress();
}

#endif
