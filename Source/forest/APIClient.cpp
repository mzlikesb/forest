// Fill out your copyright notice in the Description page of Project Settings.


#include "APIClient.h"
#include "Http.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Sound/SoundWaveProcedural.h"

UAPIClient::UAPIClient()
{
}

void UAPIClient::SendChatGPTRequest(const FString& SystemPrompt, const FString& UserPrompt)
{
    FString ApiKey = LoadApiKeyFromFile();
    if (ApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("API Key is missing!"));
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

    Request->OnProcessRequestComplete().BindUObject(this, &UAPIClient::OnChatGPTResponseReceived);
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


void UAPIClient::SendWhisperRequest(const FString& AudioFilePath)
{
    FString ApiKey = LoadApiKeyFromFile();
    if (ApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("API Key is missing!"));
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

    Request->OnProcessRequestComplete().BindUObject(this, &UAPIClient::OnWhisperResponseReceived);
    Request->SetURL("https://api.openai.com/v1/audio/transcriptions");
    Request->SetVerb("POST");

    FString Boundary = FString::Printf(TEXT("------------------------%s"), *FGuid::NewGuid().ToString());

    Request->SetHeader("Content-Type", FString::Printf(TEXT("multipart/form-data; boundary=%s"), * Boundary));
    Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *ApiKey));

    FString AbsoluteFilePath = FPaths::ProjectContentDir() + AudioFilePath;

    TArray<uint8> UpFileRawData;
    if (!FFileHelper::LoadFileToArray(UpFileRawData, *AbsoluteFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load audio file"));
        return;
    }

    FString BoundaryStart = FString::Printf(TEXT("--%s\r\n"), *Boundary);
    FString ContentDispositionFile = FString::Printf(TEXT("Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"), *FPaths::GetCleanFilename(AudioFilePath));
    FString ContentType = "Content-Type: audio/wav\r\n\r\n";

    FString BoundaryString = FString::Printf(TEXT("\r\n--%s\r\n"), *Boundary);

    FString ContentDisposition = "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
    FString TranscriptionModel = "whisper-1";

    FString ContentDispositionLanguage = "Content-Disposition: form-data; name=\"language\"\r\n\r\n";
    FString Language = "ko";


    TArray<uint8> Data;
    AppendStringToArray(Data, BoundaryStart);
    AppendStringToArray(Data, ContentDispositionFile);
    AppendStringToArray(Data, ContentType);
    Data.Append(UpFileRawData);
    AppendStringToArray(Data, BoundaryString);
    AppendStringToArray(Data, ContentDisposition);
    AppendStringToArray(Data, TranscriptionModel);
    AppendStringToArray(Data, BoundaryString);
    AppendStringToArray(Data, ContentDispositionLanguage);
    AppendStringToArray(Data, Language);
    AppendStringToArray(Data, BoundaryString);

    Request->SetContent(Data);

    Request->ProcessRequest();
}

void UAPIClient::SendTTSRequest(const FString& Text)
{
    FString ApiKey = LoadApiKeyFromFile();
    if (ApiKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("API Key is missing!"));
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

    Request->OnProcessRequestComplete().BindUObject(this, &UAPIClient::OnTTSResponseReceived);
    Request->SetURL("https://api.openai.com/v1/audio/speech");
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *ApiKey));

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField("model", "tts-1");
    JsonObject->SetStringField("input", Text);
    JsonObject->SetStringField("voice", "shimmer");
    JsonObject->SetStringField("response_format", "wav");

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    Request->SetContentAsString(RequestBody);
    Request->ProcessRequest();
}


void UAPIClient::AppendStringToArray(TArray<uint8>& Array, const FString& String)
{
    FTCHARToUTF8 Converter(*String);
    Array.Append((uint8*)Converter.Get(), Converter.Length());
}

