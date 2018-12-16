#include "stdafx.h"


#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif

LIST_ENTRY logListHeader;
//minifilter 句柄
PFLT_FILTER gFilterHandle;
KEVENT s_Event;
BOOLEAN  FLAG = TRUE;
//进程名的偏移
ULONG  ProcessNameOffset = 0;

#ifdef __cplusplus
extern "C" {
#endif
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
  NTSTATUS status;
  KdPrint(("DriverEntry \n"));
  InitializeListHead(&logListHeader);
  //注册
  status=FltRegisterFilter(DriverObject,
    &FilterRegistration,
    &gFilterHandle);
  if (NT_SUCCESS(status))
  {
    //启动过滤器
    status=FltStartFiltering(gFilterHandle);
    if(!NT_SUCCESS(status))
    {
      FltUnregisterFilter(gFilterHandle);
    }
  } 


   KeInitializeEvent(&s_Event,SynchronizationEvent,FALSE);
   StartThread();

  return STATUS_SUCCESS;
}
#ifdef __cplusplus
}; // extern "C"
#endif

NTSTATUS FilterUnload(__in FLT_FILTER_UNLOAD_FLAGS Flags)
{


  FLAG = FALSE;

  FltUnregisterFilter(gFilterHandle);
  KdPrint(("卸载成功\n"));
   return STATUS_SUCCESS;
}


