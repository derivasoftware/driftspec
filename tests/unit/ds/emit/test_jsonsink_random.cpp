#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <nlohmann/json.hpp>
#include <random>
#include <string>

#include "ds/emit/JsonSink.hpp"

namespace {

/// The JSON sink against nlohmann::json 3.11.3 on random input (DS-396's checker, kept as a test, DS-397): random
/// trees replayed through the sink in both layouts, random bytes as texts (controls, quotes, valid and invalid
/// UTF-8, the message of a refusal included), and random doubles, which the sink writes as the shortest digits
/// that read back where nlohmann's Grisu2 is at times longer. The seed is fixed: a failure is the same every run.
class TestJsonSinkAgainstNlohmann : public ::testing::Test {
 protected:
  std::mt19937_64 rng{397};

  std::uint64_t below(std::uint64_t n) { return n ? rng() % n : 0; }

  std::string randomText(int longest, bool validOnly) {
    static const char* const valid[] = {"\xc3\xb1",     "\xc3\xbc",     "\xe2\x82\xac",     "\xf0\x9f\x98\x80", "\xe2\x80\xa8",
                                        "\xe2\x80\xa9", "\xef\xbf\xbd", "\xc2\x80",         "\xdf\xbf",         "\xe0\xa0\x80",
                                        "\xed\x9f\xbf", "\xee\x80\x80", "\xf4\x8f\xbf\xbf", "\xf0\x90\x80\x80"};
    static const char* const invalid[] = {"\xc3",         "\xe2\x82",     "\xf0\x9f\x98",        "\xc0\xaf",
                                          "\xe0\x80\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80",    "\xf5\x80\x80\x80",
                                          "\xff",         "\xc1\xbf",     "\xf8\x88\x80\x80\x80"};
    std::string s;
    const int size = static_cast<int>(below(static_cast<std::uint64_t>(longest) + 1));
    for (int i = 0; i < size; ++i) {
      switch (below(validOnly ? 6 : 8)) {
        case 0:
        case 1:
          s += static_cast<char>(0x20 + below(0x5F));  // printable ASCII, '/' and DEL among it
          break;
        case 2:
          s += static_cast<char>(below(0x20));  // a control character
          break;
        case 3:
          s += "\"\\/\x7f"[below(4)];
          break;
        case 4:
        case 5:
          s += valid[below(sizeof valid / sizeof *valid)];
          break;
        case 6:
          s += static_cast<char>(0x80 + below(0x80));  // any high byte: often invalid
          break;
        default:
          s += invalid[below(sizeof invalid / sizeof *invalid)];
      }
    }
    return s;
  }

  nlohmann::json randomTree(int depth) {
    switch (below(depth > 0 ? 9 : 6)) {
      case 0:
        return nlohmann::json(randomText(6, true));
      case 1:
        return nlohmann::json(static_cast<std::int64_t>(rng()) >> below(64));
      case 2:
        return nlohmann::json(static_cast<double>(static_cast<std::int64_t>(below(2000001)) - 1000000) / 4.0);  // both write these alike
      case 3:
        return nlohmann::json(below(2) == 1);
      case 4:
        return nlohmann::json(nullptr);
      case 5:
        return nlohmann::json(static_cast<int>(below(200)) - 100);
      case 6:
      case 7: {
        nlohmann::json o = nlohmann::json::object();
        const int n = static_cast<int>(below(5));
        for (int i = 0; i < n; ++i) o[randomText(4, true)] = randomTree(depth - 1);
        return o;
      }
      default: {
        nlohmann::json a = nlohmann::json::array();
        const int n = static_cast<int>(below(5));
        for (int i = 0; i < n; ++i) a.push_back(randomTree(depth - 1));
        return a;
      }
    }
  }

  /// The tree replayed through the sink: an object's keys come in nlohmann's std::map order, byte order.
  static void replay(const nlohmann::json& j, ds::emit::ISink& sink) {
    switch (j.type()) {
      case nlohmann::json::value_t::object:
        sink.beginRecord();
        for (auto it = j.begin(); it != j.end(); ++it) {
          sink.field(it.key());
          replay(it.value(), sink);
        }
        sink.endRecord();
        break;
      case nlohmann::json::value_t::array:
        sink.beginList();
        for (const auto& x : j) replay(x, sink);
        sink.endList();
        break;
      case nlohmann::json::value_t::string:
        sink.value(j.get_ref<const std::string&>());
        break;
      case nlohmann::json::value_t::number_integer:
      case nlohmann::json::value_t::number_unsigned:
        sink.integer(j.get<std::int64_t>());
        break;
      case nlohmann::json::value_t::number_float:
        sink.number(j.get<double>());
        break;
      case nlohmann::json::value_t::boolean:
        sink.boolean(j.get<bool>());
        break;
      default:
        sink.null();
    }
  }

