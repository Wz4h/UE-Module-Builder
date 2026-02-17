// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

struct FNewModuleParams;

class FModuleBuilderEditorModule : public IModuleInterface
{
public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OnClickAddModule();

	// 返回 true 表示成功（窗口会关闭），false 表示失败（窗口保持打开）
	bool HandleConfirm(const FNewModuleParams& Params);
};
