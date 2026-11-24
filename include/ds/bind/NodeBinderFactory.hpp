#ifndef DRIFTSPEC_BIND_NODEBINDERFACTORY_HPP
#define DRIFTSPEC_BIND_NODEBINDERFACTORY_HPP

#include <memory>
#include <vector>

#include "ds/bind/AnnotationBinder.hpp"
#include "ds/bind/ExtensionBinder.hpp"
#include "ds/bind/FieldBinder.hpp"
#include "ds/bind/FormulaBinder.hpp"
#include "ds/bind/INodeBinder.hpp"
#include "ds/bind/MemberBinder.hpp"
#include "ds/bind/MemberBodyBinder.hpp"
#include "ds/bind/SignatureBinder.hpp"
#include "ds/bind/TypeBinder.hpp"

namespace ds {
namespace bind {

/// The one place that names the concrete node binders (DS-365, DS-404): it makes each, given the parts it uses, and
/// hands them back as the INodeBinder they are to the binder, in the order the table reads them.
class NodeBinderFactory {
 public:
  static std::vector<std::unique_ptr<INodeBinder>> make(const FieldBinder& fields, const AnnotationBinder& annotations,
                                                        const ExtensionBinder& extensions, const TypeBinder& types,
                                                        const SignatureBinder& signatures, const FormulaBinder& formulas,
                                                        const MemberBodyBinder& bodies, const MemberBinder& members);
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_NODEBINDERFACTORY_HPP
