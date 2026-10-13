#pragma once

#include "UObject/SoftObjectPtr.h"
#include "VMatMultiSlotScan.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class AActor;
class FAssetThumbnailPool;
class SHorizontalBox;
class SSearchBox;
class UMaterialInterface;
class UStaticMesh;

// Multi-slot 탭. 위에 slot 여러개인 mesh 목록, 가운데 고른 mesh의 slot, 아래 preset material 줄
// slot 고르고 material 누르면 그 slot에만 들어가
class SVMatMultiSlotTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVMatMultiSlotTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SVMatMultiSlotTab() override;

	// 탭 열때 불러. 한번도 안 훑었거나 맵이 바뀌었을때만 훑어
	void ScanIfNeeded();

	// 이 mesh들을 목록에서 골라놓고 첫번째로 스크롤해
	void RevealMeshes(const TArray<UStaticMesh*>& Meshes);

	// 탭 이름에 붙일 개수. 아직 안 훑었으면 비어있어
	TOptional<int32> GetFoundCount() const;

	// Palette 탭에서 preset이나 material 바꿨을수도 있어서 탭 열때 다시 그려
	void RefreshMaterialStrip();

private:
	using FItem = TSharedPtr<FVMatMultiSlotMesh>;
	using FSlotItem = TSharedPtr<FVMatSlotGroup>;

	TSharedRef<SWidget> MakeMeshSection();
	TSharedRef<SWidget> MakeSlotSection();
	TSharedRef<SWidget> MakeMaterialStrip();

	// mesh 목록
	void Scan();
	void RebuildVisibleItems();
	void SelectMeshItems(const TSet<UStaticMesh*>& Meshes);
	void OnMapChanged(uint32 MapChangeFlags);
	TSharedRef<ITableRow> OnGenerateRow(FItem Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnMeshSelectionChanged(FItem Item, ESelectInfo::Type SelectInfo);
	void OnItemDoubleClicked(FItem Item);
	TSharedPtr<SWidget> OnContextMenuOpening();
	void OnSearchTextChanged(const FText& Text);
	void OnSortModeChanged(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type NewSortMode);
	EColumnSortMode::Type GetSortMode(FName ColumnId) const;

	// 고른 줄의 mesh를 쓰는 actor를 레벨에서 선택하고 카메라를 그쪽으로 옮겨
	void SelectActorsInLevel();
	void BrowseToMeshes();
	void OpenMeshEditors();
	bool HasSelection() const;
	bool CanOpenMeshEditors() const;
	TArray<UStaticMesh*> GetSelectedMeshes() const;

	FText GetStatusText() const;
	FText GetSelectionText() const;
	FText GetEmptyText() const;
	EVisibility GetEmptyVisibility() const;

	// slot 목록
	void RefreshSlotGroups();
	TSharedRef<ITableRow> OnGenerateSlotRow(FSlotItem Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGroupingChanged(EVMatSlotGrouping NewGrouping);
	bool HasSlotSelection() const;
	FText GetSlotHeaderText() const;

	// material 줄
	void RebuildStripTiles();
	void OnStripPresetSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	void ApplyToSelectedSlots(TSoftObjectPtr<UMaterialInterface> SoftMaterial);
	void BrowseToMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial);
	FText GetStripHintText() const;

	FVMatMultiSlotScanResult ScanResult;
	bool bHasScanned = false;
	bool bLevelChanged = false;

	TArray<FItem> VisibleItems;
	TSharedPtr<SListView<FItem>> ListView;
	TSharedPtr<SSearchBox> SearchBox;
	FText SearchText;
	FName SortColumn;
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;
	// 코드로 mesh 선택 바꾸는 중엔 slot 목록 한번만 다시 만들려고
	bool bUpdatingMeshSelection = false;

	TArray<FSlotItem> SlotGroups;
	TSharedPtr<SListView<FSlotItem>> SlotListView;
	EVMatSlotGrouping Grouping = EVMatSlotGrouping::Material;
	int32 NumSlotMeshes = 0;

	TSharedPtr<SHorizontalBox> MaterialStrip;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	FString StripPresetName;
	TArray<TSharedPtr<FString>> StripPresetOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> StripPresetCombo;

	FDelegateHandle MapChangedHandle;
};
