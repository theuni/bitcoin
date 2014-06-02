#!/bin/sh
set -e
DIR=`pwd`

usage()
{
cat << EOF
usage: CODESIGNARGS="..." $0 [-n -d -q -k] [-a name.app] -p input.dmg or -i input dir -o output.tar.gz

OPTIONS:
   -i   input dir
   -p   input dmg
   -o   output tar.gz
   -n   no dependencies
   -d   dependencies only
   -q   quiet
   -k   keep temporary dirs
   -a   name of .app in the input dmg
ENVIRONMENT VARIABLES:
  CODESIGNARGS: non-optional. Parameters for codesign.

EOF
}

while getopts "h?ndqki:o:a:p:" opt; do
    case "$opt" in
    h|\?)
        usage
        exit 0
        ;;
    o)  OUT=$OPTARG
        ;;
    i)  IN=$OPTARG
        ;;
    a)  DMGAPP=$OPTARG
        ;;
    p)  DMG=$OPTARG
        ;;
    n)  NODEPS=1
        ;;
    d)  DEPSONLY=1
        ;;
    q)  QUIET=1
        ;;
    k)  KEEP=1
    esac
done

if [ -z ${OUT} ]; then
  usage
  echo "Error: output file is required"
  exit 1
fi

if [ -z ${DMG} ] && [ -z ${IN} ]; then
  usage
  echo "Error: input dir or dmg required"
  exit 1
fi

if [ ! -z ${DMG} ] && [ ! -z ${IN} ]; then
  usage
  echo "Error: -i and -p are incompatible."
  exit 1
fi

if [ ! -z ${NODEPS} ] && [ -z ${DEPSONLY} ]; then
  usage
  echo "Error: options -d and -n are incompatible"
  exit 1
fi

if [ -z "${CODESIGNARGS}" ]; then
  usage
  echo "Error: CODESIGNARGS not set."
  exit 1
fi

if [ ! -z ${DMG} ]; then
  MOUNTPOINT=dmg.mount
  if [ -z ${QUIET} ]; then
    echo "Mounting ${DMG} at ${MOUNTPOINT}"
  fi
  hdiutil attach -readonly ${DMG} -mountpoint ${MOUNTPOINT} >/dev/null
  DMG_EXTRACT_DIR=dmg.temp
  mkdir -p ${DMG_EXTRACT_DIR}
  if [ -z "${DMGAPP}" ]; then
    cp -Rf ${MOUNTPOINT}/* ${DMG_EXTRACT_DIR}
  else
    if [ -d ${MOUNTPOINT}/${DMGAPP} ]; then
      cp -Rf ${MOUNTPOINT}/${DMGAPP} ${DMG_EXTRACT_DIR}
      IN=${DMG_EXTRACT_DIR}/${DMGAPP}
    else
      echo "Error: ${DMGAPP} not found in ${DMG}"
      rm -rf ${DMG_EXTRACT_DIR}
      if [ -z ${QUIET} ]; then
        echo "Unmounting ${DMG}"
      fi
      hdiutil detach ${MOUNTPOINT} >/dev/null
      exit 1
    fi
  fi
  if [ -z ${IN} ]; then
    DMGAPP=`ls -1 ${DMG_EXTRACT_DIR}/ | grep \.app`
    if [ -z ${DMGAPP} ]; then
      echo "Error: No .app found in ${DMG}"
      rm -rf ${DMG_EXTRACT_DIR}
      if [ -z ${QUIET} ]; then
        echo "Unmounting ${DMG}"
      fi
      hdiutil detach ${MOUNTPOINT} >/dev/null
      exit 1
    fi
    if [ -z ${QUIET} ]; then
      echo "Using app: ${DMGAPP}"
    fi
    IN=${DMG_EXTRACT_DIR}/${DMGAPP}
  fi
  if [ -z ${QUIET} ]; then
    echo "Copied files"
    echo "Unmounting ${DMG}"
  fi
  hdiutil detach ${MOUNTPOINT} >/dev/null
  find ${DMG_EXTRACT_DIR} -type f -exec chmod 644 {} \;
  find ${DMG_EXTRACT_DIR} -type d -exec chmod 755 {} \;
fi

if [ ! -d ${IN} ]; then
  usage
  echo "Error: input dir: ${IN} not found"
fi

BUNDLE=`basename ${IN}`
CODESIGN=codesign
TEMPDIR=sign.temp
TEMPLIST=${TEMPDIR}/signatures.txt
rm -rf ${TEMPDIR} ${TEMPLIST}
mkdir -p ${TEMPDIR}

if [ -z ${NODEPS} ]; then
  eval "${CODESIGN} -f --file-list ${TEMPLIST} ${CODESIGNARGS} ${IN}/Contents/Frameworks/*/Versions/*"
  eval "${CODESIGN} -f --file-list ${TEMPLIST} ${CODESIGNARGS} ${IN}/Contents/Frameworks/*.dylib"
  eval "${CODESIGN} -f --file-list ${TEMPLIST} ${CODESIGNARGS} ${IN}/Contents/PlugIns/*/*.dylib"
fi

if [ -z ${DEPSONLY} ]; then
  eval "${CODESIGN} -f --file-list ${TEMPLIST} ${CODESIGNARGS} ${IN}/"
fi

for i in `grep -v CodeResources ${TEMPLIST}`; do
  TARGETFILE=${BUNDLE}/`echo ${i} | sed "s|.*${IN}/||"`
  SIZE=`pagestuff $i -p | tail -2 | grep size | sed 's/[^0-9]*//g'`
  OFFSET=`pagestuff $i -p | tail -2 | grep offset | sed 's/[^0-9]*//g'`
  SIGNFILE=${TEMPDIR}/${TARGETFILE}.sign
  DIRNAME=`dirname ${SIGNFILE}`
  mkdir -p ${DIRNAME}
  if [ -z ${QUIET} ]; then
    echo "Adding detached signature for: ${TARGETFILE}"
  fi
  dd if=$i of=${SIGNFILE} bs=1 iseek=${OFFSET} count=${SIZE} 2>/dev/null
done

for i in `grep CodeResources ${TEMPLIST}`; do
  TARGETFILE=${BUNDLE}/`echo ${i} | sed "s|.*${IN}/||"`
  RESOURCE=${TEMPDIR}/${TARGETFILE}
  DIRNAME=`dirname "${RESOURCE}"`
  mkdir -p ${DIRNAME}
  if [ -z ${QUIET} ]; then
    echo "Adding resource for: "${TARGETFILE}""
  fi
  cp "${i}" "${RESOURCE}"
done


find ${TEMPDIR}/${BUNDLE} -type f | sed "s|${TEMPDIR}/${BUNDLE}/||" | sort | tar -C ${TEMPDIR}/${BUNDLE} -n -c - -T - | gzip -9 -n > ${OUT}

rm -f ${TEMPLIST}

if [ -z ${KEEP} ]; then
  if [ -z ${QUIET} ]; then
    echo "Cleaning up"
  fi
  if [ ! -z ${DMG} ]; then
    rm -rf ${DMG_EXTRACT_DIR}
  fi
  rm -rf ${TEMPDIR}
fi

if [ -z ${QUIET} ]; then
  echo "Created ${OUT}"
fi
