#include <miniz.h>

#include <string>
#include <vector>

#include "ds/format/Format.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace format {
namespace common {

std::string Packing::pack(const std::string& text) {
  mz_ulong size = mz_compressBound(static_cast<mz_ulong>(text.size()));
  std::vector<unsigned char> out(size);
  if (mz_compress2(out.data(), &size, reinterpret_cast<const unsigned char*>(text.data()), static_cast<mz_ulong>(text.size()),
                   MZ_BEST_COMPRESSION) != MZ_OK)
    return "";
  return text::base64(out.data(), size);
}

std::string Packing::unpack(const std::string& packed) {
  const std::string bytes = text::fromBase64(packed);
  for (mz_ulong room = static_cast<mz_ulong>(bytes.size()) * 8 + 1024; room < (1ul << 30); room *= 2) {
    std::vector<unsigned char> out(room);
    mz_ulong size = room;
    const int status =
        mz_uncompress(out.data(), &size, reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<mz_ulong>(bytes.size()));
    if (status == MZ_OK) return std::string(out.begin(), out.begin() + static_cast<long>(size));
    if (status != MZ_BUF_ERROR) return "";
  }
  return "";
}

}  // namespace common
}  // namespace format
}  // namespace ds
