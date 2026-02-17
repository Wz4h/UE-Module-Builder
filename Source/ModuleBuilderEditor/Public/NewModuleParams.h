#pragma once

#include "CoreMinimal.h"

/**
 * 模块生成目标类型
 */
enum class EModuleTargetType : uint8
{
	// 生成到当前工程的 Source 目录
	Project,

	// 生成到某个工程插件的 Source 目录
	ProjectPlugin,
};

/**
 * 新建模块所需参数
 * 由 SAddModuleWindow 收集，传递给 ModuleBuilderEditorModule 处理
 */
struct FNewModuleParams
{
	// 模块名称
	FString ModuleName;

	// 模块类型（Runtime / Editor / Developer / ...）
	FString ModuleType;

	// 加载阶段（Default / PostEngineInit / ...）
	FString LoadingPhase;

	// 目标类型
	EModuleTargetType TargetType = EModuleTargetType::Project;

	// 当 TargetType = ProjectPlugin 时有效
	FString TargetPluginName;
};
