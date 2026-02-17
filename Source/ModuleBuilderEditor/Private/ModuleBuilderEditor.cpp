// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModuleBuilderEditor.h"
#include "SAddModuleWindow.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolMenus.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "ModuleBuilder"

struct FTargetResolveResult
{
	FString ContainerRoot;   // ProjectRoot 或 PluginRoot
	FString DescriptorPath;  // .uproject 或 .uplugin
	bool bIsProject = true;
};

static bool ResolveTargetFromParams(const FNewModuleParams& Params, FTargetResolveResult& Out, FString& OutError)
{
	if (Params.TargetType == EModuleTargetType::Project)
	{
		Out.bIsProject = true;
		Out.ContainerRoot = FPaths::ProjectDir();
		Out.DescriptorPath = FPaths::GetProjectFilePath();
		return true;
	}

	if (Params.TargetType == EModuleTargetType::ProjectPlugin)
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Params.TargetPluginName);
		if (!Plugin.IsValid())
		{
			OutError = TEXT("未找到目标插件：") + Params.TargetPluginName;
			return false;
		}

		Out.bIsProject = false;
		Out.ContainerRoot = Plugin->GetBaseDir();
		Out.DescriptorPath = Plugin->GetDescriptorFileName();
		return true;
	}

	OutError = TEXT("未知的目标类型。");
	return false;
}

static FString MakeBuildCsText(const FString& ModuleName, bool bIsEditorModule)
{
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
    // 模块启动时调用
}

void F%sModule::ShutdownModule()
{
    // 模块关闭时调用
}

IMPLEMENT_MODULE(F%sModule, %s)
)"), *ModuleName, *ModuleName, *ModuleName, *ModuleName, *ModuleName);
}

static bool SaveTextChecked(const FString& Path, const FString& Text, FString& OutError)
{
	const bool bOk = FFileHelper::SaveStringToFile(Text, *Path);
	if (!bOk)
	{
		OutError = TEXT("写入文件失败：") + Path;
	}
	return bOk;
}

static bool GenerateModuleFilesToTarget(const FString& ContainerRoot, const FString& ModuleName, bool bIsEditorModule, FString& OutError)
{
	const FString SourceDir = FPaths::ConvertRelativePathToFull(ContainerRoot / TEXT("Source"));
	const FString ModuleDir = FPaths::ConvertRelativePathToFull(SourceDir / ModuleName);

	const FString PublicDir  = ModuleDir / TEXT("Public");
	const FString PrivateDir = ModuleDir / TEXT("Private");

	if (!IFileManager::Get().MakeDirectory(*PublicDir, true))
	{
		OutError = TEXT("创建目录失败：") + PublicDir;
		return false;
	}

	if (!IFileManager::Get().MakeDirectory(*PrivateDir, true))
	{
		OutError = TEXT("创建目录失败：") + PrivateDir;
		return false;
	}

	const FString BuildCsPath = ModuleDir / (ModuleName + TEXT(".Build.cs"));
	const FString HPath       = PublicDir / (ModuleName + TEXT(".h"));
	const FString CppPath     = PrivateDir / (ModuleName + TEXT(".cpp"));

	if (FPaths::FileExists(BuildCsPath) || FPaths::FileExists(HPath) || FPaths::FileExists(CppPath))
	{
		OutError = TEXT("目标文件已存在，未进行覆盖。");
		return false;
	}

	if (!SaveTextChecked(BuildCsPath, MakeBuildCsText(ModuleName, bIsEditorModule), OutError)) return false;
	if (!SaveTextChecked(HPath,       MakeModuleHeaderText(ModuleName),            OutError)) return false;
	if (!SaveTextChecked(CppPath,     MakeModuleCppText(ModuleName),               OutError)) return false;

	return true;
}

