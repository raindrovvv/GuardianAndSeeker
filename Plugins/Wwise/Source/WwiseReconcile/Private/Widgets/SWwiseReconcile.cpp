/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unreal(R) Engine End User
License Agreement at https://www.unrealengine.com/en-US/eula/unreal

License Usage

Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2025 Audiokinetic Inc.
*******************************************************************************/

#include "Widgets/SWwiseReconcile.h"

#include "AkAudioStyle.h"
#include "Widgets/ProjectedResultColumn.h"
#include "Widgets/ReconcileOperationColumn.h"
#include "Widgets/ReconcileUEAssetStatusColumn.h"
#include "Widgets/SWwiseReconcileItemView.h"
#include "Widgets/WwiseReconcileObjectColumn.h"
#include "Wwise/WwiseReconcile.h"
#include "WwiseUEFeatures.h"

#include "Editor.h"
#include "Interfaces/IMainFrameModule.h"
#include "ObjectTools.h"
#include "Misc/EnumClassFlags.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "AkAudio"

void SWwiseReconcile::Construct(const FArguments& InArgs, const TArray< FWwiseReconcileItem >& InReconcileItems, TSharedRef<SWindow>& ReconcileWindow)
{
	Window = ReconcileWindow;
	bool bOnlyReferencedAssets = true;
	for(auto& Item : InReconcileItems)
	{
		ReconcileItems.Add(MakeShared<FWwiseReconcileItem>(Item));

		if (bOnlyReferencedAssets)
		{
			if (EnumHasAllFlags(Item.OperationRequired, EWwiseReconcileOperationFlags::Delete))
			{
				bool bReferenced = false;
				bool bReferencedByUndo = false;
				ObjectTools::GatherObjectReferencersForDeletion(Item.Asset.GetAsset(), bReferenced, bReferencedByUndo);

				if (!bReferenced && !bReferencedByUndo)
				{
					bOnlyReferencedAssets = false;
				}
			}
			else
			{
				bOnlyReferencedAssets = false;
			}
		}
	}

	HeaderRowWidget =
		SNew(SHeaderRow)
		.CanSelectGeneratedColumn(true);

	SetupColumns(*HeaderRowWidget);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(4)
		.BorderImage(FAkAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				// Description
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 10.f, 0.f, 10.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					.Visibility(EVisibility::Visible)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					.Padding(0.f, 0.f, 2.f, 0.f)
					[
						SNew(SImage)
						.Image(FAkAppStyle::Get().GetBrush("Icons.Warning"))
						.ColorAndOpacity(FSlateColor(FColor::Yellow))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ReconcileWarning", "Make sure the SoundBanks are properly generated before doing the Reconciliation process."))
						.ColorAndOpacity(FSlateColor(FColor::Yellow))
					]
				]
				// List
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(ReconcileList, SWwiseReconcileListView, StaticCastSharedRef<SWwiseReconcile>(AsShared()))
					.ListItemsSource(&ReconcileItems)
					.OnGenerateRow(this, &SWwiseReconcile::GenerateRow)
					.ClearSelectionOnClick(false)
					.SelectionMode(ESelectionMode::None)
					.HeaderRow(HeaderRowWidget)
					.OnMouseButtonDoubleClick(this, &SWwiseReconcile::OnReconcileItemDoubleClicked)
				]
				+ SVerticalBox::Slot()
				.Padding(0.f, 4.f, 0.f, 4.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(bOnlyReferencedAssets ? LOCTEXT("ManualActionWarning", "Manual actions are required. Please follow the instruction for each element.") : LOCTEXT("ManualActionWarningBlank", ""))
					.ColorAndOpacity(FSlateColor(FColor::Yellow))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 4.f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("ReconcileButton", "Reconcile Unreal Assets"))
						.IsEnabled(!bOnlyReferencedAssets)
						.OnClicked(this, &SWwiseReconcile::ReconcileAssets)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.OnClicked(this, &SWwiseReconcile::CloseWindow)
					]
				]
			]
		]
	];
}

SWwiseReconcile::SWwiseReconcile()
{
}

void SWwiseReconcile::SetupColumns(SHeaderRow& HeaderRow)
{
	Columns.Empty();
	HeaderRow.ClearColumns();

	auto WwiseObjectsColumn = MakeShared<FWwiseReconcileObjectColumn>();
	auto OperationColumn = MakeShared<FReconcileOperationColumn>();
	auto UEAssetStatusColumn = MakeShared<FReconcileUEAssetStatusColumn>();
	auto ProjectedResultColumn = MakeShared<FProjectedResultColumn>();

	auto WwiseObjectArgs = WwiseObjectsColumn->ConstructHeaderRowColumn();
	auto OperationArgs = OperationColumn->ConstructHeaderRowColumn();
	auto UEAssetStatusArgs = UEAssetStatusColumn->ConstructHeaderRowColumn();
	auto ProjectedResultsArgs = ProjectedResultColumn->ConstructHeaderRowColumn();

	Columns.Add(WwiseObjectArgs._ColumnId, WwiseObjectsColumn);
	Columns.Add(OperationArgs._ColumnId, OperationColumn);
	Columns.Add(UEAssetStatusArgs._ColumnId, UEAssetStatusColumn);
	Columns.Add(ProjectedResultsArgs._ColumnId, ProjectedResultColumn);

	HeaderRowWidget->AddColumn(WwiseObjectArgs);
	HeaderRowWidget->AddColumn(OperationArgs);
	HeaderRowWidget->AddColumn(UEAssetStatusArgs);
	HeaderRowWidget->AddColumn(ProjectedResultsArgs);
}

TSharedRef<ITableRow> SWwiseReconcile::GenerateRow(TSharedPtr<FWwiseReconcileItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Item.IsValid());

	TSharedRef<SWwiseReconcileRow> NewRow = SNew(SWwiseReconcileRow, ReconcileList.ToSharedRef(), SharedThis(this)).
		Item(Item);

	return NewRow;
}

FReply SWwiseReconcile::CloseWindow()
{
	Window.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

void SWwiseReconcile::OnReconcileItemDoubleClicked(TSharedPtr<FWwiseReconcileItem> item)
{
	CloseWindow();
	GEditor->SyncBrowserToObject(item->Asset);
}

FReply SWwiseReconcile::ReconcileAssets()
{
	CloseWindow();
	IWwiseReconcile::Get()->ReconcileAssets();
	return FReply::Handled();
}