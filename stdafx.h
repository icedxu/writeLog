#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifdef __cplusplus
extern "C" 
{
#endif
#include <fltKernel.h>
#include <ntddk.h>
#include <ntddstor.h>
#include <mountdev.h>
#include <ntddvol.h>

#ifdef __cplusplus
};
#endif

#define LOG_MSG  'msg'
#define MAX_VOLUME_CHARS  26
#define MAX_PATH  260


typedef struct _LOG_LIST
{
  LIST_ENTRY listNode;  
  UNICODE_STRING msg;
}LOG_LIST,*PLOG_LIST;




FLT_PREOP_CALLBACK_STATUS
  preWrite(
  __inout PFLT_CALLBACK_DATA Data,
  __in PCFLT_RELATED_OBJECTS FltObjects,
  __deref_out_opt PVOID *CompletionContext
  );


FLT_PREOP_CALLBACK_STATUS
  preRead(
  __inout PFLT_CALLBACK_DATA Data,
  __in PCFLT_RELATED_OBJECTS FltObjects,
  __deref_out_opt PVOID *CompletionContext
  );



NTSTATUS
  FilterUnload (
  __in FLT_FILTER_UNLOAD_FLAGS Flags
  );


VOID  ZwThreadProc();
VOID  FltThreadProc();
VOID  StartThread();
PCHAR GetCurrentProcessName(ULONG ProcessNameOffset);
ULONG GetProcessNameOffset(VOID);
VOID  CHAR_TO_UNICODE_STRING(PCHAR,PUNICODE_STRING);
PFLT_INSTANCE 
  XBFltGetVolumeInstance(
  IN PFLT_FILTER		pFilter,
  IN PUNICODE_STRING	pVolumeName
  );


CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
 
  { IRP_MJ_READ,
  0,
  preRead, 
  NULL,
  },

  { IRP_MJ_WRITE,
  0,
  preWrite, 
  NULL,
  },

  { IRP_MJ_OPERATION_END }
};


CONST FLT_REGISTRATION FilterRegistration = {
  sizeof( FLT_REGISTRATION ),         //  Size
  FLT_REGISTRATION_VERSION,           //  Version
  0,                                  //  Flags
  NULL,			    //  Context
  Callbacks,                          //  Operation callbacks
  FilterUnload,                       //  MiniFilterUnload
  NULL,						//  InstanceSetup
  NULL,				//  InstanceQueryTeardown
  NULL,                               //  InstanceTeardownStart
  NULL,                               //  InstanceTeardownComplete
  NULL,                               //  GenerateFileName
  NULL,                               //  GenerateDestinationFileName
  NULL                                //  NormalizeNameComponent
};


