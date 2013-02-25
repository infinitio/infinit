#ifndef ELLE_SERIALIZE_FWD_HH
# define ELLE_SERIALIZE_FWD_HH

# include <elle/serialize/ArchiveMode.hh>

namespace elle
{
  namespace serialize
  {

    typedef char DefaultCharType;
    template<ArchiveMode, typename>
    struct DefaultStreamTypeSelect;

    template<ArchiveMode mode>
    class BinaryArchive;
    typedef BinaryArchive<ArchiveMode::Output>      OutputBinaryArchive;
    typedef BinaryArchive<ArchiveMode::Input>       InputBinaryArchive;

    template<ArchiveMode mode>
    class Base64Archive;
    typedef Base64Archive<ArchiveMode::Output>      OutputBase64Archive;
    typedef Base64Archive<ArchiveMode::Input>       InputBase64Archive;

    template<ArchiveMode mode>
    class HexadecimalArchive;
    typedef HexadecimalArchive<ArchiveMode::Output> OutputHexadecimalArchive;
    typedef HexadecimalArchive<ArchiveMode::Input>  InputHexadecimalArchive;

    template<ArchiveMode mode>
    class JSONArchive;
    typedef JSONArchive<ArchiveMode::input>         InputJSONArchive;
    typedef JSONArchive<ArchiveMode::output>        OutputJSONArchive;

    template<typename T>
    class Serializer;

    template <template<ArchiveMode> class Archive>
    struct _Serializable;
  }
}

///
/// Allow full access to the serializer of the class T.
/// -----------------
/// class MyClass
/// {
/// private:
///   int     _my_int;
///
///   // The serializer has access to _my_int;
///   ELLE_SERIALIZE_FRIEND_FOR(MyClass);
/// };
/// -----------------
///
#define ELLE_SERIALIZE_FRIEND_FOR(T)                      \
  friend struct elle::serialize::Serializer<T>     \

#endif
