// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "http.h"
#include "GPTClient.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatGPTResponse, const FString&, Response);

UCLASS()
class FOREST_API AGPTClient : public AActor
{
	GENERATED_BODY()

public:
	AGPTClient();

	UFUNCTION(BlueprintCallable)
	void SendChatGPTRequest(const FString& SystemPrompt, const FString& UserPrompt);

	UPROPERTY(BlueprintAssignable)
	FOnChatGPTResponse OnChatGPTResponse;

private:
	void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString LoadApiKeyFromFile();

	TSharedPtr<FJsonObject> Deserialize(FString String);
};
