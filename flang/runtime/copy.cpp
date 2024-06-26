//===-- runtime/copy.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "copy.h"
#include "terminator.h"
#include "type-info.h"
#include "flang/Runtime/allocatable.h"
#include "flang/Runtime/descriptor.h"
#include <cstring>

namespace Fortran::runtime {
RT_OFFLOAD_API_GROUP_BEGIN

RT_API_ATTRS void CopyElement(const Descriptor &to, const SubscriptValue toAt[],
    const Descriptor &from, const SubscriptValue fromAt[],
    Terminator &terminator) {
  char *toPtr{to.Element<char>(toAt)};
  char *fromPtr{from.Element<char>(fromAt)};
  RUNTIME_CHECK(terminator, to.ElementBytes() == from.ElementBytes());
  std::memcpy(toPtr, fromPtr, to.ElementBytes());
  // Deep copy allocatable and automatic components if any.
  if (const auto *addendum{to.Addendum()}) {
    if (const auto *derived{addendum->derivedType()};
        derived && !derived->noDestructionNeeded()) {
      RUNTIME_CHECK(terminator,
          from.Addendum() && derived == from.Addendum()->derivedType());
      const Descriptor &componentDesc{derived->component()};
      const typeInfo::Component *component{
          componentDesc.OffsetElement<typeInfo::Component>()};
      std::size_t nComponents{componentDesc.Elements()};
      for (std::size_t j{0}; j < nComponents; ++j, ++component) {
        if (component->genre() == typeInfo::Component::Genre::Allocatable ||
            component->genre() == typeInfo::Component::Genre::Automatic) {
          Descriptor &toDesc{
              *reinterpret_cast<Descriptor *>(toPtr + component->offset())};
          if (toDesc.raw().base_addr != nullptr) {
            toDesc.set_base_addr(nullptr);
            RUNTIME_CHECK(terminator, toDesc.Allocate() == CFI_SUCCESS);
            const Descriptor &fromDesc{*reinterpret_cast<const Descriptor *>(
                fromPtr + component->offset())};
            CopyArray(toDesc, fromDesc, terminator);
          }
        } else if (component->genre() == typeInfo::Component::Genre::Data &&
            component->derivedType() &&
            !component->derivedType()->noDestructionNeeded()) {
          SubscriptValue extents[maxRank];
          const typeInfo::Value *bounds{component->bounds()};
          for (int dim{0}; dim < component->rank(); ++dim) {
            SubscriptValue lb{bounds[2 * dim].GetValue(&to).value_or(0)};
            SubscriptValue ub{bounds[2 * dim + 1].GetValue(&to).value_or(0)};
            extents[dim] = ub >= lb ? ub - lb + 1 : 0;
          }
          const typeInfo::DerivedType &compType{*component->derivedType()};
          StaticDescriptor<maxRank, true, 0> toStaticDescriptor;
          Descriptor &toCompDesc{toStaticDescriptor.descriptor()};
          toCompDesc.Establish(compType, toPtr + component->offset(),
              component->rank(), extents);
          StaticDescriptor<maxRank, true, 0> fromStaticDescriptor;
          Descriptor &fromCompDesc{fromStaticDescriptor.descriptor()};
          fromCompDesc.Establish(compType, fromPtr + component->offset(),
              component->rank(), extents);
          CopyArray(toCompDesc, fromCompDesc, terminator);
        }
      }
    }
  }
}

RT_API_ATTRS void CopyArray(
    const Descriptor &to, const Descriptor &from, Terminator &terminator) {
  std::size_t elements{to.Elements()};
  RUNTIME_CHECK(terminator, elements == from.Elements());
  SubscriptValue toAt[maxRank], fromAt[maxRank];
  to.GetLowerBounds(toAt);
  from.GetLowerBounds(fromAt);
  while (elements-- > 0) {
    CopyElement(to, toAt, from, fromAt, terminator);
    to.IncrementSubscripts(toAt);
    from.IncrementSubscripts(fromAt);
  }
}

RT_OFFLOAD_API_GROUP_END
} // namespace Fortran::runtime
