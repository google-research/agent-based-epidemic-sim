/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_STATUS_MACROS_H_
#define THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_STATUS_MACROS_H_

// Helper macros and methods to return and propagate errors with
// `absl::Status`.

#include "absl/status/status.h"
#include "agent_based_epidemic_sim/port/deps/status_builder.h"

// Evaluates an expression that produces a `absl::Status`. If the status
// is not ok, returns it from the current function.
//
// For example:
//   absl::Status MultiStepFunction() {
//     PANDEMIC_RETURN_IF_ERROR(Function(args...));
//     PANDEMIC_RETURN_IF_ERROR(foo.Method(args...));
//     return absl::OkStatus();
//   }
//
// The macro ends with a `pandemic::StatusBuilder` which allows the returned
// status to be extended with more details.  Any chained expressions after the
// macro will not be evaluated unless there is an error.
//
// For example:
//   absl::Status MultiStepFunction() {
//     PANDEMIC_RETURN_IF_ERROR(Function(args...)) << "in MultiStepFunction";
//     PANDEMIC_RETURN_IF_ERROR(foo.Method(args...)).Log(base_logging::ERROR)
//         << "while processing query: " << query.DebugString();
//     return absl::OkStatus();
//   }
//
//
// If using this macro inside a lambda, you need to annotate the return type
// to avoid confusion between a `pandemic::StatusBuilder` and a
// `absl::Status` type. E.g.
//
//   []() -> ::absl::Status {
//     PANDEMIC_RETURN_IF_ERROR(Function(args...));
//     PANDEMIC_RETURN_IF_ERROR(foo.Method(args...));
//     return absl::OkStatus();
//   }
#define PANDEMIC_RETURN_IF_ERROR(expr)                              \
  PANDEMIC_STATUS_MACROS_IMPL_ELSE_BLOCKER_                         \
  if (::pandemic::status_macro_internal::StatusAdaptorForMacros     \
          status_macro_internal_adaptor = {(expr), PANDEMIC_LOC}) { \
  } else /* NOLINT */                                               \
    return status_macro_internal_adaptor.Consume()

// Executes an expression `rexpr` that returns a `pandemic::StatusOr<T>`. On
// OK, extracts its value into the variable defined by `lhs`, otherwise returns
// from the current function. By default the error status is returned
// unchanged, but it may be modified by an `error_expression`. If there is an
// error, `lhs` is not evaluated; thus any side effects that `lhs` may have
// only occur in the success case.
//
// Interface:
//
//   PANDEMIC_ASSIGN_OR_RETURN(lhs, rexpr)
//   PANDEMIC_ASSIGN_OR_RETURN(lhs, rexpr, error_expression);
//
// WARNING: expands into multiple statements; it cannot be used in a single
// statement (e.g. as the body of an if statement without {})!
//
// Example: Declaring and initializing a new variable (ValueType can be anything
//          that can be initialized with assignment, including references):
//   PANDEMIC_ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(arg));
//
// Example: Assigning to an existing variable:
//   ValueType value;
//   PANDEMIC_ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
//
// Example: Assigning to an expression with side effects:
//   MyProto data;
//   PANDEMIC_ASSIGN_OR_RETURN(*data.mutable_str(), MaybeGetValue(arg));
//   // No field "str" is added on error.
//
// Example: Assigning to a std::unique_ptr.
//   PANDEMIC_ASSIGN_OR_RETURN(std::unique_ptr<T> ptr, MaybeGetPtr(arg));
//
// If passed, the `error_expression` is evaluated to produce the return
// value. The expression may reference any variable visible in scope, as
// well as a `pandemic::StatusBuilder` object populated with the error and
// named by a single underscore `_`. The expression typically uses the
// builder to modify the status and is returned directly in manner similar
// to PANDEMIC_RETURN_IF_ERROR. The expression may, however, evaluate to any
// type returnable by the function, including (void). For example:
//
// Example: Adjusting the error message.
//   PANDEMIC_ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//                    _ << "while processing query " << query.DebugString());
//
// Example: Logging the error on failure.
//   PANDEMIC_ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(query),
//   _.LogError());
//
#define PANDEMIC_ASSIGN_OR_RETURN(...)                              \
  PANDEMIC_STATUS_MACROS_IMPL_GET_VARIADIC_(                        \
      __VA_ARGS__, PANDEMIC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_, \
      PANDEMIC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_)              \
  (__VA_ARGS__)

// =================================================================
// == Implementation details, do not rely on anything below here. ==
// =================================================================

#define PANDEMIC_STATUS_MACROS_IMPL_GET_VARIADIC_(_1, _2, _3, NAME, ...) NAME

#define PANDEMIC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_(lhs, rexpr) \
  PANDEMIC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_(lhs, rexpr, std::move(_))
#define PANDEMIC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_3_(lhs, rexpr,         \
                                                        error_expression)   \
  PANDEMIC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(                            \
      PANDEMIC_STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __LINE__), lhs, \
      rexpr, error_expression)
#define PANDEMIC_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(statusor, lhs, rexpr,  \
                                                      error_expression)      \
  auto statusor = (rexpr);                                                   \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {                                  \
    ::pandemic::StatusBuilder _(std::move(statusor).status(), PANDEMIC_LOC); \
    (void)_; /* error_expression is allowed to not use this variable */      \
    return (error_expression);                                               \
  }                                                                          \
  lhs = std::move(statusor).ValueOrDie()

// Internal helper for concatenating macro values.
#define PANDEMIC_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y) x##y
#define PANDEMIC_STATUS_MACROS_IMPL_CONCAT_(x, y) \
  PANDEMIC_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y)

// The GNU compiler emits a warning for code like:
//
//   if (foo)
//     if (bar) { } else baz;
//
// because it thinks you might want the else to bind to the first if.  This
// leads to problems with code like:
//
//   if (do_expr) PANDEMIC_RETURN_IF_ERROR(expr) << "Some message";
//
// The "switch (0) case 0:" idiom is used to suppress this.
#define PANDEMIC_STATUS_MACROS_IMPL_ELSE_BLOCKER_ \
  switch (0)                                      \
  case 0:                                         \
  default:  // NOLINT

namespace pandemic {
namespace status_macro_internal {

// Provides a conversion to bool so that it can be used inside an if statement
// that declares a variable.
class StatusAdaptorForMacros {
 public:
  StatusAdaptorForMacros(const absl::Status& status, SourceLocation loc)
      : builder_(status, loc) {}

  StatusAdaptorForMacros(absl::Status&& status, SourceLocation loc)
      : builder_(std::move(status), loc) {}

  StatusAdaptorForMacros(const StatusBuilder& builder, SourceLocation loc)
      : builder_(builder) {}

  StatusAdaptorForMacros(StatusBuilder&& builder, SourceLocation loc)
      : builder_(std::move(builder)) {}

  StatusAdaptorForMacros(const StatusAdaptorForMacros&) = delete;
  StatusAdaptorForMacros& operator=(const StatusAdaptorForMacros&) = delete;

  explicit operator bool() const { return ABSL_PREDICT_TRUE(builder_.ok()); }

  pandemic::StatusBuilder&& Consume() { return std::move(builder_); }

 private:
  pandemic::StatusBuilder builder_;
};

}  // namespace status_macro_internal
}  // namespace pandemic

#endif  // THIRD_PARTY_AGENT_BASED_EPIDEMIC_SIM_PORT_DEPS_STATUS_MACROS_H_