  static std::string viaNlohmann(const std::string& s) {
    try {
      return nlohmann::json(s).dump();
    } catch (const std::exception& e) {
      return std::string("refused: ") + e.what();
    }
  }

  static std::string viaSink(const std::string& s) {
    try {
      ds::emit::JsonSink sink(false, true);
      sink.value(s);
      return sink.result();
    } catch (const std::exception& e) {
      return std::string("refused: ") + e.what();
    }
  }

  /// A double through the sink: it reads back as itself, it is never longer than nlohmann's, and it is in
  /// nlohmann's layout: an exponent where nlohmann writes one, and the same one when as long.
  static void compare(double x) {
    const std::string tree = nlohmann::json(x).dump();
    ds::emit::JsonSink sink(false, true);
    sink.number(x);
    const std::string ours = sink.result();
    if (!std::isfinite(x)) {
      EXPECT_EQ(ours, tree);
      return;
    }
    EXPECT_EQ(std::strtod(ours.c_str(), nullptr), x) << ours;
    EXPECT_LE(ours.size(), tree.size()) << ours << " " << tree;
    EXPECT_EQ(ours.find('e') == std::string::npos, tree.find('e') == std::string::npos) << ours << " " << tree;
    if (ours.size() == tree.size() && ours.find('e') != std::string::npos) {
      EXPECT_EQ(ours.substr(ours.find('e')), tree.substr(tree.find('e'))) << ours << " " << tree;
    }
  }
};

TEST_F(TestJsonSinkAgainstNlohmann, RandomTreesAreWrittenAsNlohmannWritesThemInBothLayouts) {
  int differ = 0;
  for (int i = 0; i < 20000; ++i) {
    const nlohmann::json tree = randomTree(static_cast<int>(below(5)));
    for (bool pretty : {false, true}) {
      ds::emit::JsonSink sink(pretty, true);
      replay(tree, sink);
      const std::string ours = sink.result();
      const std::string theirs = pretty ? tree.dump(2) : tree.dump();
      if (ours != theirs && ++differ <= 3) ADD_FAILURE() << "nlohmann " << theirs << "\nsink     " << ours;
    }
  }
  EXPECT_EQ(differ, 0);
}

TEST_F(TestJsonSinkAgainstNlohmann, RandomTextsAreEscapedOrRefusedAsNlohmannDoes) {
  int differ = 0, refused = 0;
  for (int i = 0; i < 200000; ++i) {
    const std::string s = randomText(10, false);
    const std::string theirs = viaNlohmann(s), ours = viaSink(s);
    refused += theirs.rfind("refused: ", 0) == 0;
    if (ours != theirs && ++differ <= 3) ADD_FAILURE() << "nlohmann " << theirs << "\nsink     " << ours;
  }
  EXPECT_EQ(differ, 0);
  EXPECT_GT(refused, 0);  // the invalid ones were tried
}

TEST_F(TestJsonSinkAgainstNlohmann, RandomDoublesReadBackNoLongerThanNlohmannsInItsLayout) {
  for (int i = 0; i < 100000; ++i) {
    std::uint64_t bits = rng();
    double x;
    std::memcpy(&x, &bits, sizeof x);
    compare(x);
  }
  for (int i = 0; i < 100000; ++i) {
    char written[64];
    std::snprintf(written, sizeof written, "%llue%d", static_cast<unsigned long long>(below(1000000)), static_cast<int>(below(25)) - 12);
    compare((below(2) ? -1 : 1) * std::strtod(written, nullptr));
  }
  for (double x : {0.0,
                   -0.0,
                   1.0,
                   0.1,
                   1e15,
                   1e16,
                   1e-4,
                   1e-5,
                   123456789012345.0,
                   1234567890123456.0,
                   9007199254740993.0,
                   DBL_MAX,
                   DBL_MIN,
                   DBL_TRUE_MIN,
                   -DBL_MAX,
                   1e308,
                   1e-308,
                   2.0 / 3.0,
                   std::numeric_limits<double>::infinity(),
                   std::numeric_limits<double>::quiet_NaN()})
    compare(x);
  for (int e = -330; e <= 310; ++e)
    for (double m : {1.0, 2.0, 5.0, 9.999999999999999}) compare(m * std::pow(10.0, e));
}

}  // namespace
