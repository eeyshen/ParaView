/*=========================================================================

  Program:   ParaView
  Module:    vtkSMCoreUtilities.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMCoreUtilities.h"

#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkSMDomain.h"
#include "vtkSMDomainIterator.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxy.h"
#include "vtkSmartPointer.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <sstream>

vtkStandardNewMacro(vtkSMCoreUtilities);
//----------------------------------------------------------------------------
vtkSMCoreUtilities::vtkSMCoreUtilities()
{
}

//----------------------------------------------------------------------------
vtkSMCoreUtilities::~vtkSMCoreUtilities()
{
}

//----------------------------------------------------------------------------
const char* vtkSMCoreUtilities::GetFileNameProperty(vtkSMProxy* proxy)
{
  if (!proxy)
  {
    return NULL;
  }

  if (proxy->GetHints())
  {
    vtkPVXMLElement* filenameHint =
      proxy->GetHints()->FindNestedElementByName("DefaultFileNameProperty");
    if (filenameHint && filenameHint->GetAttribute("name") &&
      proxy->GetProperty(filenameHint->GetAttribute("name")))
    {
      return filenameHint->GetAttribute("name");
    }
  }

  // Find the first property that has a vtkSMFileListDomain. Assume that
  // it is the property used to set the filename.
  vtkSmartPointer<vtkSMPropertyIterator> piter;
  piter.TakeReference(proxy->NewPropertyIterator());
  piter->Begin();
  while (!piter->IsAtEnd())
  {
    vtkSMProperty* prop = piter->GetProperty();
    if (prop && prop->IsA("vtkSMStringVectorProperty"))
    {
      vtkSmartPointer<vtkSMDomainIterator> diter;
      diter.TakeReference(prop->NewDomainIterator());
      diter->Begin();
      while (!diter->IsAtEnd())
      {
        if (diter->GetDomain()->IsA("vtkSMFileListDomain"))
        {
          return piter->GetKey();
        }
        diter->Next();
      }
      if (!diter->IsAtEnd())
      {
        break;
      }
    }
    piter->Next();
  }
  return NULL;
}

//----------------------------------------------------------------------------
// This is reimplemented in python's paraview.make_name_valid(). Keep both
// implementations consistent.
vtkStdString vtkSMCoreUtilities::SanitizeName(const char* name)
{
  if (!name || name[0] == '\0')
  {
    return vtkStdString();
  }

  std::ostringstream cname;
  for (size_t cc = 0; name[cc]; cc++)
  {
    if (isalnum(name[cc]) || name[cc] == '_')
    {
      cname << name[cc];
    }
  }
  // if first character is not an alphabet, add an 'a' to it.
  if (cname.str().empty() || isalpha(cname.str()[0]))
  {
    return cname.str();
  }
  else
  {
    return "a" + cname.str();
  }
}

//----------------------------------------------------------------------------
bool vtkSMCoreUtilities::AdjustRangeForLog(double range[2])
{
  assert(range[0] <= range[1]);
  if (range[0] <= 0.0 || range[1] <= 0.0)
  {
    // ranges not valid for log-space. Cannot convert.
    if (range[1] <= 0.0)
    {
      range[0] = 1.0e-4;
      range[1] = 1.0;
    }
    else
    {
      range[0] = range[1] * 0.0001;
      range[0] = (range[0] < 1.0) ? range[0] : 1.0;
    }
    return true;
  }
  return false;
}

namespace
{

template <typename T>
struct MinDelta
{
};
// This value seems to work well for float ranges we have tested
template <>
struct MinDelta<float>
{
  static const int value = 2048;
};
template <>
struct MinDelta<double>
{
  static const vtkTypeInt64 value = static_cast<vtkTypeInt64>(2048);
};

// Reperesents the following:
// T m = std::numeric_limits<T>::min();
// EquivSizeIntT im;
// std::memcpy(&im, &m, sizeof(T));
//
template <typename EquivSizeIntT>
struct MinRepresentable
{
};
template <>
struct MinRepresentable<float>
{
  static const int value = 8388608;
};
template <>
struct MinRepresentable<double>
{
  static const vtkTypeInt64 value = 4503599627370496L;
};

//----------------------------------------------------------------------------
template <typename T, typename EquivSizeIntT>
bool AdjustTRange(T range[2], EquivSizeIntT, EquivSizeIntT ulpsDiff = MinDelta<T>::value)
{
  if (range[1] < range[0])
  {
    // invalid range.
    return false;
  }

  const bool spans_zero_boundary = range[0] < 0 && range[1] > 0;
  if (spans_zero_boundary)
  { // nothing needs to be done, but this check is required.
    // if we convert into integer space the delta difference will overflow
    // an integer
    return false;
  }

  EquivSizeIntT irange[2];
  // needs to be a memcpy to avoid strict aliasing issues, doing a count
  // of 2*sizeof(T) to couple both values at the same time
  std::memcpy(irange, range, sizeof(T) * 2);

  const bool denormal = !std::isnormal(range[0]);
  const EquivSizeIntT minInt = MinRepresentable<T>::value;
  const EquivSizeIntT minDelta = denormal ? minInt + ulpsDiff : ulpsDiff;

  // determine the absolute delta between these two numbers.
  const EquivSizeIntT delta = std::abs(irange[1] - irange[0]);

  // if our delta is smaller than the min delta push out the max value
  // so that it is equal to minRange + minDelta. When our range is entirely
  // negative we should instead subtract from our max, to max a larger negative
  // value
  if (delta < minDelta)
  {
    if (irange[0] < 0)
    {
      irange[1] = irange[0] - minDelta;
    }
    else
    {
      irange[1] = irange[0] + minDelta;
    }
    std::memcpy(range, irange, sizeof(T) * 2);
    return true;
  }
  return false;
}
}

//----------------------------------------------------------------------------
bool vtkSMCoreUtilities::AlmostEqual(const double range[2], int ulpsDiff)
{
  double trange[2] = { range[0], range[1] };
  return AdjustTRange(trange, vtkTypeInt64(), vtkTypeInt64(ulpsDiff));
}

//----------------------------------------------------------------------------
bool vtkSMCoreUtilities::AdjustRange(double range[2])
{
  // If the numbers are not nearly equal, we don't touch them. This avoids running into
  // pitfalls like BUG #17152.
  if (!vtkSMCoreUtilities::AlmostEqual(range, 1024))
  {
    return false;
  }

  // Since the range is 0-range, we will offset range[1]. We've found it best to offset
  // it in float space, if possible.
  if (range[0] > VTK_FLOAT_MIN && range[0] < VTK_FLOAT_MAX)
  {
    float frange[2] = { static_cast<float>(range[0]), static_cast<float>(range[1]) };
    bool result = AdjustTRange(frange, vtkTypeInt32());
    if (result)
    { // range should be left untouched to avoid loss of precision when no
      // adjustment was needed
      range[1] = static_cast<double>(frange[1]);
    }
    return result;
  }

  return AdjustTRange(range, vtkTypeInt64());
}

//----------------------------------------------------------------------------
void vtkSMCoreUtilities::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
