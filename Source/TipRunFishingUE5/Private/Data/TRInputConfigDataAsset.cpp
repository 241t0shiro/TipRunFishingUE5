#include "Data/TRInputConfigDataAsset.h"
#include "Misc/DataValidation.h"

bool UTRInputConfigDataAsset::Validate(TArray<FText>& Errors) const
{
	bool bValid = IsValid(FishingContext) && Bindings.Num() == 9;
	TSet<ETRPlayerAction> Commands;
	TSet<const UInputAction*> Actions;
	for (const FTRInputBinding& B : Bindings)
	{
		bValid &= StaticEnum<ETRPlayerAction>()->IsValidEnumValue(int64(B.Command)) && !Commands.Contains(B.Command) &&
			IsValid(B.Action) && !Actions.Contains(B.Action);
		if (IsValid(B.Action)) { bValid &= B.Action->ValueType == EInputActionValueType::Boolean; }
		Commands.Add(B.Command); Actions.Add(B.Action);
		bool bMapped = false;
		if (IsValid(FishingContext)) { for (const auto& M : FishingContext->GetMappings()) { bMapped |= M.Action == B.Action && M.Key.IsValid(); } }
		bValid &= bMapped;
	}
	if (!bValid) { Errors.Add(FText::FromString(TEXT("Input config requires one mapped Boolean action for each M09 command, without duplicate commands/actions"))); }
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
#if WITH_EDITOR
EDataValidationResult UTRInputConfigDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors; const bool bValid = Validate(Errors);
	for (const FText& Error : Errors) { Context.AddError(Error); }
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
