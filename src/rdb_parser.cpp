#include "rdb_parser.h"

#include "debug.h"
#include "utils.h"

#include <cstdint>
#include <iostream>
#include <sstream>

RDBParseError::RDBParseError(std::string reason)
  : std::runtime_error(reason)
{
}

namespace {

class RDBParser {
  enum OpCode : std::uint8_t {
    OP_AUX,
    OP_SELECTDB,
    OP_EXPIRETIME,
    OP_EXPIRETIMEMS,
    OP_RESIZEDB,
    OP_EOF
  };

  enum ValueType : std::uint8_t {
    VT_STRING_ENCODING = 0x00,
  };

  struct LengthEncoding {
    enum : std::uint8_t {
      LENGTH,
      SPECIAL
    } type = LENGTH;

    std::uint32_t length = 0;
    std::uint8_t special = 0;
  };

  struct Entry {
    std::string key;
    std::string value;

    std::optional<Timepoint> expire_time;
  };

public:
  RDBParser(std::istream& from, IRDBParserListener& to)
    : from(from), to(to)
  {
  }

  void parse() {
    if (!this->from) throw RDBParseError("Invalid input stream");

    {
      auto redis_magic = this->parse_string(5);
      if (redis_magic != "REDIS") {
        throw RDBParseError("Missing magic");
      }
    }

    auto rdb_version = this->parse_string(4);
    if (auto maybe_rdb_version = parseInt(rdb_version)) {
      this->rdb_version = maybe_rdb_version.value();
    } else {
      throw RDBParseError("Missing rdb version");
    }

    if (DEBUG_LEVEL >= 1) std::cerr << "RDB verison: " << this->rdb_version << std::endl;

    while (true) {
      auto opcode = this->parse_op_code();

      if (opcode == OP_AUX) {
        this->parse_aux_field();
      } else if (opcode == OP_SELECTDB) {
        this->parse_db();
      } else if (opcode == OP_EOF) {
        if (this->rdb_version >= 5) {
          auto crc64_checksum = this->parse_string(8);
        }
        break;
      } else {
        throw RDBParseError(print_args("Unexpected section 0x", to_hex(static_cast<std::underlying_type_t<OpCode>>(opcode))));
      }
    }
  }

private:
  std::istream& from;
  IRDBParserListener& to;

  int rdb_version = 0;

  std::string parse_string(std::size_t count) {
    std::string result;
    result.resize(count);

    this->from.read(result.data(), count);

    if (!this->from) throw RDBParseError("Invalid input stream");

    return result;
  }

  OpCode parse_op_code() {
    auto ch = this->parse_uint8();

    if (ch == 0xFF) {
      return OP_EOF;
    }

    if (ch == 0xFA) {
      return OP_AUX;
    }

    if (ch == 0xFB) {
      return OP_RESIZEDB;
    }

    if (ch == 0xFC) {
      return OP_EXPIRETIMEMS;
    }

    if (ch == 0xFE) {
      return OP_SELECTDB;
    }

    if (ch == 0xFD) {
      return OP_EXPIRETIME;
    }

    throw RDBParseError(print_args("Unexpected opcode 0x", to_hex(ch)));
  }

  std::uint8_t peek_uint8() {
    std::uint8_t ch = this->from.peek();
    if (!this->from) throw RDBParseError("Invalid input stream");
    return ch;
  }

  std::int8_t parse_int8() {
    std::int8_t result;
    this->from.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!this->from) throw RDBParseError("Invalid input stream");
    return result;
  }

