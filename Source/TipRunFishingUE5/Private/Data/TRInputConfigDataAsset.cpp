#include "Data/TRInputConfigDataAsset.h"
#include "Misc/DataValidation.h"
#include "InputModifiers.h"

bool UTRInputConfigDataAsset::Validate(TArray<FText>& Errors) const
{
	bool bValid = IsValid(FishingContext);
	if(NavigationContext || NavigationThrottle || NavigationSteering || NavigationLook || NavigationZoom || NavigationFishingStart || NavigationBoost)
	{
		bValid &= IsValid(NavigationContext) && NavigationContext != FishingContext;
		for(const UInputAction* A : {NavigationThrottle.Get(),NavigationSteering.Get(),NavigationLook.Get(),NavigationZoom.Get(),NavigationFishingStart.Get(),NavigationBoost.Get()})
		{
			bool bMapped=false;
			if(IsValid(NavigationContext)){for(const auto& M:NavigationContext->GetMappings()){bMapped |= M.Action==A && M.Key.IsValid();}}
			bValid &= IsValid(A) && bMapped;
			if(A){bValid &= A->ValueType==((A==NavigationFishingStart || A==NavigationBoost)?EInputActionValueType::Boolean:A==NavigationLook?EInputActionValueType::Axis2D:EInputActionValueType::Axis1D);}
		}
	}
	TSet<ETRPlayerAction> Commands;
	TSet<const UInputAction*> Actions;
	for (const FTRInputBinding& B : Bindings)
	{
		bValid &= StaticEnum<ETRPlayerAction>()->IsValidEnumValue(int64(B.Command)) && !Commands.Contains(B.Command) &&
			IsValid(B.Action) && !Actions.Contains(B.Action);
		if (IsValid(B.Action)) { bValid &= B.Action->ValueType == (B.Command==ETRPlayerAction::RodAim?EInputActionValueType::Axis2D:EInputActionValueType::Boolean); }
		Commands.Add(B.Command); Actions.Add(B.Action);
		bool bMapped = false;
		if (IsValid(FishingContext)) { for (const auto& M : FishingContext->GetMappings()) { bMapped |= M.Action == B.Action && M.Key.IsValid(); } }
		bValid &= bMapped;
	}
	for(auto Required:{ETRPlayerAction::Deploy,ETRPlayerAction::Jerk,ETRPlayerAction::Fall,ETRPlayerAction::TensionFall,ETRPlayerAction::Hook,
		ETRPlayerAction::Retrieve,ETRPlayerAction::Cancel,ETRPlayerAction::NextCast,ETRPlayerAction::Pause}){bValid &= Commands.Contains(Required);}
	if (!bValid) { Errors.Add(FText::FromString(TEXT("Input config requires mapped semantic actions without duplicates: Boolean commands and optional Axis2D RodAim"))); }
	return bValid;
}
UTRInputConfigDataAsset* UTRInputConfigDataAsset::CreatePrototype(UObject* Outer)
{
	auto* Config = NewObject<UTRInputConfigDataAsset>(Outer);
	Config->FishingContext = NewObject<UInputMappingContext>(Config, TEXT("IMC_TR_Prototype"));
	const FKey Keyboard[] = { EKeys::Enter, EKeys::SpaceBar, EKeys::F, EKeys::T, EKeys::H, EKeys::R, EKeys::BackSpace, EKeys::N, EKeys::P };
	const FKey Gamepad[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_RightShoulder, EKeys::Gamepad_FaceButton_Left,
		EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_RightTrigger,
		EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Left, EKeys::Gamepad_Special_Right };
	for (int32 I = 0; I < UE_ARRAY_COUNT(Keyboard); ++I)
	{
		FTRInputBinding B; B.Command = ETRPlayerAction(I);
		B.Action = NewObject<UInputAction>(Config, FName(*FString::Printf(TEXT("IA_TR_Prototype_%d"), I)));
		B.Action->ValueType = EInputActionValueType::Boolean;
		B.Action->bTriggerWhenPaused = B.Command == ETRPlayerAction::Pause;
		Config->FishingContext->MapKey(B.Action, Keyboard[I]); Config->FishingContext->MapKey(B.Action, Gamepad[I]);
		Config->Bindings.Add(B);
	}
	return Config;
}
UTRInputConfigDataAsset* UTRInputConfigDataAsset::CreateMouseRodPrototype(UObject* Outer)
{
	auto* Config=CreatePrototype(Outer);
	for(const auto& B:Config->Bindings){if(B.Command==ETRPlayerAction::Jerk){Config->FishingContext->MapKey(B.Action,EKeys::RightMouseButton);}}
	FTRInputBinding B;B.Command=ETRPlayerAction::RodAim;
	B.Action=NewObject<UInputAction>(Config,TEXT("IA_TR_MouseRod_Prototype"));B.Action->ValueType=EInputActionValueType::Axis2D;
	Config->FishingContext->MapKey(B.Action,EKeys::Mouse2D);Config->Bindings.Add(B);return Config;
}
UTRInputConfigDataAsset* UTRInputConfigDataAsset::CreateMouseRetrievePrototype(UObject* Outer)
{
	auto* Config = CreateMouseRodPrototype(Outer);
	for (const auto& Binding : Config->Bindings)
	{
		if (Binding.Command == ETRPlayerAction::Retrieve)
		{
			Config->FishingContext->MapKey(Binding.Action, EKeys::LeftMouseButton);
		}
	}
	FTRInputBinding Quick; Quick.Command = ETRPlayerAction::QuickRetrieve;
	Quick.Action = NewObject<UInputAction>(Config, TEXT("IA_TR_QuickRetrieve_Prototype"));
	Quick.Action->ValueType = EInputActionValueType::Boolean;
	Config->FishingContext->MapKey(Quick.Action, EKeys::Q);
	Config->Bindings.Add(Quick);
	return Config;
}
#if WITH_EDITOR
EDataValidationResult UTRInputConfigDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors; const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

