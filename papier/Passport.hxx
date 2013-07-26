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

namespace std
{
  template<>
  struct hash<papier::Passport>
  {
  public:
    std::size_t operator()(papier::Passport const& s) const
    {
      // XXX: The id is definitely not unique.
      return std::hash<std::string>()(s.id());
    }
  };
}

#endif
