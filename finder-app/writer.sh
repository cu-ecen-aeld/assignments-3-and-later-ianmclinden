#!/bin/bash

WRITEFILE="$1"
WRITESTR="$2"

if [ -z "${WRITEFILE}" ] || [ -z "${WRITESTR}" ]; then
   >&2 echo "Write file or write string not specified"
   exit 1
fi

set -e # lazy error prop
mkdir -p "$(dirname "${WRITEFILE}")"
echo -e "${WRITESTR}" > "${WRITEFILE}"