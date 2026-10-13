#include "VMatPresetSettings.h"

UVMatPresetSettings& UVMatPresetSettings::Get()
{
	return *GetMutableDefault<UVMatPresetSettings>();
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
	TryUpdateDefaultConfigFile();
}
