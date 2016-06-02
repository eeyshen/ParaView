/*=========================================================================

  Program:   ParaView
  Module:    vtkPVLastSelectionInformation.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkPVLastSelectionInformation
// .SECTION Description
// vtkPVLastSelectionInformation is used to obtain the LastSelection from
// vtkPVRenderView.

#ifndef vtkPVLastSelectionInformation_h
#define vtkPVLastSelectionInformation_h

#include "vtkPVClientServerCoreRenderingModule.h" //needed for exports
#include "vtkPVSelectionInformation.h"

class VTKPVCLIENTSERVERCORERENDERING_EXPORT vtkPVLastSelectionInformation : public vtkPVSelectionInformation
{
public:
  static vtkPVLastSelectionInformation* New();
  vtkTypeMacro(vtkPVLastSelectionInformation, vtkPVSelectionInformation);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual void CopyFromObject(vtkObject*);

protected:
  vtkPVLastSelectionInformation();
  ~vtkPVLastSelectionInformation();

private:
  vtkPVLastSelectionInformation(const vtkPVLastSelectionInformation&); // Not implemented
  void operator=(const vtkPVLastSelectionInformation&); // Not implemented

};

#endif
