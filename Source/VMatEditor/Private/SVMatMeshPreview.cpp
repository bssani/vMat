#include "SVMatMeshPreview.h"

#include "AdvancedPreviewScene.h"
#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace VMatMeshPreview
{
	// 처음 보는 각도. 앞쪽 오른편 위에서 내려다봐
	const FRotator DefaultViewRotation(-20.f, -135.f, 0.f);

	// 안 고른 slot 색. 원래 material이 밝든 어둡든 구분되게 어두운 회색
	const FLinearColor DimColor(0.04f, 0.04f, 0.045f);
}

class FVMatMeshPreviewClient : public FEditorViewportClient
{
public:
	FVMatMeshPreviewClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SEditorViewport>& InViewport)
		: FEditorViewportClient(nullptr, &InPreviewScene, InViewport)
	{
		DrawHelper.bDrawGrid = false;
		DrawHelper.bDrawPivot = false;
		DrawHelper.bDrawWorldBox = false;
		DrawHelper.bDrawKillZ = false;

		SetViewMode(VMI_Lit);
		EngineShowFlags.SetSnap(0);
		EngineShowFlags.SetCompositeEditorPrimitives(true);
		OverrideNearClipPlane(1.f);
		SetViewRotation(VMatMeshPreview::DefaultViewRotation);
		bSetListenerPosition = false;
	}

	// 왼쪽 버튼으로 끌면 mesh 주위를 돌아. 작은 창이라 날아다니는 카메라보다 편해
	virtual bool ShouldOrbitCamera() const override
	{
		return true;
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);

		// 창 크기를 알아야 딱 맞게 잡혀서, 한번 그려져서 크기 생긴 다음에 맞춰
		if (PendingFocus.IsSet() && Viewport && Viewport->GetSizeXY().GetMin() > 0)
		{
			FocusViewportOnBox(PendingFocus.GetValue(), true);
			PendingFocus.Reset();
		}
	}

	void RequestFocus(const FBox& Box)
	{
		PendingFocus = Box;
	}

private:
	TOptional<FBox> PendingFocus;
};

SVMatMeshPreview::SVMatMeshPreview() = default;

SVMatMeshPreview::~SVMatMeshPreview()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}
}

void SVMatMeshPreview::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
	// 바닥은 원점에 깔려. mesh를 원점으로 옮겨서 보니까 바닥이 mesh를 뚫고 지나가서 숨겨
	PreviewScene->SetFloorVisibility(false, true);

	MeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	PreviewScene->AddComponent(MeshComponent, FTransform::Identity);

	// 엔진이 collision 보여줄때 쓰는 단색 material. "Color"로 색만 바꿔 써
	if (UMaterial* BaseMaterial = GEngine->ShadedLevelColorationUnlitMaterial)
	{
		DimMaterial.Reset(UMaterialInstanceDynamic::Create(BaseMaterial, GetTransientPackage()));
		DimMaterial->SetVectorParameterValue(TEXT("Color"), VMatMeshPreview::DimColor);
	}

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void SVMatMeshPreview::SetMesh(UStaticMesh* Mesh)
{
	if (MeshComponent->GetStaticMesh() == Mesh)
	{
		return;
	}

	// 전 mesh에 칠해둔거 남으면 slot 번호가 엉켜서 비워
	MeshComponent->EmptyOverrideMaterials();
	DimmedSlots.Reset();
	MeshComponent->SetStaticMesh(Mesh);

	// CAD mesh는 원점에서 멀리 있을때가 많아. 원점으로 당겨와서 조명이랑 하늘 한가운데 놓고 봐
	if (Mesh)
	{
		MeshComponent->SetRelativeLocation(-Mesh->GetBounds().Origin);
		FocusOnMesh();
	}
}

UStaticMesh* SVMatMeshPreview::GetMesh() const
{
	return MeshComponent->GetStaticMesh();
}

void SVMatMeshPreview::SetDimmedSlots(const TSet<int32>& SlotIndices)
{
	if (!DimMaterial.IsValid() || (SlotIndices.Num() == DimmedSlots.Num() && SlotIndices.Includes(DimmedSlots)))
	{
		return;
	}

	DimmedSlots = SlotIndices;
	MeshComponent->EmptyOverrideMaterials();
	for (const int32 SlotIndex : DimmedSlots)
	{
		MeshComponent->SetMaterial(SlotIndex, DimMaterial.Get());
	}
}

TSharedRef<FEditorViewportClient> SVMatMeshPreview::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FVMatMeshPreviewClient>(*PreviewScene, SharedThis(this));
	ViewportClient->SetRealtime(true);
	// 탭이 안 보이거나 mesh 없어서 숨겨져 있으면 안 그려
	ViewportClient->VisibilityDelegate.BindSP(this, &SVMatMeshPreview::IsVisible);
	return ViewportClient.ToSharedRef();
}

void SVMatMeshPreview::OnFocusViewportToSelection()
{
	FocusOnMesh();
}

void SVMatMeshPreview::FocusOnMesh()
{
	if (UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
	{
		ViewportClient->RequestFocus(FBox::BuildAABB(FVector::ZeroVector, Mesh->GetBounds().BoxExtent));
	}
}