FLT_PREOP_CALLBACK_STATUS
  preRead(
  __inout PFLT_CALLBACK_DATA Data,
  __in PCFLT_RELATED_OBJECTS FltObjects,
  __deref_out_opt PVOID *CompletionContext
  )
{
 
  NTSTATUS status;
  PFLT_FILE_NAME_INFORMATION nameInfo;
  UNICODE_STRING Directory_Of_Bait_files;
  UNICODE_STRING log_msg;
  UNREFERENCED_PARAMETER( FltObjects );
  UNREFERENCED_PARAMETER( CompletionContext );
  PAGED_CODE();   
  //检查中断级
  if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
  {
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
  }
  __try {	     

    //判断是否文件夹
    BOOLEAN isDir;	
    status = FltIsDirectory(FltObjects->FileObject,FltObjects->Instance,&isDir);
    if (NT_SUCCESS(status))
    {
      //文件夹直接跳过
      if (isDir)
      {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;  //是文件夹直接返回失败
      }
      else
      {
        status = FltGetFileNameInformation( Data,
                                            FLT_FILE_NAME_NORMALIZED |FLT_FILE_NAME_QUERY_DEFAULT,
                                            &nameInfo );
        if (NT_SUCCESS( status )) 
        {
          FltParseFileNameInformation( nameInfo );
          RtlInitUnicodeString( &Directory_Of_Bait_files, L"\\Device\\HarddiskVolume3\\");
          if (RtlPrefixUnicodeString(&Directory_Of_Bait_files,&nameInfo->Name,TRUE))
          {
            //进入了我想要监控的目录
            PEPROCESS obProcess = NULL;
            HANDLE hProcess;
            UNICODE_STRING fullPath;
            obProcess = IoThreadToProcess(Data->Thread);
            hProcess = PsGetProcessId(obProcess);

            fullPath.Length = 0;
            fullPath.MaximumLength = 520;

            fullPath.Buffer = (PWSTR)ExAllocatePoolWithTag(NonPagedPool,520,PROCESS_FULLPATH);
            if (fullPath.Buffer == NULL)
            {
              FltReleaseFileNameInformation( nameInfo ); 
              return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            status = GetProcessImageName(hProcess,&fullPath);
            if (!NT_SUCCESS(status))
            {
              FltReleaseFileNameInformation( nameInfo ); 
              return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            UNICODE_STRING  tmpTail;
            RtlInitUnicodeString( &tmpTail, L";\r\n"); //用来结尾

            ULONG len = fullPath.MaximumLength + nameInfo->Name.MaximumLength + tmpTail.MaximumLength;
            PLOG_LIST logListNode;
            logListNode = (PLOG_LIST)ExAllocatePool(NonPagedPool,sizeof(LOG_LIST));
            if (logListNode == NULL)
            {
              KdPrint(("队列申请失败  \n"));  
            }

            logListNode->msg.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, len, LOG_MSG);
            logListNode->msg.Length = 0;
            logListNode->msg.MaximumLength = len;

            RtlAppendUnicodeStringToString(&logListNode->msg,&fullPath);
            ExFreePoolWithTag(fullPath.Buffer,PROCESS_FULLPATH);  //在将进程全路径赋值给要保存的消息后将内存释放

            RtlAppendUnicodeToString(&logListNode->msg,L";");
            RtlAppendUnicodeStringToString(&logListNode->msg,&nameInfo->Name);
            RtlAppendUnicodeStringToString(&logListNode->msg,&tmpTail);

            //KdPrint(("&logListNode->msg = %wZ\n",&logListNode->msg));
            InsertTailList(&logListHeader,&logListNode->listNode);//插入队尾
            KeSetEvent(&s_Event,IO_NO_INCREMENT,FALSE);
          }
          FltReleaseFileNameInformation( nameInfo ); 
        }
      }//else   
    }//判断是否是文件夹
  }//__try
  __except(EXCEPTION_EXECUTE_HANDLER) {
    DbgPrint("NPPreCreate EXCEPTION_EXECUTE_HANDLER\n");				
  }
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
}



FLT_PREOP_CALLBACK_STATUS
  preWrite(
  __inout PFLT_CALLBACK_DATA Data,
  __in PCFLT_RELATED_OBJECTS FltObjects,
  __deref_out_opt PVOID *CompletionContext
  )
{
  NTSTATUS status;
  PFLT_FILE_NAME_INFORMATION nameInfo;
  UNICODE_STRING Directory_Of_Bait_files;
  UNICODE_STRING log_msg;
  UNREFERENCED_PARAMETER( FltObjects );
  UNREFERENCED_PARAMETER( CompletionContext );
  PAGED_CODE();   
  //检查中断级
  if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
  {
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
  }
  __try {	     

    //判断是否文件夹
    BOOLEAN isDir;	
    status = FltIsDirectory(FltObjects->FileObject,FltObjects->Instance,&isDir);
    if (NT_SUCCESS(status))
    {
      //文件夹直接跳过
      if (isDir)
      {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;  //是文件夹直接返回失败
      }
      else
      {
        status = FltGetFileNameInformation( Data,
          FLT_FILE_NAME_NORMALIZED |
          FLT_FILE_NAME_QUERY_DEFAULT,
          &nameInfo );
        if (NT_SUCCESS( status )) 
        {
          FltParseFileNameInformation( nameInfo );
          RtlInitUnicodeString( &Directory_Of_Bait_files, L"\\Device\\HarddiskVolume3\\");
          if (RtlPrefixUnicodeString(&Directory_Of_Bait_files,&nameInfo->Name,TRUE))
          {
                //进入了我想要监控的目录
            PEPROCESS obProcess = NULL;
            HANDLE hProcess;
            UNICODE_STRING fullPath;
            obProcess = IoThreadToProcess(Data->Thread);
            hProcess = PsGetProcessId(obProcess);

            fullPath.Length = 0;
            fullPath.MaximumLength = 520;

            fullPath.Buffer = (PWSTR)ExAllocatePoolWithTag(NonPagedPool,520,PROCESS_FULLPATH);
            if (fullPath.Buffer == NULL)
            {
                FltReleaseFileNameInformation( nameInfo ); 
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            status = GetProcessImageName(hProcess,&fullPath);
            if (!NT_SUCCESS(status))
            {
              FltReleaseFileNameInformation( nameInfo ); 
              return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            UNICODE_STRING  tmpTail;
            RtlInitUnicodeString( &tmpTail, L";\r\n"); //用来结尾

            ULONG len = fullPath.MaximumLength + nameInfo->Name.MaximumLength + tmpTail.MaximumLength;
            PLOG_LIST logListNode;
            logListNode = (PLOG_LIST)ExAllocatePool(NonPagedPool,sizeof(LOG_LIST));
            if (logListNode == NULL)
            {
                  KdPrint(("队列申请失败  \n"));  
            }

            logListNode->msg.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, len, LOG_MSG);
            logListNode->msg.Length = 0;
            logListNode->msg.MaximumLength = len;

            RtlAppendUnicodeStringToString(&logListNode->msg,&fullPath);
            ExFreePoolWithTag(fullPath.Buffer,PROCESS_FULLPATH);  //在将进程全路径赋值给要保存的消息后将内存释放

            RtlAppendUnicodeToString(&logListNode->msg,L";");
            RtlAppendUnicodeStringToString(&logListNode->msg,&nameInfo->Name);
            RtlAppendUnicodeStringToString(&logListNode->msg,&tmpTail);

            //KdPrint(("&logListNode->msg = %wZ\n",&logListNode->msg));
            InsertTailList(&logListHeader,&logListNode->listNode);//插入队尾
            KeSetEvent(&s_Event,IO_NO_INCREMENT,FALSE);
          }
          FltReleaseFileNameInformation( nameInfo ); 
        }
     }//else   
    }//判断是否是文件夹
  }//__try
  __except(EXCEPTION_EXECUTE_HANDLER) {
    DbgPrint("NPPreCreate EXCEPTION_EXECUTE_HANDLER\n");				
  }
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
}





VOID StartThread()
{
  NTSTATUS status = STATUS_SUCCESS;
  HANDLE   hThread = NULL;
  status = PsCreateSystemThread(  &hThread, //创建新线程
                                  (ACCESS_MASK)THREAD_ALL_ACCESS,
                                  NULL,
                                  NULL,//NtCurrentProcess(),线程所在地址空间的进程的handle
                                  NULL,
                                  (PKSTART_ROUTINE)FltThreadProc,  //在这里可以切换是用Zw系列还是Flt系列
                                  NULL);  //(PVOID)&kEvent    StartContext   对应ZwThreadProc中的参数
  if (!NT_SUCCESS(status))
  {
    KdPrint(("创建失败 \n"));
    ZwClose(hThread);
    return ;
  }
  KdPrint(("创建成功 \n"));
  ZwClose(hThread);
  return ;
}


VOID  ZwThreadProc()  
{  
  DbgPrint("CreateThread Successfully\n");  
  PLOG_LIST logList;
  PLIST_ENTRY pListNode;
  OBJECT_ATTRIBUTES objectAttributes;
  IO_STATUS_BLOCK iostatus;
  HANDLE hfile;
  NTSTATUS  status;
  UNICODE_STRING logFileUnicodeString;
  RtlInitUnicodeString( &logFileUnicodeString, L"\\??\\C:\\1.LOG");
  while(FLAG){
    KeWaitForSingleObject(&s_Event,Executive,KernelMode,FALSE,NULL);
    while (!IsListEmpty(&logListHeader))
    {
      LIST_ENTRY *pEntry = RemoveHeadList(&logListHeader); //得到并移除第一个节点
      logList = CONTAINING_RECORD(pEntry,LOG_LIST,listNode);

      InitializeObjectAttributes(&objectAttributes,
        &logFileUnicodeString,
        OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,//对大小写敏感 
        NULL, 
        NULL );
      status = ZwCreateFile( &hfile,  //创建文件
                              FILE_APPEND_DATA,
                              &objectAttributes, 
                              &iostatus, 
                              NULL,
                              FILE_ATTRIBUTE_NORMAL, 
                              FILE_SHARE_READ,
                              FILE_OPEN_IF,//存在该文件则打开 ,不存在则创建
                              FILE_SYNCHRONOUS_IO_NONALERT, 
                              NULL, 
                              NULL);
      if (!NT_SUCCESS(status))
      {
        KdPrint(("The file is not exist!\n"));
        ExFreePoolWithTag(logList->msg.Buffer,LOG_MSG);
        ExFreePool(logList);
        continue;
      }

      ZwWriteFile(hfile,NULL,NULL,NULL,&iostatus,logList->msg.Buffer,logList->msg.Length,NULL,NULL);
      ZwClose(hfile);
      ExFreePoolWithTag(logList->msg.Buffer,LOG_MSG);
      ExFreePool(logList);
    }
  }

  KdPrint(("线程函数结束\n"));
  //结束自己
  PsTerminateSystemThread(STATUS_SUCCESS);   
  return ;
}  


VOID  FltThreadProc()  
{  
  DbgPrint("CreateThread Successfully\n");  
  PLOG_LIST logList;
  PLIST_ENTRY pListNode;
  OBJECT_ATTRIBUTES objectAttributes;
  IO_STATUS_BLOCK iostatus;
  HANDLE fileHandle = NULL;
  NTSTATUS  status;
  UNICODE_STRING logFileUnicodeString;
  RtlInitUnicodeString( &logFileUnicodeString, L"\\??\\C:\\1.LOG");
  while(FLAG){
    KeWaitForSingleObject(&s_Event,Executive,KernelMode,FALSE,NULL);
    while (!IsListEmpty(&logListHeader))
    {
      LIST_ENTRY *pEntry = RemoveHeadList(&logListHeader); //得到并移除第一个节点
      logList = CONTAINING_RECORD(pEntry,LOG_LIST,listNode);
      InitializeObjectAttributes(&objectAttributes,
        &logFileUnicodeString,
        OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,//对大小写敏感 
        NULL, 
        NULL );

      //获得文件所在盘的实例
      PFLT_INSTANCE fileInstance = NULL;
      UNICODE_STRING  pVolumeNamec;
      RtlInitUnicodeString(&pVolumeNamec, L"\\Device\\HarddiskVolume2");//日志所在的盘  C盘
      fileInstance = XBFltGetVolumeInstance(gFilterHandle,	&pVolumeNamec);
      status = FltCreateFile( gFilterHandle, //驱动的句柄
                              fileInstance,  //卷所在的实例,
                              &fileHandle,   //文件句柄
                              FILE_APPEND_DATA|SYNCHRONIZE,
                              &objectAttributes, 
                              &iostatus, 
                              NULL,
                              FILE_ATTRIBUTE_NORMAL, 
                              FILE_SHARE_READ,
                              FILE_OPEN_IF,//存在该文件则打开 ,不存在则创建
                              FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE,
                              NULL, 
                              NULL,
                              NULL);
      if (!NT_SUCCESS(status))
      {
        KdPrint(("The file is not exist!\n"));
        ExFreePoolWithTag(logList->msg.Buffer,LOG_MSG);
        ExFreePool(logList);
        continue;
      }
      PFILE_OBJECT fileObject = NULL;
      status = ObReferenceObjectByHandle(fileHandle, 0, NULL, KernelMode,
        reinterpret_cast<void **>(&fileObject),
        NULL);

      if (!NT_SUCCESS(status)) 
      {
           KdPrint(("ObReferenceObjectByHandle fail \n"));
           if (fileHandle)
              FltClose(fileHandle);
           ExFreePoolWithTag(logList->msg.Buffer,LOG_MSG);
           ExFreePool(logList);
           continue;
      }

   //  获取文件有多大，之后好计算偏移量
      FILE_STANDARD_INFORMATION fileInfo={};
      status = FltQueryInformationFile(  fileInstance,
                                          fileObject,
                                          &fileInfo,
                                          sizeof(fileInfo),
                                          FileStandardInformation,
                                          NULL);

       if(!NT_SUCCESS(status))
       {
           KdPrint(("FltQueryInformationFile fail \n"));
          if (fileObject) {
              ObDereferenceObject(fileObject);
            }

           if (fileHandle) {
              FltClose(fileHandle);
           }
           ExFreePoolWithTag(logList->msg.Buffer,LOG_MSG);
           ExFreePool(logList);
       }
      status = FltWriteFile(  fileInstance,//
                              fileObject,
                              &fileInfo.EndOfFile,  //偏移
                              logList->msg.Length,
                              logList->msg.Buffer,
                              NULL,
                              NULL,
                              NULL, 
                              NULL);
      if (!NT_SUCCESS(status)) {
        KdPrint(("FltWriteFile fail  \n"));
      }

      if (fileObject) {
        ObDereferenceObject(fileObject);
      }

      if (fileHandle) {
        FltClose(fileHandle);
      }

      ExFreePoolWithTag(logList->msg.Buffer,LOG_MSG);
      ExFreePool(logList);
    }
  }

  KdPrint(("线程函数结束\n"));
  //结束自己
  PsTerminateSystemThread(STATUS_SUCCESS);   
  return ;
}  



//得到对应卷的实例

PFLT_INSTANCE 
  XBFltGetVolumeInstance(
  IN PFLT_FILTER		pFilter,
  IN PUNICODE_STRING	pVolumeName
  )
{
  NTSTATUS		status;
  PFLT_INSTANCE	pInstance = NULL;
  PFLT_VOLUME		pVolumeList[MAX_VOLUME_CHARS];
  BOOLEAN			bDone = FALSE;
  ULONG			uRet;
  UNICODE_STRING	uniName ={0};
  ULONG 			index = 0;
  WCHAR			wszNameBuffer[MAX_PATH] = {0};

  status = FltEnumerateVolumes(pFilter,
    NULL,
    0,
    &uRet);
  if(status != STATUS_BUFFER_TOO_SMALL)
  {
    return NULL;
  }

  status = FltEnumerateVolumes(pFilter,
    pVolumeList,
    uRet,
    &uRet);

  if(!NT_SUCCESS(status))
  {

    return NULL;
  }
  uniName.Buffer = wszNameBuffer;

  if (uniName.Buffer == NULL)
  {
    for (index = 0;index< uRet; index++)
      FltObjectDereference(pVolumeList[index]);

    return NULL;
  }

  uniName.MaximumLength = MAX_PATH*sizeof(WCHAR);

  for (index = 0; index < uRet; index++)
  {
    uniName.Length = 0;

    status = FltGetVolumeName( pVolumeList[index],
      &uniName,
      NULL);

    if(!NT_SUCCESS(status))
      continue;

    if(RtlCompareUnicodeString(&uniName,
      pVolumeName,
      TRUE) != 0)
      continue;

    status = FltGetVolumeInstanceFromName(pFilter,
      pVolumeList[index],
      NULL,
      &pInstance);

    if(NT_SUCCESS(status))
    {
      FltObjectDereference(pInstance);
      break;
    }
  }

  for (index = 0;index< uRet; index++)
    FltObjectDereference(pVolumeList[index]);

  return pInstance;
}


NTSTATUS GetProcessImageName(HANDLE processId, PUNICODE_STRING ProcessImageName)
{
NTSTATUS status;
ULONG returnedLength;
ULONG bufferLength;
HANDLE hProcess;
PVOID buffer;
PEPROCESS eProcess;
PUNICODE_STRING imageName;

PAGED_CODE(); // this eliminates the possibility of the IDLE Thread/Process

status = PsLookupProcessByProcessId(processId, &eProcess);

if(NT_SUCCESS(status))
{
    status = ObOpenObjectByPointer(eProcess,0, NULL, 0,0,KernelMode,&hProcess);
    if(NT_SUCCESS(status))
    {
    } else {
        DbgPrint("ObOpenObjectByPointer Failed: %08x\n", status);
    }
    ObDereferenceObject(eProcess);
} else {
    DbgPrint("PsLookupProcessByProcessId Failed: %08x\n", status);
}


if (NULL == ZwQueryInformationProcess) {

    UNICODE_STRING routineName;

    RtlInitUnicodeString(&routineName, L"ZwQueryInformationProcess");

    ZwQueryInformationProcess =
           (QUERY_INFO_PROCESS) MmGetSystemRoutineAddress(&routineName);

    if (NULL == ZwQueryInformationProcess) {
        DbgPrint("Cannot resolve ZwQueryInformationProcess\n");
    }
}

/* Query the actual size of the process path */
status = ZwQueryInformationProcess( hProcess,
                                    ProcessImageFileName,
                                    NULL, // buffer
                                    0, // buffer size
                                    &returnedLength);

if (STATUS_INFO_LENGTH_MISMATCH != status) {
    return status;
}

/* Check there is enough space to store the actual process
   path when it is found. If not return an error with the
   required size */
bufferLength = returnedLength - sizeof(UNICODE_STRING);
if (ProcessImageName->MaximumLength < bufferLength)
{
    ProcessImageName->MaximumLength = (USHORT) bufferLength;
    return STATUS_BUFFER_OVERFLOW;   
}

/* Allocate a temporary buffer to store the path name */
buffer = ExAllocatePoolWithTag(NonPagedPool, returnedLength, 'uLT1');

if (NULL == buffer) 
{
    return STATUS_INSUFFICIENT_RESOURCES;   
}

/* Retrieve the process path from the handle to the process */
status = ZwQueryInformationProcess( hProcess,
                                    ProcessImageFileName,
                                    buffer,
                                    returnedLength,
                                    &returnedLength);

if (NT_SUCCESS(status)) 
{
    /* Copy the path name */
    imageName = (PUNICODE_STRING) buffer;
    RtlCopyUnicodeString(ProcessImageName, imageName);
}

/* Free the temp buffer which stored the path */
ExFreePoolWithTag(buffer, 'uLT1');
return status;
}