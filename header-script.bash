#!/usr/bin/env bash

set -euo pipefail

(
    cd "$(git rev-parse --show-toplevel)"/src
    for i in $(clang++ -MM -MG bitcoin-chainstate.cpp -I. -DHAVE_CONFIG_H \
                   | sed 's@\\$@@g' \
                   | tr -d '\n' \
                   | cut -d ':' -f2 \
                   | xargs -n1 echo \
                   | grep -v '\.cpp$' \
                   | grep -v '^kernel\/')
    do
        if git ls-files --error-unmatch -- "$i" 2> /dev/null ; then
            target=kernel/"$i"
            mkdir -p "$(dirname "$target")"

            git mv -v "$i" "$target"

            regex='([^/]\b)('"$(sed -e 's/[]\/$*.^[]/\\&/g' <<< "$i")"'\b)'
            git grep -l -E "$regex" -- ':!*.md' | xargs sed -i -E 's@'"$regex"'@\1kernel/\2@g'
        else
            echo "Not tracked: $i"
        fi
    done
)
