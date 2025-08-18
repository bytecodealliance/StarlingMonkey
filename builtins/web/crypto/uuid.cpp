#include "uuid.h"
#include "host_api.h"

namespace builtins {
namespace web {
namespace crypto {

// FROM RFC 4122
// The formal definition of the UUID string representation is
// provided by the following ABNF [7]:
//
// UUID                   = time-low "-" time-mid "-"
//                           time-high-and-version "-"
//                           clock-seq-and-reserved
//                           clock-seq-low "-" node
// time-low               = 4hexOctet
// time-mid               = 2hexOctet
// time-high-and-version  = 2hexOctet
// clock-seq-and-reserved = hexOctet
// clock-seq-low          = hexOctet
// node                   = 6hexOctet
// hexOctet               = hexDigit hexDigit
// hexDigit =
//       "0" / "1" / "2" / "3" / "4" / "5" / "6" / "7" / "8" / "9" /
//       "a" / "b" / "c" / "d" / "e" / "f" /
//       "A" / "B" / "C" / "D" / "E" / "F"
struct UUID {
  uint32_t time_low;
  uint16_t time_mid;
  uint16_t time_high_and_version;
  uint8_t clock_seq_and_reserved;
  uint8_t clock_seq_low;
  uint8_t node[6];
};

// FROM RFC 4122
// 4.1.3.  Version
//
// The version number is in the most significant 4 bits of the time
// stamp (bits 4 through 7 of the time_hi_and_version field).
//
// The following table lists the currently-defined versions for this
// UUID variant.
//
// Msb0  Msb1  Msb2  Msb3   Version  Description
//
//   0     0     0     1        1    The time-based version
//                                   specified in this document.
//
//   0     0     1     0        2    DCE Security version, with
//                                   embedded POSIX UIDs.
//
//   0     0     1     1        3    The name-based version
//                                   specified in this document
//                                   that uses MD5 hashing.
//
//   0     1     0     0        4    The randomly or pseudo-
//                                   randomly generated version
//                                   specified in this document.
//
//   0     1     0     1        5    The name-based version
//                                   specified in this document
//                                   that uses SHA-1 hashing.
std::optional<std::string> random_uuid_v4(JSContext *cx) {
  UUID id{};

  {
    auto res = host_api::Random::get_bytes(sizeof(id));
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return std::nullopt;
    }

    auto bytes = std::move(res.unwrap());
    std::copy_n(bytes.begin(), sizeof(id), reinterpret_cast<uint8_t *>(&id));
  }

  // Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved to zero and
  // one, respectively.
  id.clock_seq_and_reserved &= 0x3f;
  id.clock_seq_and_reserved |= 0x80;
  // Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field to the
  // 4-bit version number from Section 4.1.3.
  id.time_high_and_version &= 0x0fff;
  id.time_high_and_version |= 0x4000;

  return fmt::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                      id.time_low,
                      id.time_mid,
                      id.time_high_and_version,
                      id.clock_seq_and_reserved, id.clock_seq_low,
                      id.node[0], id.node[1], id.node[2],
                      id.node[3], id.node[4], id.node[5]);
}

} // namespace uuid
} // namespace web
} // namespace builtins
