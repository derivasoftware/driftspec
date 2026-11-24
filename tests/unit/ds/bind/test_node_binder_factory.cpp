#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

#include "ds/bind/NodeBinderFactory.hpp"

namespace {

/// The one place that names the concrete node binders (DS-404): it makes one of each, every one a different binder,
/// each handed back as the INodeBinder it is.
TEST(TestNodeBinderFactory, ItMakesOneBinderOfEachConstruct) {
  const ds::bind::FieldBinder fields;
  const ds::bind::AnnotationBinder annotations;
  const ds::bind::ExtensionBinder extensions{annotations};
  const ds::bind::TypeBinder types;
  const ds::bind::SignatureBinder signatures{types};
  const ds::bind::FormulaBinder formulas;
  const ds::bind::MemberBodyBinder bodies{fields, annotations};
  const ds::bind::MemberBinder members{types, signatures, bodies};
  const auto binders = ds::bind::NodeBinderFactory::make(fields, annotations, extensions, types, signatures, formulas, bodies, members);
  ASSERT_EQ(binders.size(), 12u);
  std::set<std::string> kinds;
  for (const auto& b : binders) {
    ASSERT_NE(b, nullptr);
    kinds.insert(typeid(*b).name());
  }
  EXPECT_EQ(kinds.size(), binders.size());  // no construct is bound twice
}

}  // namespace