void UAPIClient::OnChatGPTResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        FString ResponseString = Response->GetContentAsString();

        TSharedPtr<FJsonObject> JsonObject = Deserialize(ResponseString);
        TArray<TSharedPtr<FJsonValue>> Choices = JsonObject->GetArrayField((TEXT("choices")));
        if (!Choices.IsEmpty()) {

            TSharedPtr<FJsonObject> Choice = Choices[0]->AsObject();
            TSharedPtr<FJsonObject> Message = Choice->GetObjectField(TEXT("message"));
            FString Content = Message->GetStringField(TEXT("content"));
            if (!Content.IsEmpty()) {
                TSharedPtr<FJsonObject> TextJson = Deserialize(Content);
                FString response = TextJson->GetStringField(TEXT("response"));
                if (!response.IsEmpty()) {
                    OnChatGPTResponse.Broadcast(response, true);
                }
                else {
                    UE_LOG(LogTemp, Error, TEXT("has not 'response' field : %s"), *ResponseString);

                    OnChatGPTResponse.Broadcast(ResponseString, false);
                }
            }
            else {

                OnChatGPTResponse.Broadcast(ResponseString, false);
            }
        }
        else {

            OnChatGPTResponse.Broadcast(ResponseString, false);
        }

    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Request failed"));
    }
}

void UAPIClient::OnWhisperResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        FString ResponseString = Response->GetContentAsString();

        TSharedPtr<FJsonObject> JsonObject = Deserialize(ResponseString);
        FString Text = JsonObject->GetStringField(TEXT("text"));
        
        OnWhisperResponse.Broadcast(Text, true);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Request failed"));
        OnWhisperResponse.Broadcast("", false);
    }
}

void UAPIClient::OnTTSResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        TArray<uint8> ResponseData = Response->GetContent();
        
        if (!ReconstructWavFile(ResponseData))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to reconstruct WAV data"));
            OnTTSResponse.Broadcast(TArray<uint8>(), false);
        }
        OnTTSResponse.Broadcast(ResponseData, true);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Request failed"));
        OnTTSResponse.Broadcast(TArray<uint8>(), false);
    }
}


FString UAPIClient::LoadApiKeyFromFile()
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

TSharedPtr<FJsonObject> UAPIClient::Deserialize(FString String) {
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(String);

    if (FJsonSerializer::Deserialize(Reader, JsonObject)) {
        return JsonObject;
    }
    else {
        return nullptr;
    }
}

USoundWaveProcedural* UAPIClient::LoadSoundWave(const TArray<uint8>& RawFileData)
{
    USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(USoundWaveProcedural::StaticClass());
    FWaveModInfo WaveInfo;
    
    if (WaveInfo.ReadWaveInfo(RawFileData.GetData(), RawFileData.Num()))
    {
        int32 SampleRate = *WaveInfo.pSamplesPerSec;
        int32 NumChannels = *WaveInfo.pChannels;
        int32 DurationDiv = NumChannels * (*WaveInfo.pBitsPerSample / 8) * SampleRate;

        SoundWave->DecompressionType = EDecompressionType::DTYPE_Procedural;

        SoundWave->Duration = (float)WaveInfo.SampleDataSize / DurationDiv;
        SoundWave->SetSampleRate(SampleRate);
        SoundWave->NumChannels = NumChannels;
        
        SoundWave->QueueAudio(RawFileData.GetData(), RawFileData.Num());

        SoundWave->SoundGroup = ESoundGroup::SOUNDGROUP_Default;
        SoundWave->AddToRoot();

        return SoundWave;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Fail to Read WaveInfo from Raw Data"));
        return nullptr;
    }
}

bool UAPIClient::ReconstructWavFile(TArray<uint8>& WavData)
{
    if (WavData.Num() < 44) // 최소 WAV 헤더 크기
        return false;

    // RIFF 청크
    FMemory::Memcpy(WavData.GetData(), "RIFF", 4);
    int32 FileSize = WavData.Num() - 8;
    FMemory::Memcpy(WavData.GetData() + 4, &FileSize, 4);
    FMemory::Memcpy(WavData.GetData() + 8, "WAVE", 4);

    // fmt 하위 청크
    FMemory::Memcpy(WavData.GetData() + 12, "fmt ", 4);
    int32 SubChunk1Size = 16; // PCM의 경우
    FMemory::Memcpy(WavData.GetData() + 16, &SubChunk1Size, 4);

    // 기존 fmt 청크 데이터 유지 (20-35 바이트)

    // data 하위 청크
    FMemory::Memcpy(WavData.GetData() + 36, "data", 4);
    int32 DataSize = WavData.Num() - 44;
    FMemory::Memcpy(WavData.GetData() + 40, &DataSize, 4);

    return true;
}