void UTRInputConfigDataAsset::CreateNavigationPrototype()
{
 if(!Bindings.ContainsByPredicate([](const auto& B){return B.Command==ETRPlayerAction::ReturnNavigation;}))
 {
  FTRInputBinding B;B.Command=ETRPlayerAction::ReturnNavigation;
  B.Action=NewObject<UInputAction>(this,TEXT("IA_ReturnNavigation"));B.Action->ValueType=EInputActionValueType::Boolean;
  FishingContext->MapKey(B.Action,EKeys::E);Bindings.Add(B);
 }
 if(!NavigationContext)
 {
 NavigationContext=NewObject<UInputMappingContext>(this,TEXT("IMC_Navigation_Prototype"));
 auto Action=[&](const TCHAR* Name,EInputActionValueType Type){auto* A=NewObject<UInputAction>(this,Name);A->ValueType=Type;return A;};
 NavigationThrottle=Action(TEXT("IA_NavigationThrottle"),EInputActionValueType::Axis1D);
 NavigationSteering=Action(TEXT("IA_NavigationSteering"),EInputActionValueType::Axis1D);
 NavigationLook=Action(TEXT("IA_NavigationLook"),EInputActionValueType::Axis2D);
 NavigationZoom=Action(TEXT("IA_NavigationZoom"),EInputActionValueType::Axis1D);
 NavigationContext->MapKey(NavigationThrottle,EKeys::W);
 NavigationContext->MapKey(NavigationThrottle,EKeys::S).Modifiers.Add(NewObject<UInputModifierNegate>(this));
 NavigationContext->MapKey(NavigationSteering,EKeys::D);
 NavigationContext->MapKey(NavigationSteering,EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(this));
 NavigationContext->MapKey(NavigationLook,EKeys::Mouse2D);
 NavigationContext->MapKey(NavigationZoom,EKeys::MouseWheelAxis);
 // Pause is shared, but fishing actions are never mapped in this context.
 for(const auto& Binding:Bindings){if(Binding.Command==ETRPlayerAction::Pause){NavigationContext->MapKey(Binding.Action,EKeys::P);}}
 }
 if(!NavigationBoost)
 {
  NavigationBoost=NewObject<UInputAction>(this,TEXT("IA_NavigationBoost"));
  NavigationBoost->ValueType=EInputActionValueType::Boolean;
  NavigationContext->MapKey(NavigationBoost,EKeys::LeftShift);
  NavigationContext->MapKey(NavigationBoost,EKeys::RightShift);
 }
 // Explicit asset migration may add the entry action without replacing existing mappings.
 if(!NavigationFishingStart)
 {
  NavigationFishingStart=NewObject<UInputAction>(this,TEXT("IA_NavigationFishingStart"));
  NavigationFishingStart->ValueType=EInputActionValueType::Boolean;
  NavigationContext->MapKey(NavigationFishingStart,EKeys::Enter);
 }
}
