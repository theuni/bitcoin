#include <random.h>
#include <kernel/fs.h>
#include <kernel/util/strencodings.h>

fs::path GetUniquePath(const fs::path& base)
{
    FastRandomContext rnd;
    fs::path tmpFile = base / HexStr(rnd.randbytes(8));
    return tmpFile;
}