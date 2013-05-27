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

uptodate "${STUN_LIBRARIES}" ||
(
    rm -Rf "${BUILDDIR}/${STUN_NAME}"

    download "${STUN_SNAPSHOT}" "${STUN_FINGERPRINT}" "${BUILDDIR}/${STUN_TARBALL}"

    cd "${BUILDDIR}" ||
    die "unable to move to the build directory '${BUILDDIR}'"

    tar xzf "${STUN_TARBALL}" ||
    die "unable to extract the tarball"

    cd stunserver ||
    die "unable to enter the directory"

    make CXXFLAGS="-fPIC -I/opt/local/include -L/opt/local/lib" || die "unable to build"

    CORE_DIR=${WORKDIR}/include/stun/stuncore
    COMMON_DIR=${WORKDIR}/include/stun/common
    mkdir -p ${CORE_DIR} ${COMMON_DIR}
    cp -r ./stuncore/*.h ${CORE_DIR}
    cp -r ./common/*.h ./common/*.hpp ${COMMON_DIR}
    cp ./stuncore/libstuncore.a ${WORKDIR}/lib/
    cp ./common/libcommon.a ${WORKDIR}/lib/libstuncommon.a
)
