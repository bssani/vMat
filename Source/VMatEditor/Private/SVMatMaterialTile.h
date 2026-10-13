#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnail;

// material 타일 하나. 누르면 입히고, 마우스 올리면 X 버튼 보여. 우클릭하면 메뉴
// Palette 탭이랑 Multi-slot 탭 아래 줄에서 같이 써
class SVMatMaterialTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVMatMaterialTile)
		: _ThumbnailSize(80)
	{}
		// 못 찾는 material이면 비워서 넘겨
		SLATE_ARGUMENT(TSharedPtr<FAssetThumbnail>, Thumbnail)
		SLATE_ARGUMENT(int32, ThumbnailSize)
		SLATE_ARGUMENT(FText, Name)
		SLATE_ARGUMENT(FText, Path)
		// 툴팁 마지막 줄. 누르면 어디에 입혀지는지 알려줘
		SLATE_ARGUMENT(FText, ApplyHint)
		SLATE_EVENT(FSimpleDelegate, OnApply)
		// 안 넘기면 X 버튼이랑 메뉴의 빼기 항목이 없어
		SLATE_EVENT(FSimpleDelegate, OnRemove)
		SLATE_EVENT(FSimpleDelegate, OnBrowse)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 우클릭은 여기서 먹어야 바깥 스크롤박스가 드래그 스크롤로 안 가져가
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TSharedRef<SWidget> MakeMissingContent() const;
	EVisibility GetRemoveButtonVisibility() const;

	TSharedPtr<FAssetThumbnail> Thumbnail;
	FSimpleDelegate OnApply;
	FSimpleDelegate OnRemove;
	FSimpleDelegate OnBrowse;
	bool bMissing = false;
};
