#pragma once
#include "CoreMinimal.h"

// 新模块参数：UI 负责填它，后面生成文件/改uplugin就用它
#pragma once
#include "CoreMinimal.h"

enum class EModuleTargetType : uint8
{
	Project,
	ProjectPlugin,
};

struct FNewModuleParams
{
	FString ModuleName;
	FString ModuleType;      // Runtime / Editor / ...
	FString LoadingPhase;    // Default / PostEngineInit / ...

	EModuleTargetType TargetType = EModuleTargetType::Project;

	// 当 TargetType=ProjectPlugin 时有效
	FString TargetPluginName;
};
