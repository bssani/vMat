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

// preset은 프로젝트마다 따로 둬. 차량마다 쓰는 material이 다르니까
// Config/DefaultEditor.ini에 저장돼서 Saved 폴더 지워도 안 날아가
UCLASS(config = Editor, defaultconfig)
class UVMatPresetSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(config)
	TArray<FVMatPreset> Presets;

	// 마지막에 쓰던 preset. 에디터 다시 켜면 이걸로 열어
	UPROPERTY(config)
	FString LastPresetName;

	static UVMatPresetSettings& Get();

	FVMatPreset* FindPreset(const FString& Name);

	// 이름 겹치면 뒤에 숫자 붙여서 돌려줘
	FString MakeUniqueName(const FString& BaseName) const;

	void Save();
};
