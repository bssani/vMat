#include "SVMatMeshPreview.h"

#include "AdvancedPreviewScene.h"
#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/AppStyle.h"
#include "VMatUI.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SVMatMeshPreview"

namespace VMatMeshPreview
{
	// 처음 보는 각도. 앞쪽 오른편 위에서 내려다봐
	const FRotator DefaultViewRotation(-20.f, -135.f, 0.f);

	// 안 고른 slot 색. 원래 material이 밝든 어둡든 구분되게 어두운 회색
	const FLinearColor DimColor(0.04f, 0.04f, 0.045f);

	// 배율 범위. 1이면 mesh가 창에 딱 맞아
	constexpr float MinZoom = 0.1f;
	constexpr float MaxZoom = 50.f;
	// 휠 한칸에 바뀌는 배율
	constexpr float WheelZoomStep = 1.2f;

	// 배율은 사람마다 달라서 마지막 preset 이름이랑 같이 개인 설정(Saved 폴더)에 둬
	static const TCHAR* ConfigSection = TEXT("VMat");
	static const TCHAR* ZoomKey = TEXT("PreviewZoom");
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

	// 가운데 버튼 이동이랑 오른쪽 버튼 확대를 카메라 거리에 비례하게. 작은 부품에서 확 날아가지 않게
	virtual bool ShouldScaleCameraSpeedByDistance() const override
	{
		return true;
	}

	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override
	{
		// 엔진 기본 휠은 한칸에 수십 cm씩 움직여서 몇 cm짜리 부품은 지나쳐버려. 배율로 확대/축소해
		const bool bWheel = EventArgs.Key == EKeys::MouseScrollUp || EventArgs.Key == EKeys::MouseScrollDown;
		if (bWheel && EventArgs.Event == IE_Pressed && IsFramed())
		{
			const float Step = EventArgs.Key == EKeys::MouseScrollUp ? VMatMeshPreview::WheelZoomStep : 1.f / VMatMeshPreview::WheelZoomStep;
			SetZoom(GetZoom().GetValue() * Step);
			return true;
		}
		return FEditorViewportClient::InputKey(EventArgs);
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);

		// 창 크기를 알아야 딱 맞는 거리를 구해서, 한번 그려져서 크기 생긴 다음에 맞춰
		if (PendingFrame.IsSet() && Viewport && Viewport->GetSizeXY().GetMin() > 0)
		{
			ApplyFrame(PendingFrame.GetValue());
			PendingFrame.Reset();
		}
	}

	// 원점에 놓인 반지름 Radius짜리 mesh를 Zoom 배율로 보게 카메라 옮겨
	void RequestFrame(float Radius, float Zoom)
	{
		PendingFrame = FFrameRequest{ Radius, Zoom };
	}

	// 아직 mesh에 안 맞췄으면 비어있어
	TOptional<float> GetZoom() const
	{
		if (!IsFramed())
		{
			return {};
		}
		const float Distance = FVector::Dist(GetViewLocation(), GetLookAtLocation());
		return FitDistance / FMath::Max(Distance, UE_KINDA_SMALL_NUMBER);
	}

	void SetZoom(float Zoom)
	{
		Zoom = FMath::Clamp(Zoom, VMatMeshPreview::MinZoom, VMatMeshPreview::MaxZoom);
		if (PendingFrame.IsSet())
		{
			PendingFrame->Zoom = Zoom;
			return;
		}
		if (!IsFramed())
		{
			return;
		}

		// 보는 방향이랑 중심은 그대로 두고 거리만 바꿔. orbit 중이어도 거리는 이 둘 사이 길이라 똑같이 돼
		const FVector LookAt = GetLookAtLocation();
		FVector Direction = (GetViewLocation() - LookAt).GetSafeNormal();
		if (Direction.IsZero())
		{
			Direction = -GetViewRotation().Vector();
		}
		SetViewLocation(LookAt + Direction * (FitDistance / Zoom));
		Invalidate();
	}

