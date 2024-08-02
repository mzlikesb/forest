/*******************************************************************************
 * Filename    :   OVRLipSyncPlaybackActorComponent.cpp
 * Content     :   OVRLipSync component for Actor objects
 * Created     :   Aug 9th, 2018
 * Copyright   :   Copyright Facebook Technologies, LLC and its affiliates.
 *                 All rights reserved.
 *
 * Licensed under the Oculus Audio SDK License Version 3.3 (the "License");
 * you may not use the Oculus Audio SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.

 * You may obtain a copy of the License at
 *
 * https://developer.oculus.com/licenses/audio-3.3/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus Audio SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "OVRLipSyncPlaybackActorComponent.h"
#include "AudioDevice.h"
#include "AudioDecompress.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "OVRLipSyncContextWrapper.h"

UAudioComponent *UOVRLipSyncPlaybackActorComponent::FindAutoplayAudioComponent() const
{
	TArray<UAudioComponent *> AudioComponents;
	GetOwner()->GetComponents<UAudioComponent>(AudioComponents);

	if (AudioComponents.Num() == 0)
	{
		return nullptr;
	}
	for (auto &AudioComponentCandidate : AudioComponents)
	{
		if (AudioComponentCandidate->bAutoActivate)
			return AudioComponentCandidate;
	}
	return nullptr;
}

// Called when the game starts
void UOVRLipSyncPlaybackActorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Sequence)
	{
		return;
	}
	auto AutoplayComponent = FindAutoplayAudioComponent();
	if (AutoplayComponent)
	{
		Start(AutoplayComponent, NULL);
	}
}

void UOVRLipSyncPlaybackActorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();

	Super::EndPlay(EndPlayReason);
}

void UOVRLipSyncPlaybackActorComponent::OnAudioPlaybackPercent(const UAudioComponent *, const USoundWave *SoundWave,
															   float Percent)
{
	if (!Sequence)
	{
		InitNeutralPose();
		return;
	}
	auto PlayPos = SoundWave->Duration * Percent;
	auto IntPos = static_cast<unsigned>(PlayPos * 100);
	if (IntPos >= Sequence->Num())
	{
		InitNeutralPose();
		return;
	}
	const auto &Frame = (*Sequence)[IntPos];
	LaughterScore = Frame.LaughterScore;
	Visemes = Frame.VisemeScores;
	OnVisemesReady.Broadcast();
}

void UOVRLipSyncPlaybackActorComponent::OnAudioPlaybackFinished(UAudioComponent *) { InitNeutralPose(); }

void UOVRLipSyncPlaybackActorComponent::Start(UAudioComponent *InAudioComponent, UOVRLipSyncFrameSequence *InSequence)
{
	AudioComponent = InAudioComponent;
	if (InSequence)
	{
		Sequence = InSequence;
	}
	PlaybackPercentHandle = AudioComponent->OnAudioPlaybackPercentNative.AddUObject(
		this, &UOVRLipSyncPlaybackActorComponent::OnAudioPlaybackPercent);
	PlaybackFinishedHandle = AudioComponent->OnAudioFinishedNative.AddUObject(
		this, &UOVRLipSyncPlaybackActorComponent::OnAudioPlaybackFinished);
	AudioComponent->Play();
}

void UOVRLipSyncPlaybackActorComponent::Stop()
{
	if (!AudioComponent)
	{
		return;
	}
	AudioComponent->OnAudioPlaybackPercentNative.Remove(PlaybackPercentHandle);
	AudioComponent->OnAudioFinishedNative.Remove(PlaybackFinishedHandle);
	AudioComponent = nullptr;
	InitNeutralPose();
}

void UOVRLipSyncPlaybackActorComponent::SetPlaybackSequence(UOVRLipSyncFrameSequence *InSequence)
{
	Sequence = InSequence;
}


// OVRLipSyncEditorModule에서 가져옴
// Decompresses SoundWave object by initializing RawPCM data
bool UOVRLipSyncPlaybackActorComponent::DecompressSoundWave(USoundWave *SoundWave)
{
	if (SoundWave->RawPCMData)
	{
		return true;
	}
	auto AudioDevice = GEngine->GetMainAudioDevice();
	if (!AudioDevice)
	{
		return false;
	}

	AudioDevice->StopAllSounds(true);
	auto OriginalDecompressionType = SoundWave->DecompressionType;
	SoundWave->DecompressionType = DTYPE_Native;
	if (SoundWave->InitAudioResource(SoundWave->GetRuntimeFormat()))
	{
		USoundWave::FAsyncAudioDecompress Decompress(SoundWave, MONO_PCM_BUFFER_SAMPLES);
		Decompress.StartSynchronousTask();
	}
	SoundWave->DecompressionType = OriginalDecompressionType;

	return true;
}

bool UOVRLipSyncPlaybackActorComponent::OVRLipSyncProcessSoundWave(TArray<uint8> RawFileData, bool UseOfflineModel)
{
	// SoundWave 안거치고 RawFileData로부터 바로 시퀀스 생성하기
	FWaveModInfo WaveInfo;
	uint8 *waveData = const_cast<uint8 *>(RawFileData.GetData());
	if (!WaveInfo.ReadWaveInfo(RawFileData.GetData(), RawFileData.Num()))
	{
		return false;
	}
	
	constexpr auto LipSyncSequenceUpateFrequency = 100;
	constexpr auto LipSyncSequenceDuration = 1.0f / LipSyncSequenceUpateFrequency;

	int32 SampleRate = *WaveInfo.pSamplesPerSec;
	int32 NumChannels = *WaveInfo.pChannels;
	auto PCMDataSize = WaveInfo.SampleDataSize / sizeof(int16_t);
	int16_t *PCMData = reinterpret_cast<int16_t *>(waveData + 44);
	auto ChunkSizeSamples = static_cast<int>(SampleRate * LipSyncSequenceDuration);
	auto ChunkSize = NumChannels * ChunkSizeSamples;

	FString ModelPath = UseOfflineModel ? FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("OVRLipSync"),
														  TEXT("OfflineModel"), TEXT("ovrlipsync_offline_model.pb"))
										: FString();
	
	ProcessedSequence = NewObject<UOVRLipSyncFrameSequence>();
	UOVRLipSyncContextWrapper context(ovrLipSyncContextProvider_Enhanced, SampleRate, 4096, ModelPath);

	float InLaughterScore = 0.0f;
	int32_t FrameDelayInMs = 0;
	TArray<float> NewVisemes;

	for (int offs = 0; offs < PCMDataSize - ChunkSize; offs += ChunkSize)
	{
		int remainingSamples = PCMDataSize - offs;
		if (remainingSamples >= ChunkSize)
		{
			context.ProcessFrame(PCMData + offs, ChunkSizeSamples, NewVisemes, InLaughterScore, FrameDelayInMs, NumChannels > 1);
			ProcessedSequence->Add(NewVisemes, InLaughterScore);
		}
	}

	if (!ProcessedSequence)
	{
		UE_LOG(LogTemp, Error, TEXT("ProcessedSequence is null"));
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("PCMDataSize: %d"), PCMDataSize);
	UE_LOG(LogTemp, Warning, TEXT("Sequence length: %d"), ProcessedSequence->Num());
	UE_LOG(LogTemp, Warning, TEXT("NewVisemes size: %d, LaughterScore: %f"), NewVisemes.Num(), LaughterScore);
	return true;
}
