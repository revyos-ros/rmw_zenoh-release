// Copyright 2023 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <zenoh.hxx>

#include "detail/identifier.hpp"

#include "rcpputils/scope_exit.hpp"

#include "rcutils/allocator.h"
#include "rcutils/strdup.h"
#include "rcutils/types.h"

#include "rmw/impl/cpp/macros.hpp"
#include "rmw/init_options.h"

extern "C"
{
//==============================================================================
/// Initialize given init options with the default values and implementation specific values.
rmw_ret_t
rmw_init_options_init(rmw_init_options_t * init_options, rcutils_allocator_t allocator)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(init_options, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ALLOCATOR(&allocator, return RMW_RET_INVALID_ARGUMENT);
  if (nullptr != init_options->implementation_identifier) {
    RMW_SET_ERROR_MSG("expected zero-initialized init_options");
    return RMW_RET_INVALID_ARGUMENT;
  }

  memset(init_options, 0, sizeof(rmw_init_options_t));
  init_options->instance_id = 0;
  init_options->implementation_identifier = rmw_zenoh_cpp::rmw_zenoh_identifier;
  init_options->allocator = allocator;
  init_options->impl = nullptr;
  init_options->enclave = nullptr;
  init_options->domain_id = RMW_DEFAULT_DOMAIN_ID;
  init_options->security_options = rmw_get_default_security_options();
  init_options->discovery_options = rmw_get_zero_initialized_discovery_options();
  return rmw_discovery_options_init(&(init_options->discovery_options), 0, &allocator);
}

//==============================================================================
/// Copy the given source init options to the destination init options.
rmw_ret_t
rmw_init_options_copy(const rmw_init_options_t * src, rmw_init_options_t * dst)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(src, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(dst, RMW_RET_INVALID_ARGUMENT);
  if (nullptr == src->implementation_identifier) {
    RMW_SET_ERROR_MSG("expected initialized dst");
    return RMW_RET_INVALID_ARGUMENT;
  }
  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    src,
    src->implementation_identifier,
    rmw_zenoh_cpp::rmw_zenoh_identifier,
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION);
  if (nullptr != dst->implementation_identifier) {
    RMW_SET_ERROR_MSG("expected zero-initialized dst");
    return RMW_RET_INVALID_ARGUMENT;
  }
  rcutils_allocator_t allocator = src->allocator;
  RCUTILS_CHECK_ALLOCATOR(&allocator, return RMW_RET_INVALID_ARGUMENT);

  rmw_init_options_t tmp;
  memcpy(&tmp, src, sizeof(rmw_init_options_t));
  tmp.implementation_identifier = rmw_zenoh_cpp::rmw_zenoh_identifier;
  tmp.security_options = rmw_get_zero_initialized_security_options();
  rmw_ret_t ret =
    rmw_security_options_copy(&src->security_options, &allocator, &tmp.security_options);
  if (RMW_RET_OK != ret) {
    return ret;
  }
  auto free_security_options = rcpputils::make_scope_exit(
    [&tmp, allocator]() {
      rmw_security_options_fini(&tmp.security_options, &allocator);
    });
  tmp.discovery_options = rmw_get_zero_initialized_discovery_options();
  ret = rmw_discovery_options_copy(
    &src->discovery_options,
    &allocator,
    &tmp.discovery_options);
  if (RMW_RET_OK != ret) {
    return ret;
  }
  auto free_discovery_options = rcpputils::make_scope_exit(
    [&tmp]() {
      rmw_ret_t tmp_ret = rmw_discovery_options_fini(&tmp.discovery_options);
      static_cast<void>(tmp_ret);
    });
  tmp.enclave = nullptr;
  if (nullptr != src->enclave) {
    tmp.enclave = rcutils_strdup(src->enclave, allocator);
    if (nullptr == tmp.enclave) {
      return RMW_RET_BAD_ALLOC;
    }
  }
  auto free_enclave = rcpputils::make_scope_exit(
    [&tmp, allocator]() {
      if (nullptr != tmp.enclave) {
        allocator.deallocate(tmp.enclave, allocator.state);
      }
    });

  tmp.allocator = src->allocator;
  // If the impl had any structure, this would be wrong, but since it is an empty pointer
  // right now this works.
  tmp.impl = src->impl;

  *dst = tmp;

  free_enclave.cancel();
  free_discovery_options.cancel();
  free_security_options.cancel();

  return RMW_RET_OK;
}

//==============================================================================
/// Finalize the given init options.
rmw_ret_t
rmw_init_options_fini(rmw_init_options_t * init_options)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(init_options, RMW_RET_INVALID_ARGUMENT);
  if (nullptr == init_options->implementation_identifier) {
    RMW_SET_ERROR_MSG("expected initialized init_options");
    return RMW_RET_INVALID_ARGUMENT;
  }
  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    init_options,
    init_options->implementation_identifier,
    rmw_zenoh_cpp::rmw_zenoh_identifier,
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION);
  rcutils_allocator_t * allocator = &init_options->allocator;
  RCUTILS_CHECK_ALLOCATOR(allocator, return RMW_RET_INVALID_ARGUMENT);

  allocator->deallocate(init_options->enclave, allocator->state);
  rmw_ret_t ret = rmw_security_options_fini(&init_options->security_options, allocator);
  if (ret != RMW_RET_OK) {
    return ret;
  }

  ret = rmw_discovery_options_fini(&init_options->discovery_options);
  *init_options = rmw_get_zero_initialized_init_options();

  return ret;
}
}  // extern "C"