  std::uint8_t parse_uint8() {
    std::uint8_t result;
    this->from.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!this->from) throw RDBParseError("Invalid input stream");
    return result;
  }

  std::int16_t parse_int16() {
    std::int16_t result;
    this->from.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!this->from) throw RDBParseError("Invalid input stream");
    // return byteswap(result);
    return result;
  }

  std::int32_t parse_int32() {
    std::int32_t result;
    this->from.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!this->from) throw RDBParseError("Invalid input stream");
    // return byteswap(result);
    return result;
  }

  std::uint32_t parse_uint32(){
    std::uint32_t result;
    this->from.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!this->from) throw RDBParseError("Invalid input stream");
    // return byteswap(result);
    return result;
  }

  std::uint64_t parse_uint64(){
    std::uint64_t result;
    this->from.read(reinterpret_cast<char*>(&result), sizeof(result));
    if (!this->from) throw RDBParseError("Invalid input stream");
    // return byteswap(result);
    return result;
  }

  LengthEncoding parse_length_encoding() {
    std::uint8_t ch = this->parse_uint8();
    LengthEncoding result;

    std::uint8_t type = (ch & 0xc0) >> 6;
    if (type == 0) {
      result = {
        .type = LengthEncoding::LENGTH,
        .length = uint32_t(ch & 0x3f),
      };
    } else if (type == 1) {
      result = {
        .type = LengthEncoding::LENGTH,
        .length = uint32_t(ch & 0x3f) << 8 + this->parse_uint8(),
      };
    } else if (type == 2) {
      result = {
        .type = LengthEncoding::LENGTH,
        .length = this->parse_uint32(),
      };
    } else if (type == 3) {
      result = {
        .type = LengthEncoding::SPECIAL,
        .special = uint8_t(ch & 0x3f),
      };
    } else {
      throw RDBParseError("Broken parser");
    }

    if (DEBUG_LEVEL >= 2) {
      std::cerr << "Length encoding parsed: " << std::endl
        << "  type = " << static_cast<int>(result.type) << std::endl
        << "  length = " << result.length << std::endl
        << "  special = " << static_cast<int>(result.special) << std::endl;
    }

    return result;
  }

  std::string parse_string_encoded() {
    if (DEBUG_LEVEL >= 2) std::cerr << "Parsing string encoded" << std::endl;
    auto encoding = this->parse_length_encoding();

    if (encoding.type == LengthEncoding::LENGTH) {
      return this->parse_string(encoding.length);
    } else if (encoding.type == LengthEncoding::SPECIAL) {
      if (encoding.special == 0) {
        return std::to_string(this->parse_int8());
      } else if (encoding.special == 1) {
        return std::to_string(this->parse_int16());
      } else if (encoding.special == 2) {
        return std::to_string(this->parse_int32());
      } else {
        throw RDBParseError("Unknown string encoding special type");
      }
    } else {
      throw RDBParseError("Unknown string encoding type");
    }
  }

  void parse_aux_field() {
    if (DEBUG_LEVEL >= 2) std::cerr << "Parsing aux field" << std::endl;
    auto key = this->parse_string_encoded();
    auto value = this->parse_string_encoded();

    if (DEBUG_LEVEL >= 1) std::cerr << "Met aux field, key = " << key << ", value = " << value << std::endl;
  }

  std::optional<Entry> parse_entry(std::uint8_t type) {
    if (type != VT_STRING_ENCODING) {
      return {};
    }

    this->parse_uint8();
    auto key = this->parse_string_encoded();
    auto value = this->parse_string_encoded();

    return Entry{
      .key = std::move(key),
      .value = std::move(value)
    };
  }

  void parse_db() {
    if (DEBUG_LEVEL >= 2) std::cerr << "Parsing db" << std::endl;

    int db_number = this->parse_uint8();
    if (DEBUG_LEVEL >= 1) std::cerr << "Parsing db #" << db_number << std::endl;

    if (this->parse_op_code() == OP_RESIZEDB) {
      auto hash_table_size = this->parse_length_encoding().length;
      auto expire_hash_table_size = this->parse_length_encoding().length;
      if (DEBUG_LEVEL >= 1) {
        std::cerr << "Resizedb info: HT size = " << hash_table_size << ", expire HT size = " << expire_hash_table_size << std::endl;
      }
    }

    while (true) {
      auto next = this->peek_uint8();

      Entry entry;
      if (next == 0xFD) {
        this->parse_uint8();
        auto expire_time_s = this->parse_uint32();
        auto maybe_entry = this->parse_entry(this->peek_uint8());
        if (!maybe_entry) {
          throw RDBParseError("Expected entry");
        }
        entry = std::move(maybe_entry.value());
        entry.expire_time = Timepoint(std::chrono::seconds(expire_time_s));

      } else if (next == 0xFC) {
        this->parse_uint8();
        auto expire_time_ms = this->parse_uint64();
        auto maybe_entry = this->parse_entry(this->peek_uint8());
        if (!maybe_entry) {
          throw RDBParseError("Expected entry");
        }
        entry = std::move(maybe_entry.value());
        entry.expire_time = Timepoint(std::chrono::milliseconds(expire_time_ms));

      } else {
        auto maybe_entry = this->parse_entry(next);
        if (!maybe_entry) {
          break;
        }
        entry = std::move(maybe_entry.value());
      }

      if (DEBUG_LEVEL >= 1) {
        std::cerr << "DEBUG Met kv entry:" << std::endl
          << "  key = " << entry.key << std::endl
          << "  value = " << entry.value << std::endl;
      }

      auto now = Clock::now();
      if (entry.expire_time) {
        if (DEBUG_LEVEL >= 1) {
          std::cerr << "  now         = " << now.time_since_epoch() << std::endl;
          std::cerr << "  expire time = " << entry.expire_time.value().time_since_epoch() << std::endl;
        }
      }

      if (!entry.expire_time || entry.expire_time > now) {
        this->to.restore(entry.key, entry.value, entry.expire_time);
      } else {
        std::cerr << "  SKIP" << std::endl;
      }
    }
  }
};

} // namespace

void RDBParse(std::istream& from, IRDBParserListener& to) {
  RDBParser parser(from, to);

  parser.parse();
}

