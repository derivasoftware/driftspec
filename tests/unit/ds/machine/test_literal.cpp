#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ds/machine/Literal.hpp"
#include "ds/model/Model.hpp"

namespace {

/// One literal of a state rule, as the binder writes it and as a reader is told it (DS-249).
TEST(TestLiteral, ALiteralIsReadAsWrittenAndToldInWords) {
  const ds::machine::Literal compared = ds::machine::Literal::read("!status=Open");
  EXPECT_TRUE(compared.negated);
  EXPECT_EQ(compared.name, "status");
  EXPECT_EQ(compared.op, "=");
  EXPECT_EQ(compared.value, "Open");
  EXPECT_EQ(compared.written(), "not status = Open");
  const ds::machine::Literal differs = ds::machine::Literal::read("status!=Paid");
  EXPECT_EQ(differs.op, "!=");
  EXPECT_EQ(differs.written(), "status != Paid");
  const ds::machine::Literal alone = ds::machine::Literal::read("cancelled");
  EXPECT_FALSE(alone.negated);
  EXPECT_TRUE(alone.op.empty());
  EXPECT_EQ(alone.written(), "cancelled");
}

}  // namespace
