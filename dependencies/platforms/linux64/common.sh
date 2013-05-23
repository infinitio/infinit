# extension
PLATFORM_LIBRARY_EXTENSION="so"
set_rpath()
{
  echo patchelf --set-rpath $1 $2
  patchelf --set-rpath $1 $2
}
