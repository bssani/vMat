#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

// 지금 선택한 actor들 중에 material 입힐 수 있는거랑 못 입히는거 정리한거
struct FVMatApplyTargets
{
	// 같은 mesh 쓰는 actor가 여러개여도 mesh는 한번만 들어가. multi-slot mesh도 같이 들어가
	TArray<UStaticMesh*> Meshes;
	TArray<UStaticMeshComponent*> Components;
	TArray<FString> ActorLabels;

	// Meshes 중에 slot 여러개인거. slot 전부 같은 material로 덮이니까 요약에서 따로 알려줘
	TArray<UStaticMesh*> MultiSlotMeshes;

	int32 NumSelectedActors = 0;
	int32 NumActorsWithoutMesh = 0;
	// /Game 밖에 있는 mesh (엔진 기본 도형 같은거)
	int32 NumLockedMeshes = 0;
};

// mesh 하나의 slot 하나
struct FVMatMeshSlot
{
	UStaticMesh* Mesh = nullptr;
	int32 SlotIndex = INDEX_NONE;
};

namespace VMatApplier
{
	// 엔진이나 다른 플러그인 에셋은 건드리면 안돼서 /Game 안에 있는 mesh만 바꿔
	bool IsEditableMesh(const UStaticMesh* Mesh);

	// slot 여러개인 mesh. Palette에선 slot 전부 덮어쓰고, slot별로 넣는건 Multi-slot 탭에서 해
	bool IsMultiSlot(const UStaticMesh* Mesh);

	FVMatApplyTargets CollectFromSelection();

	// 건너뛴거 설명하는 문장. 아무것도 안 건너뛰었으면 빈 text
	FText DescribeSkipped(const FVMatApplyTargets& Targets);

	// 선택한 mesh의 원본에 material 입히고 저장까지 해. slot 여러개면 slot 전부 같은걸로
	void ApplyMaterial(UMaterialInterface* Material);

	// multi-slot mesh용. 고른 slot에만 material 넣고 저장까지 해
	void ApplyMaterialToSlots(UMaterialInterface* Material, TConstArrayView<FVMatMeshSlot> Slots);
}
