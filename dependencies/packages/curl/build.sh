#
# ---------- globals ----------------------------------------------------------
#

SCRIPT=`basename "${0}"`
ROOTDIR=`python -c "import os;print(os.path.abspath(os.path.dirname('$0')))"`

#
# ---------- imports ----------------------------------------------------------
#

BASEDIR="${ROOTDIR}/../.."

. "${BASEDIR}/environment.sh"
. "${BASEDIR}/common.sh"

#
# ---------- entry point ------------------------------------------------------
#

uptodate "${CURL_LIBRARIES}" ||
(
    rm -Rf "${BUILDDIR}/${CURL_NAME}"

    download "${CURL_SNAPSHOT}" "${CURL_FINGERPRINT}" "${BUILDDIR}/${CURL_TARBALL}"

    cd "${BUILDDIR}" ||
    die "unable to move to the build directory '${BUILDDIR}'"

    tar xzf "${CURL_TARBALL}" ||
    die "unable to extract the tarball"

    cd "${CURL_NAME}" ||
    die "unable to enter the directory"

    ./configure                                         \
        --prefix="${WORKDIR}"                           \
        --with-ssl="${WORKDIR}"                         \
        --enable-optimize                               \
        --enable-warnings                               \
	--enable-threaded-resolver			\
        --disable-ldap                                  \
        --disable-ldaps                                 \
        --disable-rtmp                                  \
        --disable-sspi                                  \
        --disable-ssh                                   \
        --disable-rtsp                                  \
        --without-libidn                                \
        --disable-rtmp || die "unable to configure"
    make all install || die "unable to build"
    for lib in ${WORKDIR}/lib/libcurl*
    do
      set_rpath \$ORIGIN $lib
    done
)