static bool AddModuleToDescriptor(
	const FString& DescriptorPath,
	const FString& ModuleName,
	const FString& InModuleType,
	const FString& InLoadingPhase,
	FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *DescriptorPath))
	{
		OutError = TEXT("读取描述文件失败：") + DescriptorPath;
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON 解析失败。");
		return false;
	}

	const FString ModuleType   = InModuleType.IsEmpty()   ? TEXT("Runtime") : InModuleType;
	const FString LoadingPhase = InLoadingPhase.IsEmpty() ? TEXT("Default") : InLoadingPhase;

	TArray<TSharedPtr<FJsonValue>> Modules;
	if (Root->HasTypedField<EJson::Array>(TEXT("Modules")))
	{
		Modules = Root->GetArrayField(TEXT("Modules"));
	}

	for (const TSharedPtr<FJsonValue>& V : Modules)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (V.IsValid() && V->TryGetObject(ObjPtr) && ObjPtr && ObjPtr->IsValid())
		{
			FString Name;
			if ((*ObjPtr)->TryGetStringField(TEXT("Name"), Name) && Name == ModuleName)
			{
				OutError = TEXT("描述文件中已存在同名模块：") + ModuleName;
				return false;
			}
		}
	}

	TSharedPtr<FJsonObject> NewMod = MakeShared<FJsonObject>();
	NewMod->SetStringField(TEXT("Name"), ModuleName);
	NewMod->SetStringField(TEXT("Type"), ModuleType);
	NewMod->SetStringField(TEXT("LoadingPhase"), LoadingPhase);

	Modules.Add(MakeShared<FJsonValueObject>(NewMod));
	Root->SetArrayField(TEXT("Modules"), Modules);

	FString OutJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(OutJson, *DescriptorPath))
	{
		OutError = TEXT("写入描述文件失败：") + DescriptorPath;
		return false;
	}

	return true;
}

// ===== 模块实现 =====

void FModuleBuilderEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FModuleBuilderEditorModule::RegisterMenus)
	);
}

void FModuleBuilderEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FModuleBuilderEditorModule::RegisterMenus()
{
	UToolMenus* Menus = UToolMenus::Get();
	if (!Menus) return;

	if (UToolMenu* ToolsMenu = Menus->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("ModuleBuilder");

		Section.AddMenuEntry(
			"ModuleBuilder.AddModule",
			LOCTEXT("AddModuleMenu", "添加 C++ 模块"),
			LOCTEXT("AddModuleTooltip", "创建并注册一个新的 C++ 模块"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(FExecuteAction::CreateRaw(this, &FModuleBuilderEditorModule::OnClickAddModule))
		);
	}

	Menus->RefreshAllWidgets();
}

void FModuleBuilderEditorModule::OnClickAddModule()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("AddModuleWindowTitle", "添加 C++ 模块"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(
		SNew(SBox)
		.MinDesiredWidth(520.f)
		[
			SNew(SAddModuleWindow)
			.ParentWindow(Window)
			.OnConfirm(FOnConfirmModule::CreateRaw(this, &FModuleBuilderEditorModule::HandleConfirm))
		]
	);

	FSlateApplication::Get().AddWindow(Window);
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

	const bool bIsEditor = Params.ModuleType.Equals(TEXT("Editor"), ESearchCase::IgnoreCase);

	if (!GenerateModuleFilesToTarget(Target.ContainerRoot, Params.ModuleName, bIsEditor, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("GenerateFailed", "生成模块文件失败：\n{0}"),
				FText::FromString(Error)
			)
		);
		return false;
	}

	if (!AddModuleToDescriptor(Target.DescriptorPath, Params.ModuleName, Params.ModuleType, Params.LoadingPhase, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("DescriptorFailed", "更新描述文件失败：\n{0}"),
				FText::FromString(Error)
			)
		);
		return false;
	}

	const FString ModuleDir = FPaths::ConvertRelativePathToFull(Target.ContainerRoot / TEXT("Source") / Params.ModuleName);
	FPlatformProcess::ExploreFolder(*ModuleDir);

	FMessageDialog::Open(EAppMsgType::Ok,
		LOCTEXT("SuccessMessage",
			"模块添加成功！\n\n"
			"已生成模块文件，并更新描述文件（.uproject/.uplugin）。\n\n"
			"建议下一步：\n"
			"1）重新生成项目文件\n"
			"2）编译一次（编辑器编译 / IDE Build）"
		)
	);

	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FModuleBuilderEditorModule, ModuleBuilderEditor)
