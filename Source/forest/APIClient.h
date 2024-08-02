// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "http.h"
#include "Components/ActorComponent.h"
#include "APIClient.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatGPTResponse, const FString&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWhisperResponse, const FString&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTTSResponse, const FString&, Text);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FOREST_API UAPIClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UAPIClient();

	UFUNCTION(BlueprintCallable)
	void SendChatGPTRequest(const FString& SystemPrompt, const FString& UserPrompt);

	UFUNCTION(BlueprintCallable)
	void SendWhisperRequest(const FString& AudioFilePath);

	UFUNCTION(BlueprintCallable)
	void SendTTSRequest(const FString& Text);
	
	UFUNCTION(BlueprintCallable)
	USoundWaveProcedural* LoadSoundWaveFromFile(const FString& FilePath, TArray<uint8>& RawFileData);

	UPROPERTY(BlueprintAssignable)
	FOnChatGPTResponse OnChatGPTResponse;

	UPROPERTY(BlueprintAssignable)
	FOnWhisperResponse OnWhisperResponse;

	UPROPERTY(BlueprintAssignable)
	FOnTTSResponse OnTTSResponse;
private:
	void OnChatGPTResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnWhisperResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnTTSResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	void AppendStringToArray(TArray<uint8>& Array, const FString& String);

	FString LoadApiKeyFromFile();

	TSharedPtr<FJsonObject> Deserialize(FString String);
	bool ReconstructWavFile(TArray<uint8>& WavData);
};