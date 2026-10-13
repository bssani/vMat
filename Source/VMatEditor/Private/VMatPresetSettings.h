#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "VMatPresetSettings.generated.h"

// preset 하나 = 이름 + material 모음집
USTRUCT()
struct FVMatPreset
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
};

// json 파일 모양. 나중에 구조 바뀌면 Version 보고 옮기려고 넣어둬
USTRUCT()
struct FVMatPresetFile
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<FVMatPreset> Presets;
};

// preset은 플러그인 폴더 안 Config/VMatPresets.json에 저장해
// 플러그인 폴더만 복사하거나 git으로 받으면 preset도 같이 따라가. material 경로가 같은 프로젝트면 그대로 써
UCLASS(config = Editor)
class UVMatPresetSettings : public UObject
{
	GENERATED_BODY()

public:
	// config는 예전 버전 때문에 남겨둔거야. 예전엔 프로젝트 Config/DefaultEditor.ini에 저장했어
	// json이 없을때 여기 읽힌걸 json으로 한번 옮기고, 그 뒤로는 ini에 안 써
	UPROPERTY(config)
	TArray<FVMatPreset> Presets;

	// 마지막에 쓰던 preset. 사람마다 달라서 json 말고 Saved 폴더 개인 설정에 둬. git에 안 올라가
	UPROPERTY(config)
	FString LastPresetName;

	static UVMatPresetSettings& Get();
	static FString GetPresetFilePath();

	FVMatPreset* FindPreset(const FString& Name);

	// 이름 겹치면 뒤에 숫자 붙여서 돌려줘
	FString MakeUniqueName(const FString& BaseName) const;

	// 바꿀때마다 불러. preset은 json에, 마지막 preset은 개인 설정에 저장해
	void Save();

private:
	void Load();
	void SavePresetFile() const;

	bool bLoaded = false;
	// json이 깨져서 못 읽었으면 덮어쓰면 날아가서 저장 막아둬
	bool bPresetFileBroken = false;
};
