#ifndef PLASMA_HERMES_CLERK_HXX
# define PLASMA_HERMES_CLERK_HXX

ELLE_SERIALIZE_SIMPLE(plasma::hermes::Chunk, archive, value, format)
{
  enforce(format == 0);

  archive & value._id;
  archive & value._off;
  archive & value._size;
}

#endif // !PLASMA_HERMES_CLERK_HXX
