/*=========================================================================

  Program:   ParaView
  Module:    vtkPVProcessModule.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVProcessModule.h"

#include "vtkCallbackCommand.h"
#include "vtkCharArray.h"
#include "vtkDataSet.h"
#include "vtkDoubleArray.h"
#include "vtkDummyController.h"
#include "vtkFloatArray.h"
#include "vtkLongArray.h"
#include "vtkMapper.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPVConfig.h"
#include "vtkPolyData.h"
#include "vtkShortArray.h"
#include "vtkSource.h"
#include "vtkString.h"
#include "vtkStringList.h"
#include "vtkToolkits.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkClientServerStream.h"
#include "vtkClientServerInterpreter.h"
#include "vtkTimerLog.h"
#include "vtkProcessModuleGUIHelper.h"
#include "vtkPVServerInformation.h"
#include "vtkInstantiator.h"
#include "vtkPVOptions.h"

#include "vtkPVDemoPaths.h"

#include "vtkKWDirectoryUtilities.h"
#include <sys/stat.h>

// initialze the class variables
int vtkPVProcessModule::GlobalLODFlag = 0;


//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVProcessModule);
vtkCxxRevisionMacro(vtkPVProcessModule, "1.20");

//----------------------------------------------------------------------------
vtkPVProcessModule::vtkPVProcessModule()
{
  this->MPIMToNSocketConnectionID.ID = 0;
  this->LogThreshold = 0;
  this->DemoPath = 0;
  this->ServerInformation = vtkPVServerInformation::New();
  this->UseTriangleStrips = 0;
  this->UseImmediateMode = 1;
  this->Options = 0;
}

//----------------------------------------------------------------------------
vtkPVProcessModule::~vtkPVProcessModule()
{ 
  this->SetDemoPath(0);
  this->FinalizeInterpreter();
  this->ServerInformation->Delete();
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::Start(int argc, char **argv)
{
  if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for vtkPVProcessModule to be able to run a gui.");
    return -1;
    }
  
  if (this->Controller == NULL)
    {
    this->Controller = vtkDummyController::New();
    vtkMultiProcessController::SetGlobalController(this->Controller);
    }
  return this->GUIHelper->RunGUIStart(argc, argv, 1, 0);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::Exit()
{
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetDirectoryListing(const char* dir,
                                            vtkStringList* dirs,
                                            vtkStringList* files,
                                            int save)
{
  // Get the listing from the server.
  vtkClientServerID lid = this->NewStreamObject("vtkPVServerFileListing");
  this->GetStream() << vtkClientServerStream::Invoke
                    << lid << "GetFileListing" << dir << save
                    << vtkClientServerStream::End;
  this->SendStream(vtkProcessModule::DATA_SERVER_ROOT);
  vtkClientServerStream result;
  if(!this->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT).GetArgument(0, 0, &result))
    {
    vtkErrorMacro("Error getting file list result from server.");
    this->DeleteStreamObject(lid);
    this->SendStream(vtkProcessModule::DATA_SERVER_ROOT);
    return 0;
    }
  this->DeleteStreamObject(lid);
  this->SendStream(vtkProcessModule::DATA_SERVER_ROOT);

  // Parse the listing.
  if ( dirs )
    {
    dirs->RemoveAllItems();
    }
  if ( files )
    {
    files->RemoveAllItems();
    }
  if(result.GetNumberOfMessages() == 2)
    {
    int i;
    // The first message lists directories.
    if ( dirs )
      {
      for(i=0; i < result.GetNumberOfArguments(0); ++i)
        {
        const char* d;
        if(result.GetArgument(0, i, &d))
          {
          dirs->AddString(d);
          }
        else
          {
          vtkErrorMacro("Error getting directory name from listing.");
          }
        }
      }

    // The second message lists files.
    if ( files )
      {
      for(i=0; i < result.GetNumberOfArguments(1); ++i)
        {
        const char* f;
        if(result.GetArgument(1, i, &f))
          {
          files->AddString(f);
          }
        else
          {
          vtkErrorMacro("Error getting file name from listing.");
          }
        }
      }
    return 1;
    }
  else
    {
    return 0;
    }
}

//----------------------------------------------------------------------------
vtkObjectBase* vtkPVProcessModule::GetObjectFromID(vtkClientServerID id)
{
  return this->Interpreter->GetObjectFromID(id);
}

//----------------------------------------------------------------------------
vtkObjectBase* vtkPVProcessModule::GetObjectFromIntID(unsigned int idin)
{
  vtkClientServerID id;
  id.ID = idin;
  return this->GetObjectFromID(id);
}


//----------------------------------------------------------------------------
void vtkPVProcessModule::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "LogThreshold: " << this->LogThreshold << endl;
  os << indent << "DemoPath: " << (this->DemoPath?this->DemoPath:"(none)") << endl;
  os << indent << "UseTriangleStrips: " << this->UseTriangleStrips << endl;
  os << indent << "UseImmediateMode: " << this->UseImmediateMode << endl;

  os << indent << "Options: ";
  if(this->Options)
    {
    this->Options->PrintSelf(os << endl, indent.GetNextIndent());
    }
  else
    {
    os << "(none)" << endl;
    }

  os << indent << "ServerInformation: ";
  if (this->ServerInformation)
    {
    this->ServerInformation->PrintSelf(os << endl, indent.GetNextIndent());
    }
  else
    {
    os << "(none)" << endl;;
    }
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::InitializeInterpreter()
{
  if(this->Interpreter)
    {
    return;
    }

  this->Superclass::InitializeInterpreter();

}

//----------------------------------------------------------------------------
void vtkPVProcessModule::FinalizeInterpreter()
{
  if(!this->Interpreter)
    {
    return;
    }

  this->Superclass::FinalizeInterpreter();
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::LoadModule(const char* name, const char* directory)
{
  this->GetStream()
    << vtkClientServerStream::Invoke
    << this->GetProcessModuleID()
    << "LoadModuleInternal" << name << directory
    << vtkClientServerStream::End;
  this->SendStream(vtkProcessModule::DATA_SERVER);
  int result = 0;
  if(!this->GetLastResult(vtkProcessModule::DATA_SERVER_ROOT).GetArgument(0, 0, &result))
    {
    vtkErrorMacro("LoadModule could not get result from server.");
    return 0;
    }
  return result;
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::LoadModuleInternal(const char* name,
                                           const char* directory)
{
  const char* paths[] = {directory, 0};
  return this->Interpreter->Load(name, paths);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SendPrepareProgress()
{ 
  if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for SendPrepareProgress.");
    return;
    }
  this->GUIHelper->SendPrepareProgress();
  this->Superclass::SendPrepareProgress();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SendCleanupPendingProgress()
{
  this->Superclass::SendCleanupPendingProgress();
  if ( this->ProgressRequests > 0 )
    {
    return;
    }
 if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for SendCleanupPendingProgress.");
    return;
    }
  this->GUIHelper->SendCleanupPendingProgress();
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SetLocalProgress(const char* filter, int progress)
{
 if(!this->GUIHelper)
    {
    vtkErrorMacro("GUIHelper must be set, for SetLocalProgress.  " << filter << " " << progress);
    return;
    }
 this->GUIHelper->SetLocalProgress(filter, progress);
}


//----------------------------------------------------------------------------
void vtkPVProcessModule::LogStartEvent(char* str)
{
  vtkTimerLog::MarkStartEvent(str);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::LogEndEvent(char* str)
{
  vtkTimerLog::MarkEndEvent(str);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::SetLogBufferLength(int length)
{
  vtkTimerLog::SetMaxEntries(length);
}

//----------------------------------------------------------------------------
void vtkPVProcessModule::ResetLog()
{
  vtkTimerLog::ResetLog();
}
//----------------------------------------------------------------------------
void vtkPVProcessModule::SetEnableLog(int flag)
{
  vtkTimerLog::SetLogging(flag);
}
//----------------------------------------------------------------------------
void vtkPVProcessModule::SetGlobalLODFlag(int val)
{
  if (vtkPVProcessModule::GlobalLODFlag == val)
    {
    return;
    }
  this->GetStream() << vtkClientServerStream::Invoke
                  << this->GetProcessModuleID()
                  << "SetGlobalLODFlagInternal"
                  << val
                  << vtkClientServerStream::End;
  this->SendStream(vtkProcessModule::CLIENT|vtkProcessModule::DATA_SERVER);
}

 
//----------------------------------------------------------------------------
void vtkPVProcessModule::SetGlobalLODFlagInternal(int val)
{
  vtkPVProcessModule::GlobalLODFlag = val;
}



//----------------------------------------------------------------------------
int vtkPVProcessModule::GetGlobalLODFlag()
{
  return vtkPVProcessModule::GlobalLODFlag;
}

//============================================================================
// Stuff that is a part of render-process module.

//-----------------------------------------------------------------------------
const char* vtkPVProcessModule::GetDemoPath()
{
  int found=0;
  char temp1[1024];
  struct stat fs;

  this->SetDemoPath(NULL);

#ifdef _WIN32  

  if (this->GetApplicationInstallationDirectory())
    {
    sprintf(temp1, "%s/Demos/Demo1.pvs",
            this->GetApplicationInstallationDirectory());
    if (stat(temp1, &fs) == 0) 
      {
      sprintf(temp1, "%s/Demos",
              this->GetApplicationInstallationDirectory());
      this->SetDemoPath(temp1);
      found=1;
      }
    }

#else

  vtkKWDirectoryUtilities* util = vtkKWDirectoryUtilities::New();
  const char* selfPath = util->FindSelfPath(
    this->Options->GetArgv0());
  if (selfPath)
    {
    const char* relPath = "../share/paraview-" PARAVIEW_VERSION "/Demos";
    char* newPath = new char[strlen(selfPath)+strlen(relPath)+2];
    sprintf(newPath, "%s/%s", selfPath, relPath);

    char* demoFile = new char[strlen(newPath)+strlen("/Demo1.pvs")+1];
    sprintf(demoFile, "%s/Demo1.pvs", newPath);

    if (stat(demoFile, &fs) == 0) 
      {
      this->SetDemoPath(newPath);
      found = 1;
      }
    delete[] demoFile;
    delete[] newPath;
    }
  util->Delete();

#endif // _WIN32  

  if (!found)
    {
    // Look in binary and installation directories
    const char** dir;
    for(dir=VTK_PV_DEMO_PATHS; !found && *dir; ++dir)
      {
      sprintf(temp1, "%s/Demo1.pvs", *dir);
      if (stat(temp1, &fs) == 0) 
        {
        this->SetDemoPath(*dir);
        found = 1;
        }
      }
    }

  return this->DemoPath;
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetRenderNodePort()
{
  if ( !this->Options )
    {
    return 0;
    }
  return this->Options->GetRenderNodePort();
}

//----------------------------------------------------------------------------
char* vtkPVProcessModule::GetMachinesFileName()
{
  if ( !this->Options )
    {
    return 0;
    }
  return this->Options->GetMachinesFileName();
}

//----------------------------------------------------------------------------
int vtkPVProcessModule::GetClientMode()
{
  if ( !this->Options )
    {
    return 0;
    }
  return this->Options->GetClientMode();
}

//----------------------------------------------------------------------------
// This method leaks memory.  It is a quick and dirty way to set different 
// DISPLAY environment variables on the render server.  I think the string 
// cannot be deleted until paraview exits.  The var should have the form:
// "DISPLAY=amber1"
void vtkPVProcessModule::SetProcessEnvironmentVariable(int processId,
                                                       const char* var)
{
  (void)processId;
  char* envstr = vtkString::Duplicate(var);
  putenv(envstr);
}
