#!/bin/bash

FILESDIR="$1"
SEARCHSTR="$2"

if [ -z "${FILESDIR}" ] || [ ! -d "${FILESDIR}" ]; then
   >&2 echo "Specified files dir '${FILESDIR}' empty or does not exist"
   exit 1
fi

if [ -z "${SEARCHSTR}" ]; then
   >&2 echo "Search string not specified"
   exit 1
fi

# The number of files are X and the number of matching lines are Y

FILES="$(find "${FILESDIR}" -type f | wc -l)"
LINES="$(grep -r "${SEARCHSTR}" "${FILESDIR}" | wc -l)"
echo "The number of files are ${FILES} and the number of matching lines are ${LINES}"