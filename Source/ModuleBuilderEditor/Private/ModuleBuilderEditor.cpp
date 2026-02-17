// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModuleBuilderEditor.h"
#include "SAddModuleWindow.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FModuleBuilderEditorModule"

struct FTargetResolveResult
{
	FString ContainerRoot;     // ProjectRoot 或 PluginRoot
	FString DescriptorPath;    // .uproject 或 .uplugin
	bool bIsProject = true;
};

static bool ResolveTargetFromParams(
	const FNewModuleParams& Params,
	FTargetResolveResult& Out,
	FString& OutError)
{
	if (Params.TargetType == EModuleTargetType::Project)
	{
		Out.bIsProject = true;
		Out.ContainerRoot = FPaths::ProjectDir();
		Out.DescriptorPath = FPaths::GetProjectFilePath(); // xxx.uproject
		return true;
	}

	if (Params.TargetType == EModuleTargetType::ProjectPlugin)
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Params.TargetPluginName);
		if (!Plugin.IsValid())
		{
			OutError = TEXT("找不到目标插件：") + Params.TargetPluginName;
			return false;
		}

		Out.bIsProject = false;
		Out.ContainerRoot = Plugin->GetBaseDir();              // 插件根
		Out.DescriptorPath = Plugin->GetDescriptorFileName();  // xxx.uplugin
		return true;
	}

	OutError = TEXT("未知 TargetType");
	return false;
}



static FString MakeBuildCsText(const FString& ModuleName, bool bIsEditorModule)
{
	// 最小 Build.cs（你后面再按需扩展依赖）
	FString Text;
	Text += TEXT("using UnrealBuildTool;\n\n");
	Text += FString::Printf(TEXT("public class %s : ModuleRules\n{\n"), *ModuleName);
	Text += FString::Printf(TEXT("\tpublic %s(ReadOnlyTargetRules Target) : base(Target)\n\t{\n"), *ModuleName);
	Text += TEXT("\t\tPCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;\n\n");

	Text += TEXT("\t\tPublicDependencyModuleNames.AddRange(new string[]\n\t\t{\n");
	Text += TEXT("\t\t\t\"Core\",\n");
	Text += TEXT("\t\t\t\"CoreUObject\",\n");
	Text += TEXT("\t\t\t\"Engine\"\n");
	Text += TEXT("\t\t});\n\n");

	if (bIsEditorModule)
	{
		Text += TEXT("\t\tPrivateDependencyModuleNames.AddRange(new string[]\n\t\t{\n");
		Text += TEXT("\t\t\t\"UnrealEd\",\n");
		Text += TEXT("\t\t\t\"Slate\",\n");
		Text += TEXT("\t\t\t\"SlateCore\",\n");
		Text += TEXT("\t\t\t\"ToolMenus\"\n");
		Text += TEXT("\t\t});\n\n");
	}

	Text += TEXT("\t}\n}\n");
	return Text;
}

static FString MakeModuleHeaderText(const FString& ModuleName)
{
	return FString::Printf(TEXT(
R"(#pragma once

#include "Modules/ModuleManager.h"

/**
 * %s 模块
 */
class F%sModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
)"), *ModuleName, *ModuleName);
}

static FString MakeModuleCppText(const FString& ModuleName)
{
	return FString::Printf(TEXT(
R"(#include "%s.h"
#include "Modules/ModuleManager.h"

void F%sModule::StartupModule()
{
    // TODO：模块启动逻辑
}

void F%sModule::ShutdownModule()
{
    // TODO：模块关闭逻辑
}

IMPLEMENT_MODULE(F%sModule, %s)
)"), *ModuleName, *ModuleName, *ModuleName, *ModuleName, *ModuleName);
}

static bool SaveTextChecked(const FString& Path, const FString& Text, FString& OutError)
{
	const bool bOk = FFileHelper::SaveStringToFile(Text, *Path);
	if (!bOk)
	{
		OutError = TEXT("写入失败：") + Path;
	}
	return bOk;
}

static bool GenerateModuleFilesToTarget(
	const FString& ContainerRoot,
	const FString& ModuleName,
	bool bIsEditorModule,
	FString& OutError)
{
	const FString SourceDir = FPaths::ConvertRelativePathToFull(ContainerRoot / TEXT("Source"));
	const FString ModuleDir = FPaths::ConvertRelativePathToFull(SourceDir / ModuleName);

	const FString PublicDir  = ModuleDir / TEXT("Public");
	const FString PrivateDir = ModuleDir / TEXT("Private");

	if (!IFileManager::Get().MakeDirectory(*PublicDir, true))
	{ OutError = TEXT("创建目录失败：") + PublicDir; return false; }

	if (!IFileManager::Get().MakeDirectory(*PrivateDir, true))
	{ OutError = TEXT("创建目录失败：") + PrivateDir; return false; }

	const FString BuildCsPath = ModuleDir / (ModuleName + TEXT(".Build.cs"));
	const FString HPath       = PublicDir / (ModuleName + TEXT(".h"));
	const FString CppPath     = PrivateDir / (ModuleName + TEXT(".cpp"));

	if (FPaths::FileExists(BuildCsPath) || FPaths::FileExists(HPath) || FPaths::FileExists(CppPath))
	{
		OutError = TEXT("目标文件已存在（避免覆盖）");
		return false;
	}

	if (!SaveTextChecked(BuildCsPath, MakeBuildCsText(ModuleName, bIsEditorModule), OutError)) return false;
	if (!SaveTextChecked(HPath,       MakeModuleHeaderText(ModuleName),              OutError)) return false;
	if (!SaveTextChecked(CppPath,     MakeModuleCppText(ModuleName),                 OutError)) return false;

	// ✅ 最后再确认文件确实落盘
	if (!FPaths::FileExists(BuildCsPath) || !FPaths::FileExists(HPath) || !FPaths::FileExists(CppPath))
	{
		OutError = TEXT("写入后文件不存在（可能权限/路径问题）");
		return false;
	}

	return true;
}

void FModuleBuilderEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FModuleBuilderEditorModule::RegisterMenus)
	);
	
	UE_LOG(LogTemp, Warning, TEXT("FModuleBuilderEditorModule::StartupModule"));
}

void FModuleBuilderEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	
}

void FModuleBuilderEditorModule::RegisterMenus()
{
	UToolMenus* Menus = UToolMenus::Get();
	if (!Menus) return;
	
	if (UToolMenu* Toolbar = Menus->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = Toolbar->FindOrAddSection("Yourplugin");

		Section.AddMenuEntry(
			"YourPlugin_AddModule_Toolbar",
			FText::FromString(TEXT("添加模块")),
			FText::FromString(TEXT("创建一个新模块")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(),"Icons.Plus"),
			FUIAction(FExecuteAction::CreateRaw(this, &FModuleBuilderEditorModule::OnClickAddModule))
		);

		UE_LOG(LogTemp, Warning, TEXT("[YourPlugin] Added toolbar button to LevelEditor.LevelEditorToolBar / Settings"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[YourPlugin] ExtendMenu(LevelEditor.LevelEditorToolBar) failed"));
	}

	// 强制刷新（有时 Live Coding/热重载不刷新 UI）
	Menus->RefreshAllWidgets();
}

void FModuleBuilderEditorModule::OnClickAddModule()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("添加模块")))
		.SizingRule(ESizingRule::Autosized) 
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(
	SNew(SBox)
	.MinDesiredWidth(520.f)
	[
	SNew(SAddModuleWindow)
		.ParentWindow(Window)
		.OnConfirm(FOnConfirmModule::CreateRaw(this,&FModuleBuilderEditorModule::HandleConfirm))
	]
);

	FSlateApplication::Get().AddWindow(Window);
}

static bool AddModuleToDescriptor(
    const FString& DescriptorPath,   // .uplugin or .uproject
    const FString& ModuleName,
    const FString& InModuleType,
    const FString& InLoadingPhase,
    FString& OutError)
{
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *DescriptorPath))
    { OutError = TEXT("读取描述文件失败：") + DescriptorPath; return false; }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    { OutError = TEXT("解析 JSON 失败"); return false; }

    // ✅ 默认值兜底（避免空字符串导致字段缺失/无意义）
    const FString ModuleType    = InModuleType.IsEmpty() ? TEXT("Runtime") : InModuleType;
    const FString LoadingPhase  = InLoadingPhase.IsEmpty() ? TEXT("Default") : InLoadingPhase;

    // 取 Modules（没有就创建）
    TArray<TSharedPtr<FJsonValue>> Modules;
    if (Root->HasTypedField<EJson::Array>(TEXT("Modules")))
    {
        Modules = Root->GetArrayField(TEXT("Modules"));
    }

    // 防重复
    for (const TSharedPtr<FJsonValue>& V : Modules)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (V.IsValid() && V->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
        {
            FString Name;
            if ((*ObjPtr)->TryGetStringField(TEXT("Name"), Name) && Name == ModuleName)
            {
                OutError = TEXT("描述文件已存在同名模块：") + ModuleName;
                return false;
            }
        }
    }

    // ✅ 新模块对象：强制写入 3 个字段
    TSharedPtr<FJsonObject> NewMod = MakeShared<FJsonObject>();
    NewMod->SetStringField(TEXT("Name"), ModuleName);
    NewMod->SetStringField(TEXT("Type"), ModuleType);
    NewMod->SetStringField(TEXT("LoadingPhase"), LoadingPhase);

    Modules.Add(MakeShared<FJsonValueObject>(NewMod));
    Root->SetArrayField(TEXT("Modules"), Modules);

    // 写回（格式化）
    FString OutJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    if (!FFileHelper::SaveStringToFile(OutJson, *DescriptorPath))
    { OutError = TEXT("写回描述文件失败：") + DescriptorPath; return false; }

    return true;
}

bool FModuleBuilderEditorModule::HandleConfirm(const FNewModuleParams& Params)
{
	FString Error;
	FTargetResolveResult Target;

	if (!ResolveTargetFromParams(Params, Target, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return false;
	}

	const bool bIsEditor = Params.ModuleType.Contains(TEXT("Editor"));

	if (!GenerateModuleFilesToTarget(Target.ContainerRoot, Params.ModuleName, bIsEditor, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("❌ 生成文件失败：") + Error));
		return false;
	}

	if (!AddModuleToDescriptor(Target.DescriptorPath, Params.ModuleName, Params.ModuleType, Params.LoadingPhase, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("❌ 写入描述文件失败：") + Error));
		return false;
	}

	const FString ModuleDir = FPaths::ConvertRelativePathToFull(Target.ContainerRoot / TEXT("Source") / Params.ModuleName);
	FPlatformProcess::ExploreFolder(*ModuleDir);

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
		TEXT("✅ 添加模块成功！\n\n")
		TEXT("已生成文件并更新 .uproject/.uplugin。\n\n")
		TEXT("下一步建议：\n")
		TEXT("1) 重新生成工程文件（Generate Project Files）\n")
		TEXT("2) 编译一次（Editor Compile / IDE Build）")
	));

	return true;
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModuleBuilderEditorModule, ModuleBuilderEditor)