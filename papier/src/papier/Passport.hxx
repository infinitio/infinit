#ifndef PAPIER_PASSPORT_HXX
# define PAPIER_PASSPORT_HXX

ELLE_SERIALIZE_SIMPLE(papier::Passport,
                      archive,
                      value,
                      version)
{
  enforce(version == 0);

  archive & value._id;
  archive & value._name;
  archive & value._owner_K;
  archive & value._signature;
}

#endif
