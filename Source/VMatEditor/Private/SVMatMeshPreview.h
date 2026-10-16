#pragma once

#include "SEditorViewport.h"
#include "UObject/StrongObjectPtr.h"

class FAdvancedPreviewScene;
class FVMatMeshPreviewClient;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

// Multi-slot 탭 오른쪽 미리보기. mesh 하나 띄워놓고, slot 고르면 나머지 slot을 어둡게 칠해서 어느 부분인지 보여줘
class SVMatMeshPreview : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SVMatMeshPreview) {}
	SLATE_END_ARGS()

	SVMatMeshPreview();
	virtual ~SVMatMeshPreview() override;

	void Construct(const FArguments& InArgs);

	// nullptr면 비워. 다른 mesh로 바뀔때만 카메라를 mesh에 맞춰
	void SetMesh(UStaticMesh* Mesh);
	UStaticMesh* GetMesh() const;

	// 이 slot들은 어둡게 칠해. 비어있으면 다 원래 material로
	void SetDimmedSlots(const TSet<int32>& SlotIndices);

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	// F 누르면 지금 배율 그대로 mesh를 가운데로
	virtual void OnFocusViewportToSelection() override;

private:
	void FrameMesh();

	// 배율. 1이면 mesh가 창에 딱 맞아
	void SetZoom(float NewZoom);
	void RememberZoom();
	void SaveZoom() const;
	int32 GetZoomPercent() const;
	void OnZoomPercentChanged(int32 Percent);
	void OnZoomPercentCommitted(int32 Percent, ETextCommit::Type CommitType);
	FReply OnFitClicked();

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FVMatMeshPreviewClient> ViewportClient;
	// preview scene이 GC에서 지켜줘서 raw로 들고 있어도 돼
	UStaticMeshComponent* MeshComponent = nullptr;
	TStrongObjectPtr<UMaterialInstanceDynamic> DimMaterial;
	TSet<int32> DimmedSlots;
	// 다음 mesh도 이 배율로 열어. 개인 설정에 저장돼서 에디터 껐다 켜도 남아
	float Zoom = 1.f;
};
