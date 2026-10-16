#include "VMatMaterialApplier.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopedSlowTask.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "VMatMaterialApplier"

namespace VMatApplier
{
	bool IsEditableMesh(const UStaticMesh* Mesh)
	{
		return Mesh->GetPackage()->GetName().StartsWith(TEXT("/Game/"));
	}

	bool IsMultiSlot(const UStaticMesh* Mesh)
	{
		return Mesh->GetStaticMaterials().Num() > 1;
	}

	static bool HasOverride(const UStaticMeshComponent* Component)
	{
		return Component->OverrideMaterials.ContainsByPredicate([](const UMaterialInterface* Material) { return Material != nullptr; });
	}

	// component에 override 남아있으면 원본 바꿔도 월드에 안 보여. 예전 툴이 여기에도 넣어놔서 지워줘야 돼
	static void ClearOverrides(UStaticMeshComponent* Component)
	{
		Component->Modify();
		for (int32 SlotIndex = 0; SlotIndex < Component->OverrideMaterials.Num(); ++SlotIndex)
		{
			if (Component->OverrideMaterials[SlotIndex] != nullptr)
			{
				Component->SetMaterial(SlotIndex, nullptr);
			}
		}
	}

	// SetMaterial은 부를때마다 mesh를 다시 build해서, slot 5개면 build도 5번이야
	// slot 여러개 바꿀땐 한번에 바꾸고 build는 한번만 해
	static void SetSlotMaterials(UStaticMesh* Mesh, TConstArrayView<int32> SlotIndices, UMaterialInterface* Material)
	{
		if (SlotIndices.Num() == 1)
		{
			Mesh->SetMaterial(SlotIndices[0], Material);
			return;
		}

		// SetMaterial처럼 mesh description 먼저 올려놔야 undo 기록이 제대로 남아
		for (int32 LODIndex = 0; LODIndex < Mesh->GetNumSourceModels(); ++LODIndex)
		{
			Mesh->GetMeshDescription(LODIndex);
		}

		FProperty* MaterialsProperty = FindFProperty<FProperty>(UStaticMesh::StaticClass(), UStaticMesh::GetStaticMaterialsName());
		Mesh->PreEditChange(MaterialsProperty);
		TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
		for (const int32 SlotIndex : SlotIndices)
		{
			Materials[SlotIndex].MaterialInterface = Material;
		}
		FPropertyChangedEvent ChangedEvent(MaterialsProperty);
		Mesh->PostEditChangeProperty(ChangedEvent);

		// physical material도 slot 따라가서 SetMaterial처럼 다시 만들어
		if (UBodySetup* BodySetup = Mesh->GetBodySetup())
		{
			BodySetup->CreatePhysicsMeshes();
		}
	}