private:
	struct FFrameRequest
	{
		float Radius = 0.f;
		float Zoom = 1.f;
	};

	bool IsFramed() const
	{
		return FitDistance > 0.f && !PendingFrame.IsSet();
	}

	void ApplyFrame(const FFrameRequest& Request)
	{
		const FIntPoint Size = Viewport->GetSizeXY();
		const float Radius = FMath::Max(Request.Radius, 0.01f);

		// 엔진 FocusViewportOnBox는 반지름을 10cm 밑으로 안 내려서 작은 부품이 콩알만하게 나와. 직접 구해
		// 가로 세로 화각 중 좁은 쪽에 mesh 감싸는 구가 딱 들어가는 거리
		const float HalfFOV = FMath::DegreesToRadians(ViewFOV * 0.5f);
		const float HalfVerticalFOV = FMath::Atan(FMath::Tan(HalfFOV) * Size.Y / Size.X);
		FitDistance = Radius / FMath::Sin(FMath::Min(HalfFOV, HalfVerticalFOV));

		// 바짝 다가가도 앞이 안 잘리게 near clip도 크기 따라 줄여
		OverrideNearClipPlane(FMath::Clamp(Radius * 0.01f, 0.01f, 1.f));

		// orbit 중이면 회전값 의미가 달라서 일반 카메라로 돌려놓고 옮겨. 끌면 엔진이 다시 orbit으로 바꿔
		ToggleOrbitCamera(false);
		SetLookAtLocation(FVector::ZeroVector);
		const float Zoom = FMath::Clamp(Request.Zoom, VMatMeshPreview::MinZoom, VMatMeshPreview::MaxZoom);
		SetViewLocation(-GetViewRotation().Vector() * (FitDistance / Zoom));
		Invalidate();
	}

	TOptional<FFrameRequest> PendingFrame;
	// 배율 100%일때 카메라 거리
	float FitDistance = 0.f;
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
	if (GConfig->GetFloat(VMatMeshPreview::ConfigSection, VMatMeshPreview::ZoomKey, Zoom, GEditorPerProjectIni))
	{
		Zoom = FMath::Clamp(Zoom, VMatMeshPreview::MinZoom, VMatMeshPreview::MaxZoom);
	}

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

	// 휠로 바꾼 배율도 다음 mesh에 이어서 써
	RememberZoom();

	// 전 mesh에 칠해둔거 남으면 slot 번호가 엉켜서 비워
	MeshComponent->EmptyOverrideMaterials();
	DimmedSlots.Reset();
	MeshComponent->SetStaticMesh(Mesh);

	// CAD mesh는 원점에서 멀리 있을때가 많아. 원점으로 당겨와서 조명이랑 하늘 한가운데 놓고 봐
	if (Mesh)
	{
		MeshComponent->SetRelativeLocation(-Mesh->GetBounds().Origin);
		FrameMesh();
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

void SVMatMeshPreview::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	// 오른쪽 아래 배율 칸. 숫자 쓰거나 좌우로 끌어서 바꿔
	Overlay->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Bottom)
	.Padding(8.f)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(FMargin(8.f, 2.f, 2.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				VMatUI::HintText(LOCTEXT("Zoom", "Zoom"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(64.f)
				[
					SNew(SSpinBox<int32>)
					.MinValue(FMath::RoundToInt(VMatMeshPreview::MinZoom * 100.f))
					.MaxValue(FMath::RoundToInt(VMatMeshPreview::MaxZoom * 100.f))
					.MinSliderValue(FMath::RoundToInt(VMatMeshPreview::MinZoom * 100.f))
					.MaxSliderValue(1000)
					.SliderExponent(2.f)
					.Delta(5)
					.Value(this, &SVMatMeshPreview::GetZoomPercent)
					.OnValueChanged(this, &SVMatMeshPreview::OnZoomPercentChanged)
					.OnValueCommitted(this, &SVMatMeshPreview::OnZoomPercentCommitted)
					.ToolTipText(LOCTEXT("ZoomTooltip", "How big the mesh shows. 100% fits it to the view.\nType a number or drag sideways. The mouse wheel changes it too.\nThe next mesh opens at the same zoom."))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3.f, 0.f, 0.f, 0.f)
			[
				VMatUI::HintText(LOCTEXT("Percent", "%"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 0.f, 0.f)
			[
				VMatUI::IconButton("GenericCurveEditor.ZoomToFit", LOCTEXT("FitTooltip", "Fit the mesh to the view (100%)"),
					FOnClicked::CreateSP(this, &SVMatMeshPreview::OnFitClicked))
			]
		]
	];
}

void SVMatMeshPreview::OnFocusViewportToSelection()
{
	RememberZoom();
	FrameMesh();
}

void SVMatMeshPreview::FrameMesh()
{
	if (UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
	{
		ViewportClient->RequestFrame(Mesh->GetBounds().SphereRadius, Zoom);
	}
}

void SVMatMeshPreview::SetZoom(float NewZoom)
{
	Zoom = FMath::Clamp(NewZoom, VMatMeshPreview::MinZoom, VMatMeshPreview::MaxZoom);
	ViewportClient->SetZoom(Zoom);
}

void SVMatMeshPreview::RememberZoom()
{
	const TOptional<float> CurrentZoom = ViewportClient->GetZoom();
	if (CurrentZoom.IsSet())
	{
		Zoom = FMath::Clamp(CurrentZoom.GetValue(), VMatMeshPreview::MinZoom, VMatMeshPreview::MaxZoom);
		SaveZoom();
	}
}

void SVMatMeshPreview::SaveZoom() const
{
	GConfig->SetFloat(VMatMeshPreview::ConfigSection, VMatMeshPreview::ZoomKey, Zoom, GEditorPerProjectIni);
}

int32 SVMatMeshPreview::GetZoomPercent() const
{
	return FMath::RoundToInt(ViewportClient->GetZoom().Get(Zoom) * 100.f);
}

void SVMatMeshPreview::OnZoomPercentChanged(int32 Percent)
{
	SetZoom(Percent / 100.f);
}

void SVMatMeshPreview::OnZoomPercentCommitted(int32 Percent, ETextCommit::Type CommitType)
{
	SetZoom(Percent / 100.f);
	SaveZoom();
}

FReply SVMatMeshPreview::OnFitClicked()
{
	SetZoom(1.f);
	SaveZoom();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
