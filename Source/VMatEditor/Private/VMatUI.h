#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"

class STextBlock;

// 두 탭에서 같이 쓰는 작은 위젯들. 버튼 모양 통일하려고 모아놨어
namespace VMatUI
{
	// 아이콘만 있는 버튼. 뭐하는건지는 툴팁으로 알려줘
	TSharedRef<SWidget> IconButton(FName IconName, const FText& ToolTip, FOnClicked OnClicked, TAttribute<bool> IsEnabled = true);

	// 아이콘 + 글자 버튼
	TSharedRef<SWidget> TextButton(FName IconName, const FText& Label, const FText& ToolTip, FOnClicked OnClicked, TAttribute<bool> IsEnabled = true);

	// 작고 흐린 글자. 상태 줄이랑 안내 문구에 써
	TSharedRef<STextBlock> HintText(TAttribute<FText> Text);
}
