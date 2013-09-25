#ifndef FRETE_FRETE_HXX
# define FRETE_FRETE_HXX

# include <frete/Frete.hh>

// ########## HXX ############
#include <elle/serialize/construct.hh>
#include <elle/serialize/extract.hh>

# include <elle/serialize/MapSerializer.hxx>

ELLE_SERIALIZE_SIMPLE(frete::Frete::TransferSnapshot::TransferInfo,
                      ar,
                      res,
                      version)
{
  enforce(version == 0);

  ar & named("file_id", res._file_id);
  ar & named("root", res._root);
  ar & named("path", res._path);
  ar & named("file_size", res._file_size);

  if (ar.mode == ArchiveMode::input)
    res._full_path = boost::filesystem::path(res._root) / res._path;
}

ELLE_SERIALIZE_SIMPLE(frete::Frete::TransferSnapshot::TransferProgressInfo,
                      ar,
                      res,
                      version)
{
  enforce(version == 0);

  ar & base_class<frete::Frete::TransferSnapshot::TransferInfo>(res);
  ar & named("progress", res._progress);
}

ELLE_SERIALIZE_SIMPLE(frete::Frete::TransferSnapshot,
                      ar,
                      res,
                      version)
{
  enforce(version == 0);

  ar & named("transfers", res._transfers);
  ar & named("count", res._count);
  ar & named("total_size", res._total_size);
  ar & named("progress", res._progress);
}

#endif
