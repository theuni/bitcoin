Fuzz-testing Bitcoin Core
==========================

Building AFL
-------------

It is recommended to always use the latest version of afl:
```
wget http://lcamtuf.coredump.cx/afl/releases/afl-latest.tgz
tar -zxvf afl-latest.tgz
cd afl-<version>
make
export AFLPATH=$PWD
```

Instrumentation
----------------

To build Bitcoin Core using AFL instrumentation (this assumes that the
`AFLPATH` was set as above):
```
./configure --disable-ccache --disable-shared --enable-tests CC=${AFLPATH}/afl-gcc CXX=${AFLPATH}/afl-g++
export AFL_HARDEN=1
cd src/
make test/test_bitcoin_fuzzy
```
We disable ccache because we don't want to pollute the ccache with instrumented
objects, and similarly don't want to use non-instrumented cached objects linked
in.

Preparing fuzzing
------------------

AFL needs an input directory with examples, and an output directory where it will place
examples that it found.

```
mkdir inputs
AFLIN=$PWD/inputs
mkdir outputs
AFLOUT=$PWD/outputs
```

Fuzzing
--------

To start the actual fuzzing use:
```
$AFLPATH/afl-fuzz -i ${AFLIN} -o ${AFLOUT} -- test/test_bitcoin_fuzzy
```

You may have to change some kernel parameters to test optimally - `afl-fuzz`
will print an error and suggestion if so.

