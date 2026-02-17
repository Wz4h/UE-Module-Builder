// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

struct FNewModuleParams;

class FModuleBuilderEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	private:
	void RegisterMenus();
	void OnClickAddModule();
	bool HandleConfirm(const FNewModuleParams& Pramas);
};
