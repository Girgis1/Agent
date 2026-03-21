// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldUISystemTypes.generated.h"

UENUM(BlueprintType)
enum class EWorldUIActivationMode : uint8
{
	Disabled,
	InRange,
	Aimed,
	InRangeOrAimed,
	InRangeAndAimed
};

UENUM(BlueprintType)
enum class EWorldUIFacingMode : uint8
{
	None,
	FaceCamera,
	LimitedFaceCamera
};

USTRUCT(BlueprintType)
struct FWorldUIAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Action")
	FName ActionTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Action")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Action")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Action")
	FName NameValue = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Action")
	FString StringValue;
};

USTRUCT(BlueprintType)
struct FWorldUITextBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	FName FieldTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	FText Value;
};

USTRUCT(BlueprintType)
struct FWorldUIScalarBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	FName FieldTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	float Value = 0.0f;
};

USTRUCT(BlueprintType)
struct FWorldUIBoolBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	FName FieldTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	bool bValue = false;
};

USTRUCT(BlueprintType)
struct FWorldUIColorBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	FName FieldTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	FLinearColor Value = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct FWorldUIVisibilityBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	FName FieldTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	bool bVisible = true;
};

USTRUCT(BlueprintType)
struct FWorldUIModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Model")
	FLinearColor AccentColor = FLinearColor(0.18f, 0.9f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Model")
	TArray<FWorldUITextBinding> TextBindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Model")
	TArray<FWorldUIScalarBinding> ScalarBindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Model")
	TArray<FWorldUIBoolBinding> BoolBindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Model")
	TArray<FWorldUIColorBinding> ColorBindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Model")
	TArray<FWorldUIVisibilityBinding> VisibilityBindings;
};
