#
# ---------- globals ----------------------------------------------------------
#

# general
PACKAGESDIR="${BASEDIR}/packages"
PLATFORMDIR="${BASEDIR}/platforms/${PLATFORM}"
BUILDDIR="${PLATFORMDIR}/build"
WORKDIR="${PLATFORMDIR}/work"
DEPENDENCIES="openssl curl"

#
# ---------- imports ----------------------------------------------------------
#

. "${PLATFORMDIR}/common.sh"

#
# ---------- packages ---------------------------------------------------------
#

# openssl
OPENSSL_VERSION="1.0.1c"
OPENSSL_BUILDDIR="openssl-${OPENSSL_VERSION}"
OPENSSL_NAME="openssl-${OPENSSL_VERSION}"
OPENSSL_TARBALL="${OPENSSL_NAME}.tar.gz"
OPENSSL_SNAPSHOT="http://www.openssl.org/source/${OPENSSL_TARBALL}"
OPENSSL_FINGERPRINT="ae412727c8c15b67880aef7bd2999b2e"
OPENSSL_LIBRARIES="${WORKDIR}/lib/libssl.${PLATFORM_LIBRARY_EXTENSION} ${WORKDIR}/lib/libcrypto.${PLATFORM_LIBRARY_EXTENSION}"

# curl
CURL_VERSION="7.30.0"
CURL_BUILDDIR="curl-${CURL_VERSION}"
CURL_NAME="curl-${CURL_VERSION}"
CURL_TARBALL="${CURL_NAME}.tar.gz"
CURL_SNAPSHOT="http://curl.haxx.se/download/${CURL_TARBALL}"
CURL_FINGERPRINT="60bb6ff558415b73ba2f00163fd307c5"
CURL_LIBRARIES="${WORKDIR}/lib/libcurl.${PLATFORM_LIBRARY_EXTENSION}"

#
# ---------- functions --------------------------------------------------------
#

die()
{
    echo "[!] ${1}" 1>&2
    exit 1
}

download()
{
    snapshot="${1}"
    fingerprint="${2}"
    tarball="${3}"

    TRIES=3

    for i in $(seq 0 ${TRIES}) ; do
        if [ ! -f "${tarball}" ] ; then
            wget "${snapshot}" -O "${tarball}" ||
            die "unable to download the snapshot"
        fi

        checksum=$(openssl dgst -md5 "${tarball}" | cut -d' ' -f2)

        test "${fingerprint}" = "${checksum}" &&
        return

        rm -f "${tarball}"
    done

    die "the checksum differs for '${tarball}'"
}

uptodate()
{
    libraries="${1}"

    for library in ${libraries} ; do
	echo ${library}
        if [ ! -f "${library}" ] ; then
            return 1
        fi
    done

    return 0
}
