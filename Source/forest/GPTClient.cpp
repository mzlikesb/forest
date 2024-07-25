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
    TSharedPtr<FJsonObject> SystemMessage = MakeShareable(new FJsonObject);
    SystemMessage->SetStringField("role", "system");
    SystemMessage->SetStringField("content", SystemPrompt + TEXT(" Please use JSON format for the response."));
    MessagesArray.Add(MakeShareable(new FJsonValueObject(SystemMessage)));

    TSharedPtr<FJsonObject> UserMessage = MakeShareable(new FJsonObject);
    UserMessage->SetStringField("role", "user");
    UserMessage->SetStringField("content", UserPrompt);
    MessagesArray.Add(MakeShareable(new FJsonValueObject(UserMessage)));

    JsonObject->SetArrayField("messages", MessagesArray);

    TSharedPtr<FJsonObject> ResponseFormatObject = MakeShareable(new FJsonObject);
    ResponseFormatObject->SetStringField("type", "json_object");
    JsonObject->SetObjectField("response_format", ResponseFormatObject);

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
        try {
            TSharedPtr<FJsonObject> JsonObject = Deserialize(ResponseString);
            TArray<TSharedPtr<FJsonValue>> Choices = JsonObject->GetArrayField("choices");
            if (Choices.IsEmpty())
            {
                UE_LOG(LogTemp, Error, TEXT("Invalid choice object."));
                return;
            }
            TSharedPtr<FJsonObject> Choice = Choices[0]->AsObject();
            TSharedPtr<FJsonObject> Message = Choice->GetObjectField("message");
            FString Content = Message->GetStringField("content");

            TSharedPtr<FJsonObject> ContentObject = Deserialize(Content);
            TArray<TSharedPtr<FJsonValue>> Keywords = ContentObject->GetArrayField("Keywords");
                
            if (Keywords.IsEmpty())
            {
                UE_LOG(LogTemp, Error, TEXT("No keywords found."));
                return;
            }
            TSharedPtr<FJsonValue> FirstKeyword = Keywords[0];

            if (FirstKeyword->Type == EJson::String)
            {
                FString FirstKeywordString = FirstKeyword->AsString();
                UE_LOG(LogTemp, Log, TEXT("Keywords: %s"), *FirstKeywordString);

                OnChatGPTResponse.Broadcast(FirstKeywordString);
            }
        }
        catch(...) {
            UE_LOG(LogTemp, Error, TEXT("fail to find keywords"));
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

TSharedPtr<FJsonObject> AGPTClient::Deserialize(FString String) {
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(String);

    if (FJsonSerializer::Deserialize(Reader, JsonObject)) {
        return JsonObject;
    }
    else {
        return nullptr;
    }
}