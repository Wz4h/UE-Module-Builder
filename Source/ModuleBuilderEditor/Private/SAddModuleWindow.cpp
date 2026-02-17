#include "SAddModuleWindow.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "ModuleBuilderWindow"

static TSharedRef<SWidget> MakeComboItemWidget(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
}

void SAddModuleWindow::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	OnConfirm    = InArgs._OnConfirm;
	OnCancel     = InArgs._OnCancel;

	InitOptions();
	RefreshProjectPlugins();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(12)
		[
			SNew(SVerticalBox)

			// 标题
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 10)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "添加 C++ 模块"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
			]

			// 模块名称
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ModuleNameLabel", "模块名称"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			[
				SAssignNew(ModuleNameText, SEditableTextBox)
				.HintText(LOCTEXT("ModuleNameHint", "例如：MyGameplay 或 MyPluginEditor"))
			]

			// 目标类型
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TargetLabel", "目标位置"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&TargetTypeOptions)
				.InitiallySelectedItem(SelectedTargetType)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type)
				{
					SelectedTargetType = NewItem;

					if (SelectedTargetType.IsValid() && *SelectedTargetType == TEXT("ProjectPlugin"))
					{
						RefreshProjectPlugins();
						if (!SelectedProjectPlugin.IsValid() && ProjectPluginOptions.Num() > 0)
						{
							SelectedProjectPlugin = ProjectPluginOptions[0];
						}
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return MakeComboItemWidget(Item); })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return SelectedTargetType.IsValid()
							? FText::FromString(*SelectedTargetType)
							: LOCTEXT("TargetSelectHint", "请选择");
					})
				]
			]

			// 项目插件选择
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this](){ return GetPluginPickerVisibility(); })
				.Text(LOCTEXT("ProjectPluginLabel", "项目插件"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 12)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.Visibility_Lambda([this](){ return GetPluginPickerVisibility(); })
				.OptionsSource(&ProjectPluginOptions)
				.InitiallySelectedItem(SelectedProjectPlugin)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type)
				{
					SelectedProjectPlugin = NewItem;
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return MakeComboItemWidget(Item); })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return SelectedProjectPlugin.IsValid()
							? FText::FromString(*SelectedProjectPlugin)
							: LOCTEXT("PluginSelectHint", "请选择插件");
					})
				]
			]

			// 模块类型
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ModuleTypeLabel", "模块类型"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ModuleTypeOptions)
				.InitiallySelectedItem(SelectedModuleType)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type)
				{
					SelectedModuleType = NewItem;
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return MakeComboItemWidget(Item); })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return SelectedModuleType.IsValid()
							? FText::FromString(*SelectedModuleType)
							: LOCTEXT("ModuleTypeSelectHint", "请选择");
					})
				]
			]

			// 加载阶段
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LoadingPhaseLabel", "加载阶段"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&LoadingPhaseOptions)
				.InitiallySelectedItem(SelectedLoadingPhase)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type)
				{
					SelectedLoadingPhase = NewItem;
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return MakeComboItemWidget(Item); })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return SelectedLoadingPhase.IsValid()
							? FText::FromString(*SelectedLoadingPhase)
							: LOCTEXT("LoadingPhaseSelectHint", "请选择");
					})
				]
			]

			// 按钮
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(6, 0))

				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelButton", "取消"))
					.OnClicked(this, &SAddModuleWindow::HandleCancel)
				]

				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("ConfirmButton", "确定"))
					.OnClicked(this, &SAddModuleWindow::HandleConfirm)
				]
			]
		]
	];
}

void SAddModuleWindow::InitOptions()
{
	ModuleTypeOptions = {
		MakeShared<FString>(TEXT("Runtime")),
		MakeShared<FString>(TEXT("Editor")),
		MakeShared<FString>(TEXT("Developer")),
		MakeShared<FString>(TEXT("RuntimeNoCommandlet")),
		MakeShared<FString>(TEXT("EditorNoCommandlet")),
	};
	SelectedModuleType = ModuleTypeOptions[0];

	LoadingPhaseOptions = {
		MakeShared<FString>(TEXT("Default")),
		MakeShared<FString>(TEXT("PostEngineInit")),
		MakeShared<FString>(TEXT("PreDefault")),
		MakeShared<FString>(TEXT("PostDefault")),
	};
	SelectedLoadingPhase = LoadingPhaseOptions[0];

	TargetTypeOptions = {
		MakeShared<FString>(TEXT("Project")),
		MakeShared<FString>(TEXT("ProjectPlugin")),
	};
	SelectedTargetType = TargetTypeOptions[0];
}

void SAddModuleWindow::RefreshProjectPlugins()
{
	ProjectPluginOptions.Reset();
	SelectedProjectPlugin.Reset();

	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (Plugin->GetType() == EPluginType::Project)
		{
			ProjectPluginOptions.Add(MakeShared<FString>(Plugin->GetName()));
		}
	}

	if (ProjectPluginOptions.Num() > 0)
	{
		SelectedProjectPlugin = ProjectPluginOptions[0];
	}
}

EVisibility SAddModuleWindow::GetPluginPickerVisibility() const
{
	if (SelectedTargetType.IsValid() && *SelectedTargetType == TEXT("ProjectPlugin"))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FReply SAddModuleWindow::HandleConfirm()
{
	FNewModuleParams Params;

	Params.ModuleName = ModuleNameText.IsValid()
		? ModuleNameText->GetText().ToString().TrimStartAndEnd()
		: TEXT("");

	Params.ModuleType   = SelectedModuleType.IsValid()   ? *SelectedModuleType   : TEXT("Runtime");
	Params.LoadingPhase = SelectedLoadingPhase.IsValid() ? *SelectedLoadingPhase : TEXT("Default");

	if (SelectedTargetType.IsValid() && *SelectedTargetType == TEXT("ProjectPlugin"))
	{
		Params.TargetType = EModuleTargetType::ProjectPlugin;
		Params.TargetPluginName = SelectedProjectPlugin.IsValid() ? *SelectedProjectPlugin : TEXT("");
	}
	else
	{
		Params.TargetType = EModuleTargetType::Project;
	}

	if (Params.ModuleName.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("ModuleNameEmpty", "模块名称不能为空。"));
		return FReply::Handled();
	}

	if (Params.TargetType == EModuleTargetType::ProjectPlugin && Params.TargetPluginName.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("PluginNotSelected", "请选择目标项目插件。"));
		return FReply::Handled();
	}

	if (!OnConfirm.IsBound())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("DelegateNotBound", "内部错误：确认回调未绑定。"));
		return FReply::Handled();
	}

	const bool bOk = OnConfirm.Execute(Params);

	if (!bOk)
	{
		return FReply::Handled();
	}

	if (TSharedPtr<SWindow> W = ParentWindow.Pin())
	{
		W->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SAddModuleWindow::HandleCancel()
{
	if (OnCancel.IsBound())
	{
		OnCancel.Execute();
	}
	if (TSharedPtr<SWindow> W = ParentWindow.Pin())
	{
		W->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
