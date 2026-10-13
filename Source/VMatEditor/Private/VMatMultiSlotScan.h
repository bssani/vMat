#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;

// 레벨에 있는 multi-slot mesh 하나. 이걸 쓰는 actor도 같이 들고 있어
struct FVMatMultiSlotMesh
{
	// 맵 바뀌거나 지워져도 안 터지게 weak로 들고 있어
	TWeakObjectPtr<UStaticMesh> Mesh;
	TArray<TWeakObjectPtr<AActor>> Actors;

	FString Name;
	FString Folder;
	int32 NumSlots = 0;
};

struct FVMatMultiSlotScanResult
{
	TArray<TSharedPtr<FVMatMultiSlotMesh>> Meshes;

	int32 NumActorsScanned = 0;
	int32 NumMeshesScanned = 0;
	double Seconds = 0.0;
};

struct FVMatSlotTarget
{
	TWeakObjectPtr<UStaticMesh> Mesh;
	int32 SlotIndex = INDEX_NONE;
};

// mesh 여러개 골랐을때 slot 묶는 기준
enum class EVMatSlotGrouping : uint8
{
	// 지금 들어있는 material이 같은 slot끼리. CAD에서 같은 색이던 slot이 한 줄로 모여
	Material,
	// slot 이름이 같은 slot끼리. Datasmith는 CAD material ID를 slot 이름으로 써서 mesh끼리 이름이 겹쳐
	SlotName
};

// slot 목록 한 줄. 이 줄 골라서 material 누르면 Targets에 다 들어가
struct FVMatSlotGroup
{
	FString Label;
	TArray<FVMatSlotTarget> Targets;
	int32 NumMeshes = 0;

	// mesh 하나만 골랐을때만 slot 번호가 들어가
	int32 SlotIndex = INDEX_NONE;

	// 다 같은 material이면 그거. 섞여 있으면 비어있고 bMixedMaterials
	TWeakObjectPtr<UMaterialInterface> Material;
	bool bMixedMaterials = false;
};

namespace VMatMultiSlot
{
	// 지금 열린 레벨을 훑어. 레벨에 올라온 mesh는 이미 메모리에 있어서 에셋을 새로 로드하지 않아
	FVMatMultiSlotScanResult ScanLevel();

	// mesh 하나면 slot 번호대로, 여러개면 Grouping 기준으로 묶어
	TArray<TSharedPtr<FVMatSlotGroup>> GroupSlots(const TArray<UStaticMesh*>& Meshes, EVMatSlotGrouping Grouping);
}
