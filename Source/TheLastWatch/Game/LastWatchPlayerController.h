// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "HorrorPlayerController.h"
#include "LastWatchPlayerController.generated.h"

class AInteractableActor;
class ALastWatchHUD;
class ARadioSet;
class UInputAction;
class UInputMappingContext;
class UFootstepComponent;
class UInventoryComponent;
struct FInputActionValue;

/** What the keeper is currently doing; decides which inputs are live */
enum class EKeeperMode : uint8
{
	Explore,
	Reading,
	Radio,
	Inventory
};

/**
 *  The keeper's controller.
 *  Finds what the player is looking at, owns the inventory and switches between
 *  exploring, reading documents, using the radio and browsing the inventory.
 */
UCLASS(config="Game")
class THELASTWATCH_API ALastWatchPlayerController : public AHorrorPlayerController
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UFootstepComponent> Footsteps;

protected:

	/** How far the keeper can reach, in cm */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(Units="cm"))
	float InteractRange = 220.0f;

	/** Dial speed while holding A/D */
	UPROPERTY(EditAnywhere, Category="Radio", meta=(Units="kHz"))
	float TuneSpeed = 60.0f;

	/** Dial step per mouse wheel notch */
	UPROPERTY(EditAnywhere, Category="Radio", meta=(Units="kHz"))
	float TuneWheelStep = 4.0f;

	/** Capsule half height while crouched (standing is 96) */
	UPROPERTY(EditAnywhere, Category="Movement", meta=(Units="cm"))
	float CrouchedHalfHeight = 55.0f;

	UPROPERTY(EditAnywhere, Category="Movement", meta=(Units="cm/s"))
	float CrouchSpeed = 160.0f;

	/** Steepest ground the keeper can walk up, in degrees */
	UPROPERTY(EditAnywhere, Category="Movement")
	float WalkableSlope = 50.0f;

	/** Falling below this height (into the sea) puts the keeper back at the player start */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(Units="cm"))
	float RespawnBelowZ = -150.0f;

public:

	ALastWatchPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	UInventoryComponent* GetInventory() const { return Inventory; }

	/** Opens the document reader on the first page */
	void OpenNote(FName NoteId);

	/** Sits down at the radio */
	void EnterRadio(ARadioSet* Radio);

	/** Returns to exploring from any overlay */
	void ReturnToExplore();

	void ShowToast(const FText& Text);
	void SetSubtitle(const FText& Speaker, const FText& Line);

	// development console commands (open the console with the ` key)

	/** Opens a document by id */
	UFUNCTION(Exec)
	void DebugOpenNote(FName NoteId);

	/** Adds an item by id */
	UFUNCTION(Exec)
	void DebugGiveItem(FName ItemId);

	/** Opens the inventory */
	UFUNCTION(Exec)
	void DebugInventory();

	/** Sits at the first radio in the level, tuned to Frequency (kHz) */
	UFUNCTION(Exec)
	void DebugRadio(float Frequency);

	/** Starts night 1..5 from its beginning */
	UFUNCTION(Exec)
	void DebugSetNight(int32 Night);

	/** Sends a story event, e.g. DebugStoryEvent task:refuel */
	UFUNCTION(Exec)
	void DebugStoryEvent(const FString& Event);

	/** Completes the current objective by sending the event it waits for */
	UFUNCTION(Exec)
	void DebugSkipObjective();

	/** Skips an objective every Interval seconds until the game ends (story smoke test) */
	UFUNCTION(Exec)
	void DebugAutoplay(float Interval);

	/** Moves the keeper to X Y Z (cm) looking at Yaw / Pitch (degrees) */
	UFUNCTION(Exec)
	void DebugTeleport(float X, float Y, float Z, float Yaw, float Pitch);

	/** Logs every light within Radius cm of the keeper (intensity, units, position) */
	UFUNCTION(Exec)
	void DebugDumpLights(float Radius);

	/** Traces up from the middle of every lighthouse stair tread and logs any with less than Height clearance */
	UFUNCTION(Exec)
	void DebugStairHeadroom(float Height);

	/** Turns the lighthouse lamp on (1) or off (0) */
	UFUNCTION(Exec)
	void DebugLamp(bool bLit);

	/** Takes a screenshot with UI after Seconds, giving textures and shaders time to load */
	UFUNCTION(Exec)
	void DebugShotAfter(float Seconds);

	// state read by the HUD
	EKeeperMode GetMode() const { return Mode; }
	AInteractableActor* GetFocused() const { return Focused.Get(); }
	FName GetOpenNoteId() const { return OpenNoteId; }
	int32 GetNotePage() const { return NotePage; }
	ARadioSet* GetActiveRadio() const { return ActiveRadio.Get(); }
	int32 GetInventorySelection() const { return InventorySelection; }

	/** 0..1 while holding E on a hold task, 0 otherwise */
	float GetHoldProgress() const;

protected:

	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	void SetMode(EKeeperMode NewMode);
	void UpdateFocus();
	ALastWatchHUD* GetKeeperHUD() const;

	// input handlers
	void OnInteract();
	void OnInteractReleased();
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnCrouchHeld();
	void OnCrouchReleased();
	void OnCrouchToggle();
	void OnBack();
	void OnToggleInventory();
	void OnPagePrev();
	void OnPageNext();
	void OnListUp();
	void OnListDown();
	void OnTune(const FInputActionValue& Value);
	void OnTuneWheel(const FInputActionValue& Value);

private:

	/** Builds our input actions and mapping contexts in code, so no input assets are needed */
	void CreateKeeperInput();

	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchHoldAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchToggleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BackAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InventoryAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PagePrevAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PageNextAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ListUpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ListDownAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> TuneAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> TuneWheelAction;

	/** Always active: interact, inventory */
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> ExploreContext;

	/** Reader and inventory: page/list navigation, back. Blocks movement keys. */
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> OverlayContext;

	/** Radio: tuning, back. Blocks movement keys. */
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RadioContext;

	EKeeperMode Mode = EKeeperMode::Explore;
	bool bMovementBlocked = false;
	float LastBlockReport = 0.0f;

	TWeakObjectPtr<AInteractableActor> Focused;
	TWeakObjectPtr<ARadioSet> ActiveRadio;

	/** Story task being held, and for how long */
	TWeakObjectPtr<class AStoryTask> HoldTarget;
	float HoldTime = 0.0f;

	FName OpenNoteId;
	int32 NotePage = 0;

	/** True when the reader was opened from the inventory, so closing goes back there */
	bool bReadingFromInventory = false;
	int32 InventorySelection = 0;
};
