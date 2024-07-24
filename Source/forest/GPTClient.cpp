// Fill out your copyright notice in the Description page of Project Settings.


#include "GPTClient.h"
#include "Http.h"
#include "Json.h"
#include "JsonUtilities.h"

AGPTClient::AGPTClient()
{
}

void AGPTClient::SendChatGPTRequest(const FString& SystemPrompt, const FString& UserPrompt)
{
    FString ApiKey = LoadApiKeyFromFile();
    if (ApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("API Key is missing!"));
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

    Request->OnProcessRequestComplete().BindUObject(this, &AGPTClient::OnResponseReceived);
    Request->SetURL("https://api.openai.com/v1/chat/completions");
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *ApiKey));

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField("model", "gpt-4o-mini");

    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    TSharedPtr<FJsonObject> MessageObject1 = MakeShareable(new FJsonObject);
    MessageObject1->SetStringField("role", "system");
    MessageObject1->SetStringField("content", SystemPrompt);
    MessagesArray.Add(MakeShareable(new FJsonValueObject(MessageObject1)));

    TSharedPtr<FJsonObject> MessageObject2 = MakeShareable(new FJsonObject);
    MessageObject2->SetStringField("role", "user");
    MessageObject2->SetStringField("content", UserPrompt);
    MessagesArray.Add(MakeShareable(new FJsonValueObject(MessageObject2)));

    JsonObject->SetArrayField("messages", MessagesArray);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    Request->SetContentAsString(RequestBody);

    Request->ProcessRequest();
}

void AGPTClient::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        FString ResponseString = Response->GetContentAsString();
        UE_LOG(LogTemp, Log, TEXT("Response: %s"), *ResponseString);

        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

        if (FJsonSerializer::Deserialize(Reader, JsonObject))
        {
            TArray<TSharedPtr<FJsonValue>> Choices = JsonObject->GetArrayField("choices");
            if (Choices.Num() > 0)
            {
                TSharedPtr<FJsonObject> Choice = Choices[0]->AsObject();
                TSharedPtr<FJsonObject> Message = Choice->GetObjectField("message");
                FString Content = Message->GetStringField("content");


                UE_LOG(LogTemp, Log, TEXT("Response: %s"), *Content);
                OnChatGPTResponse.Broadcast(Content);
            }
        }
        
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Request failed"));
    }
}

FString AGPTClient::LoadApiKeyFromFile()
{
    FString ApiKey;
    FString FilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("apikey.txt"));

    if (FFileHelper::LoadFileToString(ApiKey, *FilePath))
    {
        ApiKey = ApiKey.TrimStartAndEnd();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load API key from file: %s"), *FilePath);
    }

    return ApiKey;
}