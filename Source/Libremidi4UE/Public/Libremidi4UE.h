// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include <memory>

namespace libremidi
{
	class observer;
}

class FLibremidi4UEModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	std::unique_ptr<libremidi::observer> MidiObserver;
};
