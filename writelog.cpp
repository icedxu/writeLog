#include "stdafx.h"


#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif

LIST_ENTRY logListHeader;
KSPIN_LOCK HidePathListLock;
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

  //获取进程名称偏移
  ProcessNameOffset=GetProcessNameOffset();

   KeInitializeEvent(&s_Event,SynchronizationEvent,FALSE);
   StartThread();

  return STATUS_SUCCESS;
}
#ifdef __cplusplus
}; // extern "C"
#endif

NTSTATUS FilterUnload(__in FLT_FILTER_UNLOAD_FLAGS Flags)
{
	FltUnregisterFilter(gFilterHandle);
    FLAG = FALSE;
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
            PCHAR procName=GetCurrentProcessName(ProcessNameOffset);
            UNICODE_STRING unicodeBuffer;
            RtlInitUnicodeString(&unicodeBuffer,L"");
            CHAR_TO_UNICODE_STRING(procName,&unicodeBuffer); //将进程名转为UNICODE_STRING

            UNICODE_STRING  tmpTail;
            RtlInitUnicodeString( &tmpTail, L";\r\n"); //用来结尾

            ULONG len = unicodeBuffer.MaximumLength + nameInfo->Name.MaximumLength + tmpTail.MaximumLength;

            PLOG_LIST logListNode;
            logListNode = (PLOG_LIST)ExAllocatePool(NonPagedPool,sizeof(LOG_LIST));
            if (logListNode == NULL)
            {
              KdPrint(("队列申请失败  \n"));  
            }

            logListNode->msg.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, len, LOG_MSG);
            logListNode->msg.Length = 0;
            logListNode->msg.MaximumLength = len;

            RtlAppendUnicodeStringToString(&logListNode->msg,&unicodeBuffer);
            RtlAppendUnicodeToString(&logListNode->msg,L";");
            RtlAppendUnicodeStringToString(&logListNode->msg,&nameInfo->Name);
            RtlAppendUnicodeStringToString(&logListNode->msg,&tmpTail);

          //  KdPrint(("&logListNode->msg = %wZ\n",&logListNode->msg));
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
                PCHAR procName=GetCurrentProcessName(ProcessNameOffset);
                UNICODE_STRING unicodeBuffer;
                RtlInitUnicodeString(&unicodeBuffer,L"");
                CHAR_TO_UNICODE_STRING(procName,&unicodeBuffer); //将进程名转为UNICODE_STRING

                UNICODE_STRING  tmpTail;
                RtlInitUnicodeString( &tmpTail, L";\r\n"); //用来结尾

                ULONG len = unicodeBuffer.MaximumLength + nameInfo->Name.MaximumLength + tmpTail.MaximumLength;


                PLOG_LIST logListNode;
                logListNode = (PLOG_LIST)ExAllocatePool(NonPagedPool,sizeof(LOG_LIST));
                if (logListNode == NULL)
                {
                  KdPrint(("队列申请失败  \n"));  
                }

                logListNode->msg.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, len, LOG_MSG);
                logListNode->msg.Length = 0;
                logListNode->msg.MaximumLength = len;

                RtlAppendUnicodeStringToString(&logListNode->msg,&unicodeBuffer);
                RtlAppendUnicodeToString(&logListNode->msg,L";");
                RtlAppendUnicodeStringToString(&logListNode->msg,&nameInfo->Name);
                RtlAppendUnicodeStringToString(&logListNode->msg,&tmpTail);

                KdPrint(("&logListNode->msg = %wZ\n",&logListNode->msg));
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







 //获取进程名
PCHAR
GetCurrentProcessName(ULONG ProcessNameOffset)
{
    PEPROCESS       curproc;
    char            *nameptr;
    ULONG           i;
 
    //
    // We only try and get the name if we located the name offset
    //
    if( ProcessNameOffset ) {
    
        //
        // Get a pointer to the current process block
        //
        curproc = PsGetCurrentProcess();
 
        //
        // Dig into it to extract the name. Make sure to leave enough room
        // in the buffer for the appended process ID.
        //
        nameptr   = (PCHAR) curproc + ProcessNameOffset;
		/*
		#if defined(_M_IA64)
        sprintf( szName + strlen(szName), ":%I64d", PsGetCurrentProcessId());
		#else
        sprintf( szName + strlen(szName), ":%d", (ULONG) PsGetCurrentProcessId());
		#endif
		//*/
 
    } else {
		
       nameptr="";
    }
    return nameptr;
}



/************************************************************************/
/*    获取进程名称偏移                                                 */
/************************************************************************/
//////////////////////////////////////////////////////////////////////////
//获取进程名称


ULONG 
  GetProcessNameOffset(
  VOID
  )
{
  PEPROCESS       curproc;
  ULONG             i;

  curproc = PsGetCurrentProcess();

  //
  // Scan for 12KB, hopping the KPEB never grows that big!
  //
  for( i = 0; i < 3*PAGE_SIZE; i++ ) 
  {

    if( !strncmp( "System", (PCHAR) curproc + i, strlen("System") )) 
    {

      return i;
    }
  }
  //
  // Name not found - oh, well
  //
  return 0;
}


VOID CHAR_TO_UNICODE_STRING(PCHAR ch,PUNICODE_STRING unicodeBuffer)
{
    ANSI_STRING ansiBuffer;
    UNICODE_STRING buffer_proc;
    ULONG len = strlen(ch);
    KdPrint(("len = %ld\n",len));
    ansiBuffer.Buffer = ch;
    ansiBuffer.Length = ansiBuffer.MaximumLength = (USHORT)len;
    RtlAnsiStringToUnicodeString(unicodeBuffer,&ansiBuffer,TRUE);

   // DbgPrint("ansiBuffer = %Z\n",&ansiBuffer);  
    //DbgPrint("unicodeBuffer = %wZ\n",unicodeBuffer); 

}