#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "NewModuleParams.h"

class SWindow;
class SEditableTextBox;

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnConfirmModule, const FNewModuleParams&);

class SAddModuleWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAddModuleWindow) {}
	SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_EVENT(FOnConfirmModule, OnConfirm)
	SLATE_EVENT(FSimpleDelegate, OnCancel)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs);

private:
	// 外部传入
	TWeakPtr<SWindow> ParentWindow;
	FOnConfirmModule OnConfirm;
	FSimpleDelegate OnCancel;

	// 输入控件
	TSharedPtr<SEditableTextBox> ModuleNameText;

	// 下拉选项：模块类型、加载阶段
	TArray<TSharedPtr<FString>> ModuleTypeOptions;
	TArray<TSharedPtr<FString>> LoadingPhaseOptions;
	TSharedPtr<FString> SelectedModuleType;
	TSharedPtr<FString> SelectedLoadingPhase;

	// 目标类型下拉（工程 / 工程插件）
	TArray<TSharedPtr<FString>> TargetTypeOptions;
	TSharedPtr<FString> SelectedTargetType; // "Project" / "ProjectPlugin"

	// 工程插件列表下拉（仅当选 ProjectPlugin 时显示）
	TArray<TSharedPtr<FString>> ProjectPluginOptions; // 存插件名
	TSharedPtr<FString> SelectedProjectPlugin;

private:
	void InitOptions();
	void RefreshProjectPlugins();

	// Slate 可见性：只有选了 ProjectPlugin 才显示插件下拉
	EVisibility GetPluginPickerVisibility() const;

	// 按钮
	FReply HandleConfirm();
	FReply HandleCancel();
};
