#include "VMatPresetSettings.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "VMatPresetSettings"

namespace VMatPresetStorage
{
	static const TCHAR* UserSection = TEXT("VMat");
	static const TCHAR* LastPresetKey = TEXT("LastPresetName");

	static void NotifyFailure(const FText& Message)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 10.f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

UVMatPresetSettings& UVMatPresetSettings::Get()
{
	UVMatPresetSettings* Settings = GetMutableDefault<UVMatPresetSettings>();
	if (!Settings->bLoaded)
	{
		Settings->Load();
	}
	return *Settings;
}

FString UVMatPresetSettings::GetPresetFilePath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VMat"));
	check(Plugin.IsValid());
	return FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Config/VMatPresets.json"));
}

void UVMatPresetSettings::Load()
{
	bLoaded = true;
	const FString FilePath = GetPresetFilePath();

	FString Json;
	if (FFileHelper::LoadFileToString(Json, *FilePath))
	{
		FVMatPresetFile File;
		if (FJsonObjectConverter::JsonObjectStringToUStruct(Json, &File))
		{
			Presets = MoveTemp(File.Presets);
		}
		else
		{
			// 손으로 고치다 깨졌을수도 있어. 빈걸로 덮어쓰면 다 날아가니까 고칠때까지 저장 안해
			bPresetFileBroken = true;
			Presets.Reset();
			VMatPresetStorage::NotifyFailure(FText::Format(
				LOCTEXT("BrokenFile", "Couldn't read V-Mat presets from {0}.\nFix or delete the file. Preset changes won't be saved until then."),
				FText::FromString(FilePath)));
		}
	}
	else if (!Presets.IsEmpty())
	{
		// json은 없는데 예전 버전이 프로젝트 ini에 남긴 preset이 있으면 json으로 옮겨
		SavePresetFile();
	}

	// 개인 설정에 없으면 예전 ini에 있던 값 그대로 써
	FString UserLastPreset;
	if (GConfig->GetString(VMatPresetStorage::UserSection, VMatPresetStorage::LastPresetKey, UserLastPreset, GEditorPerProjectIni))
	{
		LastPresetName = UserLastPreset;
	}
}

FVMatPreset* UVMatPresetSettings::FindPreset(const FString& Name)
{
	return Presets.FindByPredicate([&Name](const FVMatPreset& Preset) { return Preset.Name == Name; });
}

FString UVMatPresetSettings::MakeUniqueName(const FString& BaseName) const
{
	auto IsTaken = [this](const FString& Name)
	{
		return Presets.ContainsByPredicate([&Name](const FVMatPreset& Preset) { return Preset.Name == Name; });
	};

	if (!IsTaken(BaseName))
	{
		return BaseName;
	}

	for (int32 Suffix = 2; ; ++Suffix)
	{
		const FString Candidate = FString::Printf(TEXT("%s %d"), *BaseName, Suffix);
		if (!IsTaken(Candidate))
		{
			return Candidate;
		}
	}
}

void UVMatPresetSettings::Save()
{
	if (!bPresetFileBroken)
	{
		SavePresetFile();
	}

	GConfig->SetString(VMatPresetStorage::UserSection, VMatPresetStorage::LastPresetKey, *LastPresetName, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

void UVMatPresetSettings::SavePresetFile() const
{
	FVMatPresetFile File;
	File.Presets = Presets;

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(File, Json))
	{
		return;
	}

	// 내용이 같으면 안 써. preset만 고르고 바꾼건 없는데 git에 바뀐걸로 뜨면 귀찮아
	const FString FilePath = GetPresetFilePath();
	FString Existing;
	if (FFileHelper::LoadFileToString(Existing, *FilePath) && Existing == Json)
	{
		return;
	}

	if (!FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		VMatPresetStorage::NotifyFailure(FText::Format(
			LOCTEXT("SaveFailed", "Couldn't save V-Mat presets to {0}.\nCheck whether the file is read-only."),
			FText::FromString(FilePath)));
	}
}

#undef LOCTEXT_NAMESPACE
