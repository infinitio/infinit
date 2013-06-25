#
# ---------- globals ----------------------------------------------------------
#

SCRIPT=$(basename "$0")
ROOTDIR=$(dirname $(realpath "$0"))

#
# ---------- imports ----------------------------------------------------------
#

BASEDIR="${ROOTDIR}/../.."

. "${BASEDIR}/environment.sh"
. "${BASEDIR}/common.sh"

#
# ---------- entry point ------------------------------------------------------
#



if ! uptodate "${OPENSSL_LIBRARIES}"; then
    download "${OPENSSL_SNAPSHOT}" "${OPENSSL_FINGERPRINT}" "${BUILDDIR}/${OPENSSL_TARBALL}"
    cd "${BUILDDIR}" || die "unable to move to the build directory '${BUILDDIR}'"
    tar xzf "${OPENSSL_TARBALL}" || die "unable to extract the tarball"
    cd "${OPENSSL_NAME}" || die "unable to enter the directory"
    case "$PLATFORM" in
        (linux*|macos*)
            ./config                                                            \
                --prefix="${WORKDDIR}"                                          \
                --openssldir="${WORKDIR}"                                       \
                shared                                                          \
                || die "unable to configure"

            make install || die "unable to build"
            ;;
        (win*)
            perl Configure VC-WIN32 -shared --prefix="${WORKDIR}"
            ms/do_ms.bat
            tmp="$(mktemp /tmp/build.XXXXXX.bat)"
            echo '@echo off' >  "$tmp"
            cat "$(cygpath "${VS110COMNTOOLS}\\vsvars32.bat")" >>  "$tmp"
            echo 'echo %PATH%' >> "$tmp"
            echo 'echo %INCLUDE%' >> "$tmp"
            echo 'echo %LIB%' >> "$tmp"
            chmod u+x "$tmp"
            OLDIFS="$IFS"
            output=$("$tmp")
            msvc_path="$(echo "$output" | sed -n '1p')"
            export INCLUDE="$(echo "$output" | sed -n '2p')"
            export LIB="$(echo "$output" | sed -n '3p')"
            IFS=";"
            for path in $msvc_path; do
                NEWPATH="$NEWPATH:$(cygpath "$path")"
            done
            IFS="$OLDIFS"
            export PATH="$NEWPATH:$PATH"
            rm "$tmp"
            nmake -f ms/nt.mak
            nmake -f ms/nt.mak install
            ;;
    esac
fi
