#pragma once

#include <boost/bimap.hpp> // NEEDEDINCLUDE

namespace opossum {

enum class JitExpressionType;

extern const boost::bimap<JitExpressionType, std::string> jit_expression_type_to_string;

}  // namespace opossum
