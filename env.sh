#!/bin/sh

INFINIT_SOURCE_DIR=$(git rev-parse --show-toplevel)
INFINIT_BUILD_DIR=$PWD

ELLE_LOG_LEVEL="infinit.*:DUMP,reactor.network.*:DUMP"

DISTCC_HOSTS="development.infinit.io"

infinit_env_init() {
	export INFINIT_BUILD_DIR=${INFINIT_BUILD_DIR}
	export INFINIT_SOURCE_DIR=${INFINIT_SOURCE_DIR}
	export ELLE_LOG_LEVEL="${ELLE_LOG_LEVEL}"
	export DISTCC_HOSTS="${DISTCC_HOSTS}"

	export OLD_PYTHONPATH=$PYTHONPATH
	export OLD_RPROMPT=$RPROMPT

	export PYTHONPATH=${INFINIT_SOURCE_DIR}/elle/drake/src/:${INFINIT_BUILD_DIR}/lib/python:$PYTHONPATH
	export RPROMPT="Infinit ${RPROMPT}"

	export ELLE_PROCESS_DEBUG_CMD="urxvt -e ./debug.sh %s &"
}

infinit_env_clean() {
	unset INFINIT_BUILD_DIR
	unset INFINIT_SOURCE_DIR
	unset ELLE_LOG_COMPONENTS
	unset ELLE_LOG_LEVEL
	unset DISTCC_HOSTS

	unset OLD_PYTHONPATH
	unset OLD_RPROMPT

	export PYTHONPATH=${OLD_PYTHONPATH}
	export RPROMPT=${OLD_RPROMPT}

	unset ELLE_PROCESS_DEBUG_CMD
}

infinit_env_init

. ${INFINIT_SOURCE_DIR}/scripts/framework.sh
