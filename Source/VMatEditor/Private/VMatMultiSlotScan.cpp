#include "VMatMultiSlotScan.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "VMatMaterialApplier.h"

namespace VMatMultiSlot
{
	FVMatMultiSlotScanResult ScanLevel()
	{
		FVMatMultiSlotScanResult Result;
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return Result;
		}

		const double StartTime = FPlatformTime::Seconds();

		// mesh마다 slot 수는 한번만 확인해. 같은 볼트 mesh가 천개 있어도 판정은 한번
		TSet<UStaticMesh*> SeenMeshes;
		TMap<UStaticMesh*, TSharedPtr<FVMatMultiSlotMesh>> Found;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			++Result.NumActorsScanned;

			TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
			for (UStaticMeshComponent* Component : Components)
			{
				UStaticMesh* Mesh = Component->GetStaticMesh();
				if (!Mesh)
				{
					continue;
				}

				bool bSeen = false;
				SeenMeshes.Add(Mesh, &bSeen);

				TSharedPtr<FVMatMultiSlotMesh>* Entry = Found.Find(Mesh);
				if (!Entry)
				{
					if (bSeen || !VMatApplier::IsEditableMesh(Mesh) || !VMatApplier::IsMultiSlot(Mesh))
					{
						continue;
					}

					TSharedPtr<FVMatMultiSlotMesh> NewEntry = MakeShared<FVMatMultiSlotMesh>();
					NewEntry->Mesh = Mesh;
					NewEntry->Name = Mesh->GetName();
					NewEntry->Folder = FPackageName::GetLongPackagePath(Mesh->GetPackage()->GetName());
					NewEntry->NumSlots = Mesh->GetStaticMaterials().Num();
					Entry = &Found.Add(Mesh, NewEntry);
				}

				// 한 actor에 같은 mesh component가 여러개 있으면 actor는 한번만 넣어
				TArray<TWeakObjectPtr<AActor>>& Actors = (*Entry)->Actors;
				if (Actors.IsEmpty() || Actors.Last().Get() != Actor)
				{
					Actors.Add(Actor);
				}
			}
		}

		Found.GenerateValueArray(Result.Meshes);
		Result.NumMeshesScanned = SeenMeshes.Num();
		Result.Seconds = FPlatformTime::Seconds() - StartTime;
		return Result;
	}

	TArray<TSharedPtr<FVMatSlotGroup>> GroupSlots(const TArray<UStaticMesh*>& Meshes, EVMatSlotGrouping Grouping)
	{
		TArray<TSharedPtr<FVMatSlotGroup>> Groups;

		// mesh 하나면 묶을 필요 없이 slot 번호대로 한 줄씩
		if (Meshes.Num() == 1)
		{
			UStaticMesh* Mesh = Meshes[0];
			const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
			for (int32 SlotIndex = 0; SlotIndex < Materials.Num(); ++SlotIndex)
			{
				TSharedPtr<FVMatSlotGroup> Group = MakeShared<FVMatSlotGroup>();
				Group->Label = Materials[SlotIndex].MaterialSlotName.ToString();
				Group->Targets.Add({ Mesh, SlotIndex });
				Group->NumMeshes = 1;
				Group->SlotIndex = SlotIndex;
				Group->Material = Materials[SlotIndex].MaterialInterface;
				Groups.Add(Group);
			}
			return Groups;
		}

		TMap<FString, TSharedPtr<FVMatSlotGroup>> GroupsByKey;
		TMap<FString, TSet<UStaticMesh*>> MeshesByKey;
		for (UStaticMesh* Mesh : Meshes)
		{
			const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
			for (int32 SlotIndex = 0; SlotIndex < Materials.Num(); ++SlotIndex)
			{
				UMaterialInterface* Material = Materials[SlotIndex].MaterialInterface;
				FString Key;
				FString Label;
				if (Grouping == EVMatSlotGrouping::Material)
				{
					// 폴더만 다르고 이름 같은 material이 섞이면 안돼서 경로로 묶어
					Key = Material ? Material->GetPathName() : FString();
					Label = Material ? Material->GetName() : TEXT("None");
				}
				else
				{
					// 이름 없는 slot은 번호로 묶어
					const FName SlotName = Materials[SlotIndex].MaterialSlotName;
					Key = SlotName.IsNone() ? FString::Printf(TEXT("Element %d"), SlotIndex) : SlotName.ToString();
					Label = Key;
				}

				TSharedPtr<FVMatSlotGroup>& Group = GroupsByKey.FindOrAdd(Key);
				if (!Group)
				{
					Group = MakeShared<FVMatSlotGroup>();
					Group->Label = Label;
					Group->Material = Material;
				}
				else if (!Group->bMixedMaterials && Group->Material.Get() != Material)
				{
					Group->bMixedMaterials = true;
					Group->Material = nullptr;
				}

				Group->Targets.Add({ Mesh, SlotIndex });
				MeshesByKey.FindOrAdd(Key).Add(Mesh);
			}
		}

		for (TPair<FString, TSharedPtr<FVMatSlotGroup>>& Pair : GroupsByKey)
		{
			Pair.Value->NumMeshes = MeshesByKey[Pair.Key].Num();
			Groups.Add(Pair.Value);
		}

		// 많이 쓰이는 slot이 위로 오게
		Groups.Sort([](const TSharedPtr<FVMatSlotGroup>& A, const TSharedPtr<FVMatSlotGroup>& B)
		{
			if (A->NumMeshes != B->NumMeshes)
			{
				return A->NumMeshes > B->NumMeshes;
			}
			return A->Label < B->Label;
		});
		return Groups;
	}
}
