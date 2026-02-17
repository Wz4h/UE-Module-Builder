#include "SAddModuleWindow.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"


#include "Widgets/SWindow.h"

#include "Interfaces/IPluginManager.h" // ✅ 获取插件列表

static TSharedRef<SWidget> MakeComboItemWidget(TSharedPtr<FString> Item)
{
    return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::FromString(TEXT("")));
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
                .Text(FText::FromString(TEXT("添加模块")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
            ]

            // 模块名
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("模块名（ModuleName）")))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
            [
                SAssignNew(ModuleNameText, SEditableTextBox)
                .HintText(FText::FromString(TEXT("例如：MyGameplay 或 MyPluginEditor")))
            ]

            // 目标类型（工程 / 工程插件）
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("归属位置（Target）")))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&TargetTypeOptions)
                .InitiallySelectedItem(SelectedTargetType)
                .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type)
                {
                    SelectedTargetType = NewItem;

                    // 选了工程插件时，确保插件列表有默认选中
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
                            : FText::FromString(TEXT("请选择"));
                    })
                ]
            ]

            // 工程插件选择（仅当 TargetType=ProjectPlugin 可见）
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0,0,0,6)
            
            [
                SNew(STextBlock)
                .Visibility_Lambda([this](){ return GetPluginPickerVisibility(); })
                .Text(FText::FromString(TEXT("选择工程插件（Project Plugin）")))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0,0,0,12)
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
                            : FText::FromString(TEXT("请选择插件"));
                    })
                ]
            ]

            // 模块类型
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("模块类型（Type）")))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&ModuleTypeOptions)
                .InitiallySelectedItem(SelectedModuleType)
                .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type){ SelectedModuleType = NewItem; })
                .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return MakeComboItemWidget(Item); })
                [
                    SNew(STextBlock).Text_Lambda([this]()
                    {
                        return SelectedModuleType.IsValid()
                            ? FText::FromString(*SelectedModuleType)
                            : FText::FromString(TEXT("请选择"));
                    })
                ]
            ]

            // LoadingPhase
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("加载阶段（LoadingPhase）")))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&LoadingPhaseOptions)
                .InitiallySelectedItem(SelectedLoadingPhase)
                .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewItem, ESelectInfo::Type){ SelectedLoadingPhase = NewItem; })
                .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return MakeComboItemWidget(Item); })
                [
                    SNew(STextBlock).Text_Lambda([this]()
                    {
                        return SelectedLoadingPhase.IsValid()
                            ? FText::FromString(*SelectedLoadingPhase)
                            : FText::FromString(TEXT("请选择"));
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
                    .Text(FText::FromString(TEXT("取消")))
                    .OnClicked(this, &SAddModuleWindow::HandleCancel)
                ]

                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("确定")))
                    .OnClicked(this, &SAddModuleWindow::HandleConfirm)
                ]
            ]
        ]
    ];
}

void SAddModuleWindow::InitOptions()
{
    // 模块类型（常用）
    ModuleTypeOptions = {
        MakeShared<FString>(TEXT("Runtime")),
        MakeShared<FString>(TEXT("Editor")),
        MakeShared<FString>(TEXT("Developer")),
        MakeShared<FString>(TEXT("RuntimeNoCommandlet")),
        MakeShared<FString>(TEXT("EditorNoCommandlet")),
    };
    SelectedModuleType = ModuleTypeOptions[0];

    // LoadingPhase（常用）
    LoadingPhaseOptions = {
        MakeShared<FString>(TEXT("Default")),
        MakeShared<FString>(TEXT("PostEngineInit")),
        MakeShared<FString>(TEXT("PreDefault")),
        MakeShared<FString>(TEXT("PostDefault")),
    };
    SelectedLoadingPhase = LoadingPhaseOptions[0];

    // ✅ 目标类型
    TargetTypeOptions = {
        MakeShared<FString>(TEXT("Project")),       // 当前工程
        MakeShared<FString>(TEXT("ProjectPlugin")), // 当前工程插件
    };
    SelectedTargetType = TargetTypeOptions[0];
}

void SAddModuleWindow::RefreshProjectPlugins()
{
    ProjectPluginOptions.Reset();
    SelectedProjectPlugin.Reset();

    // 只列出 Project 插件（不列 Engine 插件，1.0 更安全）
    for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
    {
        if (Plugin->GetType() == EPluginType::Project)
        {
            ProjectPluginOptions.Add(MakeShared<FString>(Plugin->GetName()));
        }
    }

    // 默认选第一个
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

    // 1) 收集参数
    Params.ModuleName = ModuleNameText.IsValid()
        ? ModuleNameText->GetText().ToString().TrimStartAndEnd()
        : TEXT("");

    Params.ModuleType = SelectedModuleType.IsValid() ? *SelectedModuleType : TEXT("Runtime");
    Params.LoadingPhase = SelectedLoadingPhase.IsValid() ? *SelectedLoadingPhase : TEXT("Default");

    if (SelectedTargetType.IsValid() && *SelectedTargetType == TEXT("ProjectPlugin"))
    {
        Params.TargetType = EModuleTargetType::ProjectPlugin;
        Params.TargetPluginName = SelectedProjectPlugin.IsValid() ? *SelectedProjectPlugin : TEXT("");
    }
    else
    {
        Params.TargetType = EModuleTargetType::Project;
        Params.TargetPluginName.Reset();
    }

    // 2) 基础校验（失败：不关闭）
    if (Params.ModuleName.IsEmpty())
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("模块名不能为空")));
        return FReply::Handled();
    }

    if (Params.TargetType == EModuleTargetType::ProjectPlugin && Params.TargetPluginName.IsEmpty())
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("请选择一个工程插件")));
        return FReply::Handled();
    }

    // 3) 必须绑定回调（失败：不关闭）
    if (!OnConfirm.IsBound())
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("OnConfirm 未绑定（内部错误）")));
        return FReply::Handled();
    }

    // 4) 执行业务（由外部决定成功与否）
    const bool bOk = OnConfirm.Execute(Params);

    // 失败：不关闭，让用户继续改
    if (!bOk)
    {
        return FReply::Handled();
    }

    // 5) 成功：关闭窗口
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
