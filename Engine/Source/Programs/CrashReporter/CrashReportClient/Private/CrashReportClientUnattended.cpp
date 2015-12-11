// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "CrashReportClientUnattended.h"
#include "CrashReportUtil.h"
#include "CrashDescription.h"

FCrashReportClientUnattended::FCrashReportClientUnattended(FPlatformErrorReport& InErrorReport)
	: ReceiverUploader(FCrashReportClientConfig::Get().GetReceiverAddress())
	, DataRouterUploader(FCrashReportClientConfig::Get().GetDataRouterURL())
	, ErrorReport(InErrorReport)
{
	ErrorReport.TryReadDiagnosticsFile();

	// Process the report synchronously
	ErrorReport.DiagnoseReport();

	// Update properties for the crash.
	ErrorReport.SetPrimaryCrashProperties( *FPrimaryCrashProperties::Get() );

	StartTicker();
}

bool FCrashReportClientUnattended::Tick(float UnusedDeltaTime)
{
	if (!FCrashUploadBase::IsInitialized())
	{
		FCrashUploadBase::StaticInitialize(ErrorReport);
	}

	if (!ReceiverUploader.IsUploadCalled())
	{
		// Can be called only when we have all files.
		ReceiverUploader.BeginUpload(ErrorReport);
	}

	// IsWorkDone will always return true here (since ReceiverUploader can't finish until the diagnosis has been sent), but it
	//  has the side effect of joining the worker thread.
	if (!ReceiverUploader.IsFinished())
	{
		// More ticks, please
		return true;
	}

	if (!DataRouterUploader.IsUploadCalled())
	{
		// Can be called only when we have all files.
		DataRouterUploader.BeginUpload(ErrorReport);
	}

	// IsWorkDone will always return true here (since DataRouterUploader can't finish until the diagnosis has been sent), but it
	//  has the side effect of joining the worker thread.
	if (!DataRouterUploader.IsFinished())
	{
		// More ticks, please
		return true;
	}

	FPlatformMisc::RequestExit(false /* don't force */);
	return false;
}

void FCrashReportClientUnattended::StartTicker()
{
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCrashReportClientUnattended::Tick), 1.f);
}
