// Copyright (c) 2025-2026, YUKI TAKA. All rights reserved.
// Licensed under the BSD 2-Clause License. See LICENSE file for details.

#include "Libremidi4UE.h"
#include "Libremidi4UELog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FLibremidi4UEModule"

void FLibremidi4UEModule::StartupModule()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE module loaded"));
}

void FLibremidi4UEModule::ShutdownModule()
{
	UE_LOG(LogLibremidi4UE, Log, TEXT("Libremidi4UE module unloaded"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLibremidi4UEModule, Libremidi4UE)
