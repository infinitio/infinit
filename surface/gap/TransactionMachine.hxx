#ifndef TRANSFERMACHINE_HXX
# define TRANSFERMACHINE_HXX

# include <elle/serialize/SetSerializer.hxx>

ELLE_SERIALIZE_SIMPLE(surface::gap::TransactionMachine::Snapshot, ar, res, version)
{
  enforce(version == 0);

  ar & named("data", res.data);
  ar & named("files", res.files);
  ar & named("state", res.state);
}

#endif
