// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

/**
 *  Keeps track of the items and documents the keeper has collected.
 *  Items and notes are referenced by their ids in Content/Data.
 */
UCLASS(ClassGroup=(LastWatch), meta=(BlueprintSpawnableComponent))
class THELASTWATCH_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Inventory")
	TArray<FName> Items;

	UPROPERTY(VisibleAnywhere, Category="Inventory")
	TArray<FName> Notes;

	/** Every item ever received, including ones since used up */
	UPROPERTY(VisibleAnywhere, Category="Inventory")
	TArray<FName> Taken;

public:

	void AddItem(FName ItemId) { Items.AddUnique(ItemId); Taken.AddUnique(ItemId); }
	bool RemoveItem(FName ItemId) { return Items.Remove(ItemId) > 0; }
	bool HasItem(FName ItemId) const { return Items.Contains(ItemId); }

	void AddNote(FName NoteId) { Notes.AddUnique(NoteId); }
	bool HasNote(FName NoteId) const { return Notes.Contains(NoteId); }

	const TArray<FName>& GetItems() const { return Items; }
	const TArray<FName>& GetNotes() const { return Notes; }
	const TArray<FName>& GetTaken() const { return Taken; }

	/** Replaces everything, e.g. when loading a saved night */
	void SetContents(const TArray<FName>& InItems, const TArray<FName>& InNotes, const TArray<FName>& InTaken)
	{
		Items = InItems;
		Notes = InNotes;
		Taken = InTaken;
	}
};
