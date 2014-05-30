#!/bin/sh
set -e
#set -x
usage()
{
cat << EOF
usage: $0 [-q] -i input.tar.gz -o program.app

OPTIONS:
   -i   input signature tarball
   -o   target app bundle
   -q   quiet

ENVIRONMENT VARIABLES:
  CODESIGN_ALLOCATE: path to codesign_allocate binary
  PAGESTUFF: path to pagestuff binary

EOF
}

ARCH=x86_64
while getopts "qh?i:o:" opt; do
    case "$opt" in
    h|\?)
        usage
        exit 0
        ;;
    o)  OUT="${OPTARG}"
        ;;
    i)  IN="${OPTARG}"
        ;;
    q)  QUIET=1
        ;;
    a)  ARCH="${OPTARG}"
        ;;
    esac
done

if [ -z "${IN}" ] || [ -z "${OUT}" ]; then
  usage
  echo "Error: input tarball and output dir are required"
  exit 1
fi

if [ -z "${CODESIGN_ALLOCATE}" ]; then
  CODESIGN_ALLOCATE=codesign_allocate
fi

if [ -z "${PAGESTUFF}" ]; then
  PAGESTUFF=pagestuff
fi

if [ -z ${QUIET} ]; then
  echo "Extracting ${IN} to ${OUT}"
fi

tar -C ${OUT} -xf ${IN}

for i in `find ${OUT} -name "*.sign"`; do
  SIZE=`stat -c %s ${i}`
  TARGET_FILE=`echo ${i} | sed 's/\.sign$//'`

  if [ -z ${QUIET} ]; then
    echo "Allocating space for the signature of size ${SIZE} in ${TARGET_FILE}"
  fi
  ${CODESIGN_ALLOCATE} -i ${TARGET_FILE} -a ${ARCH} ${SIZE} -o ${i}.tmp

  OFFSET=`${PAGESTUFF} ${i}.tmp -p | tail -2 | grep offset | sed 's/[^0-9]*//g'`
  if [ -z ${QUIET} ]; then
    echo "Attaching signature at offset ${OFFSET}"
  fi

  dd if=$i of=${i}.tmp bs=1 seek=${OFFSET} count=${SIZE} 2>/dev/null
  mv ${i}.tmp ${TARGET_FILE}
  rm ${i}
  if [ -z ${QUIET} ]; then
    echo "Success.\n"
  fi
done
