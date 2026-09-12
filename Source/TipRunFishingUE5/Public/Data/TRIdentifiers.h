#pragma once

#include "CoreMinimal.h"
#include "TRIdentifiers.generated.h"

// Zero is invalid. Allocation and monotonic increment belong to Game (M04/M06).

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRCastId
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 Value = 0;

	FTRCastId() = default;
	explicit FTRCastId(int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }
	bool operator==(const FTRCastId& Other) const { return Value == Other.Value; }
	bool operator!=(const FTRCastId& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const FTRCastId& Id) { return ::GetTypeHash(Id.Value); }
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRActorSimId
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 Value = 0;

	FTRActorSimId() = default;
	explicit FTRActorSimId(int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }
	bool operator==(const FTRActorSimId& Other) const { return Value == Other.Value; }
	bool operator!=(const FTRActorSimId& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const FTRActorSimId& Id) { return ::GetTypeHash(Id.Value); }
};

USTRUCT(BlueprintType)
struct TIPRUNFISHINGUE5_API FTRBiteToken
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	FTRCastId CastId = {};

	UPROPERTY(BlueprintReadOnly, Category = "TipRun")
	int64 Sequence = 0;

	bool IsValid() const { return CastId.IsValid() && Sequence > 0; }
	bool operator==(const FTRBiteToken& Other) const
	{
		return CastId == Other.CastId && Sequence == Other.Sequence;
	}
	bool operator!=(const FTRBiteToken& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const FTRBiteToken& Token)
	{
		return HashCombine(GetTypeHash(Token.CastId), ::GetTypeHash(Token.Sequence));
	}
};
