#pragma once

#include "AssetRegistry/AssetData.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnailPool;
class SEditableTextBox;
class SWidgetSwitcher;
class SWrapBox;
class UMaterialInterface;
class UStaticMesh;
struct FVMatPreset;

DECLARE_DELEGATE_OneParam(FOnShowMultiSlotMeshes, const TArray<UStaticMesh*>&);

// Palette 탭. 위에 preset, 가운데 material 타일, 아래 지금 선택한거 요약
class SVMatPaletteTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVMatPaletteTab) {}
		// 요약에서 multi-slot 보기 누르면 불러. 탭 넘기는건 바깥에서 해
		SLATE_EVENT(FOnShowMultiSlotMeshes, OnShowMultiSlotMeshes)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SVMatPaletteTab() override;

	// Multi-slot 탭에서 preset 바꿨으면 여기도 따라가
	void SyncPresetFromSettings();

private:
	TSharedRef<SWidget> MakePresetBar();
	TSharedRef<SWidget> MakeMaterialArea();
	TSharedRef<SWidget> MakeSelectionFooter();

	// preset
	FVMatPreset& GetCurrentPreset();
	void SelectPreset(const FString& Name);
	void RefreshPresetOptions();
	FText GetCurrentPresetText() const;
	void OnPresetSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	FReply OnRenamePresetClicked();
	void OnPresetNameCommitted(const FText& NewText, ETextCommit::Type CommitType);
	FReply OnNewPresetClicked();
	FReply OnDuplicatePresetClicked();
	FReply OnDeletePresetClicked();

	// material
	void RebuildMaterialGrid();
	bool HasMaterials() const;
	void AddMaterials(TConstArrayView<FAssetData> Assets);
	bool AreAssetsAcceptable(TArrayView<FAssetData> Assets) const;
	void OnAssetsDropped(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets);
	FReply OnAddSelectedClicked();
	void ApplyMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial);
	void RemoveMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial);
	void BrowseToMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial);

	// 선택 요약
	void RefreshSelectionSummary();
	void OnSelectionChanged(UObject* Selection);
	void OnShowMultiSlotClicked();

	FOnShowMultiSlotMeshes OnShowMultiSlotMeshes;

	FString CurrentPresetName;
	TArray<TSharedPtr<FString>> PresetNameOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PresetCombo;
	// 0 = 드롭다운, 1 = 이름 바꾸는 칸
	TSharedPtr<SWidgetSwitcher> PresetNameSwitcher;
	TSharedPtr<SEditableTextBox> PresetNameBox;
	bool bIsRenaming = false;

	TSharedPtr<SWrapBox> MaterialGrid;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	int32 NumSelectedActors = 0;
	int32 NumMeshesToChange = 0;
	FText SkippedText;
	FText SelectionLabels;
	// 선택 바뀌기 전까지 들고 있어서 weak로 둬
	TArray<TWeakObjectPtr<UStaticMesh>> SkippedMultiSlotMeshes;
	FDelegateHandle SelectionChangedHandle;
};