	// 바꾼 mesh를 레벨에서 쓰고 있는 actor 수. 선택 안 한 actor도 같이 바뀌니까 알려주려고 세는거야
	static int32 CountActorsUsing(const TSet<UStaticMesh*>& Meshes)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		TSet<AActor*> Actors;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && Meshes.Contains(It->GetStaticMesh()))
			{
				if (AActor* Owner = It->GetOwner())
				{
					Actors.Add(Owner);
				}
			}
		}
		return Actors.Num();
	}

	// 바꾼 원본 mesh는 바로 저장해. 레벨은 평소처럼 Ctrl+S로 저장하면 돼
	static bool SaveMeshes(const TArray<UStaticMesh*>& Meshes)
	{
		if (Meshes.IsEmpty())
		{
			return true;
		}

		TArray<UPackage*> PackagesToSave;
		for (UStaticMesh* Mesh : Meshes)
		{
			PackagesToSave.AddUnique(Mesh->GetPackage());
		}
		return FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false) == FEditorFileUtils::PR_Success;
	}

	static void Notify(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	FVMatApplyTargets CollectFromSelection()
	{
		FVMatApplyTargets Targets;
		if (!GEditor)
		{
			return Targets;
		}

		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
		Targets.NumSelectedActors = SelectedActors.Num();

		TSet<UStaticMesh*> MeshSet;
		TSet<UStaticMesh*> LockedMeshes;

		for (AActor* Actor : SelectedActors)
		{
			Targets.ActorLabels.Add(Actor->GetActorLabel());

			// 첫번째 component만 보면 복합 actor에서 빠지는게 생겨서 다 모아
			TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
			bool bHasMesh = false;
			for (UStaticMeshComponent* Component : Components)
			{
				UStaticMesh* Mesh = Component->GetStaticMesh();
				if (!Mesh)
				{
					continue;
				}
				bHasMesh = true;

				if (!IsEditableMesh(Mesh))
				{
					LockedMeshes.Add(Mesh);
					continue;
				}

				if (!MeshSet.Contains(Mesh))
				{
					MeshSet.Add(Mesh);
					Targets.Meshes.Add(Mesh);
					if (IsMultiSlot(Mesh))
					{
						Targets.MultiSlotMeshes.Add(Mesh);
					}
				}
				Targets.Components.Add(Component);
			}

			if (!bHasMesh)
			{
				++Targets.NumActorsWithoutMesh;
			}
		}

		Targets.NumLockedMeshes = LockedMeshes.Num();
		return Targets;
	}

	FText DescribeSkipped(const FVMatApplyTargets& Targets)
	{
		TArray<FText> Parts;
		if (Targets.NumLockedMeshes > 0)
		{
			Parts.Add(FText::Format(LOCTEXT("SkippedLocked", "{0} engine/plugin mesh(es)"), Targets.NumLockedMeshes));
		}
		if (Targets.NumActorsWithoutMesh > 0)
		{
			Parts.Add(FText::Format(LOCTEXT("SkippedNoMesh", "{0} actor(s) without mesh"), Targets.NumActorsWithoutMesh));
		}

		if (Parts.IsEmpty())
		{
			return FText::GetEmpty();
		}
		return FText::Format(LOCTEXT("SkippedFormat", "Skipped: {0}"), FText::Join(FText::FromString(TEXT(", ")), Parts));
	}

	void ApplyMaterial(UMaterialInterface* Material)
	{
		if (!Material || !GEditor)
		{
			return;
		}

		const FVMatApplyTargets Targets = CollectFromSelection();
		const FText Skipped = DescribeSkipped(Targets);

		if (Targets.NumSelectedActors == 0)
		{
			Notify(LOCTEXT("NoSelection", "Select mesh actors in the level first."), false);
			return;
		}

		// 바꿀게 있는지 먼저 보고 transaction 열어. 이미 같은 material인 slot은 안 건드려
		// mesh를 다시 build하는게 느려서 이미 전부 같은 material인 mesh는 아예 build 안 하게
		TArray<TPair<UStaticMesh*, TArray<int32>>> MeshesToChange;
		for (UStaticMesh* Mesh : Targets.Meshes)
		{
			TArray<int32> SlotIndices;
			for (int32 SlotIndex = 0; SlotIndex < Mesh->GetStaticMaterials().Num(); ++SlotIndex)
			{
				if (Mesh->GetMaterial(SlotIndex) != Material)
				{
					SlotIndices.Add(SlotIndex);
				}
			}
			if (!SlotIndices.IsEmpty())
			{
				MeshesToChange.Emplace(Mesh, MoveTemp(SlotIndices));
			}
		}

		TArray<UStaticMeshComponent*> ComponentsToClear;
		for (UStaticMeshComponent* Component : Targets.Components)
		{
			if (HasOverride(Component))
			{
				ComponentsToClear.Add(Component);
			}
		}

		if (MeshesToChange.IsEmpty() && ComponentsToClear.IsEmpty())
		{
			const FText Message = Targets.Meshes.IsEmpty()
				? FText::Format(LOCTEXT("NothingApplicable", "Nothing to change. {0}"), Skipped)
				: FText::Format(LOCTEXT("AlreadyApplied", "{0} is already applied."), FText::FromString(Material->GetName()));
			Notify(Message, Targets.Meshes.Num() > 0);
			return;
		}

		const FText MaterialName = FText::FromString(Material->GetName());
		TArray<UStaticMesh*> ChangedMeshes;
		{
			// 버튼 한번 누른건 Ctrl+Z 한번으로 되돌아가게 하나로 묶어
			FScopedTransaction Transaction(FText::Format(LOCTEXT("ApplyTransaction", "V-Mat: Apply {0}"), MaterialName));

			// mesh 수백개를 한번에 바꾸면 몇초 걸려서, 오래 걸릴때만 진행창 띄워
			FScopedSlowTask SlowTask(MeshesToChange.Num(), FText::Format(LOCTEXT("Applying", "Applying {0}..."), MaterialName));
			SlowTask.MakeDialogDelayed(0.5f);

			for (const TPair<UStaticMesh*, TArray<int32>>& Change : MeshesToChange)
			{
				SlowTask.EnterProgressFrame();
				SetSlotMaterials(Change.Key, Change.Value, Material);
				ChangedMeshes.Add(Change.Key);
			}

			for (UStaticMeshComponent* Component : ComponentsToClear)
			{
				ClearOverrides(Component);
			}
		}

		const bool bSaved = SaveMeshes(ChangedMeshes);

		FText Message = FText::Format(
			LOCTEXT("Applied", "Applied {0} to {1} mesh(es). {2} actor(s) in the level use them."),
			MaterialName, Targets.Meshes.Num(), CountActorsUsing(TSet<UStaticMesh*>(Targets.Meshes)));
		if (Targets.MultiSlotMeshes.Num() > 0)
		{
			Message = FText::Format(LOCTEXT("AppliedMultiSlot", "{0}\n{1} multi-slot mesh(es): every slot got {2}."), Message, Targets.MultiSlotMeshes.Num(), MaterialName);
		}
		if (!Skipped.IsEmpty())
		{
			Message = FText::Format(LOCTEXT("AppliedWithSkipped", "{0}\n{1}"), Message, Skipped);
		}
		if (!bSaved)
		{
			Message = FText::Format(LOCTEXT("SaveFailed", "{0}\nSaving the meshes failed. Check whether the files are read-only."), Message);
		}
		Notify(Message, bSaved);
	}

	void ApplyMaterialToSlots(UMaterialInterface* Material, TConstArrayView<FVMatMeshSlot> Slots)
	{
		if (!Material || !GEditor)
		{
			return;
		}

		// mesh별로 바꿀 slot 모아. /Game 밖에 있거나 없는 slot 번호는 빼
		TMap<UStaticMesh*, TArray<int32>> SlotsByMesh;
		int32 NumSlots = 0;
		for (const FVMatMeshSlot& Slot : Slots)
		{
			if (Slot.Mesh && IsEditableMesh(Slot.Mesh) && Slot.Mesh->GetStaticMaterials().IsValidIndex(Slot.SlotIndex))
			{
				TArray<int32>& MeshSlots = SlotsByMesh.FindOrAdd(Slot.Mesh);
				if (!MeshSlots.Contains(Slot.SlotIndex))
				{
					MeshSlots.Add(Slot.SlotIndex);
					++NumSlots;
				}
			}
		}

		if (SlotsByMesh.IsEmpty())
		{
			Notify(LOCTEXT("NoSlotsSelected", "Select the slots to change first."), false);
			return;
		}

		// 이미 같은 material인 slot은 안 건드려. mesh 다시 build하는게 느리거든
		TArray<TPair<UStaticMesh*, TArray<int32>>> SlotsToChange;
		for (const TPair<UStaticMesh*, TArray<int32>>& Pair : SlotsByMesh)
		{
			TArray<int32> SlotIndices = Pair.Value.FilterByPredicate([&Pair, Material](const int32 SlotIndex)
			{
				return Pair.Key->GetMaterial(SlotIndex) != Material;
			});
			if (!SlotIndices.IsEmpty())
			{
				SlotsToChange.Emplace(Pair.Key, MoveTemp(SlotIndices));
			}
		}

		// 레벨에서 이 mesh 쓰는 component에 그 slot override가 남아있으면 원본 바꿔도 안 보여서 지워
		TArray<TPair<UStaticMeshComponent*, int32>> OverridesToClear;
		UWorld* World = GEditor->GetEditorWorldContext().World();
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			const TArray<int32>* MeshSlots = It->GetWorld() == World ? SlotsByMesh.Find(It->GetStaticMesh()) : nullptr;
			if (!MeshSlots)
			{
				continue;
			}
			for (const int32 SlotIndex : *MeshSlots)
			{
				if (It->OverrideMaterials.IsValidIndex(SlotIndex) && It->OverrideMaterials[SlotIndex] != nullptr)
				{
					OverridesToClear.Emplace(*It, SlotIndex);
				}
			}
		}

		const FText MaterialName = FText::FromString(Material->GetName());
		if (SlotsToChange.IsEmpty() && OverridesToClear.IsEmpty())
		{
			Notify(FText::Format(LOCTEXT("SlotsAlreadyApplied", "{0} is already in those slots."), MaterialName), true);
			return;
		}

		TArray<UStaticMesh*> ChangedMeshes;
		{
			// 버튼 한번 누른건 Ctrl+Z 한번으로 되돌아가게 하나로 묶어
			FScopedTransaction Transaction(FText::Format(LOCTEXT("ApplySlotsTransaction", "V-Mat: Apply {0} to slots"), MaterialName));

			// mesh 수백개를 한번에 바꾸면 몇초 걸려서, 오래 걸릴때만 진행창 띄워
			FScopedSlowTask SlowTask(SlotsToChange.Num(), FText::Format(LOCTEXT("ApplyingSlots", "Applying {0}..."), MaterialName));
			SlowTask.MakeDialogDelayed(0.5f);

			for (const TPair<UStaticMesh*, TArray<int32>>& Change : SlotsToChange)
			{
				SlowTask.EnterProgressFrame();
				SetSlotMaterials(Change.Key, Change.Value, Material);
				ChangedMeshes.Add(Change.Key);
			}

			for (const TPair<UStaticMeshComponent*, int32>& Override : OverridesToClear)
			{
				Override.Key->Modify();
				Override.Key->SetMaterial(Override.Value, nullptr);
			}
		}

		const bool bSaved = SaveMeshes(ChangedMeshes);

		TSet<UStaticMesh*> Meshes;
		SlotsByMesh.GetKeys(Meshes);
		FText Message = FText::Format(
			LOCTEXT("AppliedSlots", "Applied {0} to {1} slot(s) on {2} mesh(es). {3} actor(s) in the level use them."),
			MaterialName, NumSlots, SlotsByMesh.Num(), CountActorsUsing(Meshes));
		if (!bSaved)
		{
			Message = FText::Format(LOCTEXT("SlotsSaveFailed", "{0}\nSaving the meshes failed. Check whether the files are read-only."), Message);
		}
		Notify(Message, bSaved);
	}
}

#undef LOCTEXT_NAMESPACE
