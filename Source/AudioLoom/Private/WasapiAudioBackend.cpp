// Copyright (c) 2026 AudioLoom Contributors.

#include "WasapiAudioBackend.h"

#if PLATFORM_MAC
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <thread>
#include <atomic>
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include <atomic>
#include "Windows/HideWindowsPlatformTypes.h"

#pragma comment(lib, "ole32.lib")

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

class FWasapiAudioBackendImpl
{
public:
	std::atomic<bool> bStopRequested{false};
	std::atomic<bool> bPlaying{false};
	std::atomic<bool> bLoop{false};
	TArray<float> PCMCopy;
	int32 SourceChannels = 1;
	int32 OutputChannel = 0;  // 0 = all channels
	FString DeviceId;
	bool bExclusive = false;
	int32 BufferSizeMs = 0;  // 0 = driver default (exclusive) or 1s (shared)
	std::thread PlaybackThread;

	void RunPlayback()
	{
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		IMMDeviceEnumerator* pEnumerator = nullptr;
		IMMDevice* pDevice = nullptr;
		IAudioClient* pAudioClient = nullptr;
		IAudioRenderClient* pRenderClient = nullptr;
		WAVEFORMATEX* pwfx = nullptr;

		HRESULT hr = CoCreateInstance(
			__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

		if (FAILED(hr) || !pEnumerator)
		{
			CoUninitialize();
			bPlaying = false;
			return;
		}

		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
		if (DeviceId.Len() > 0)
		{
			IMMDeviceCollection* pDevices = nullptr;
			if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices)))
			{
				UINT Count = 0;
				pDevices->GetCount(&Count);
				for (UINT i = 0; i < Count; ++i)
				{
					IMMDevice* pCandidate = nullptr;
					if (SUCCEEDED(pDevices->Item(i, &pCandidate)))
					{
						LPWSTR pwszId = nullptr;
						if (SUCCEEDED(pCandidate->GetId(&pwszId)))
						{
							if (FString(pwszId) == DeviceId)
							{
								if (pDevice) pDevice->Release();
								pDevice = pCandidate;
								CoTaskMemFree(pwszId);
								break;
							}
							CoTaskMemFree(pwszId);
						}
						pCandidate->Release();
					}
				}
				pDevices->Release();
			}
		}

		if (!pDevice)
		{
			pEnumerator->Release();
			CoUninitialize();
			bPlaying = false;
			return;
		}

		hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
		pEnumerator->Release();
		pDevice->Release();

		if (FAILED(hr) || !pAudioClient)
		{
			CoUninitialize();
			bPlaying = false;
			return;
		}

		hr = pAudioClient->GetMixFormat(&pwfx);
		if (FAILED(hr) || !pwfx)
		{
			pAudioClient->Release();
			CoUninitialize();
			bPlaying = false;
			return;
		}

		// Exclusive mode: use AUDCLNT_SHAREMODE_EXCLUSIVE, fall back to shared if unsupported
		AUDCLNT_SHAREMODE ShareMode = bExclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
		REFERENCE_TIME hnsRequested = bExclusive ? (BufferSizeMs > 0 ? (REFERENCE_TIME)BufferSizeMs * REFTIMES_PER_MILLISEC : 0) : REFTIMES_PER_SEC;
		REFERENCE_TIME hnsPeriodicity = bExclusive ? hnsRequested : 0;
		DWORD StreamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
		hr = pAudioClient->Initialize(ShareMode, StreamFlags, hnsRequested, hnsPeriodicity, pwfx, nullptr);
		if (FAILED(hr) && bExclusive)
		{
			ShareMode = AUDCLNT_SHAREMODE_SHARED;
			hnsRequested = REFTIMES_PER_SEC;
			hnsPeriodicity = 0;
			hr = pAudioClient->Initialize(ShareMode, StreamFlags, hnsRequested, hnsPeriodicity, pwfx, nullptr);
		}
		// Fallback to polling if event-driven not supported
		bool bUseEvent = SUCCEEDED(hr);
		if (!bUseEvent)
		{
			StreamFlags = 0;
			hnsPeriodicity = 0;
			hnsRequested = bExclusive ? (BufferSizeMs > 0 ? (REFERENCE_TIME)BufferSizeMs * REFTIMES_PER_MILLISEC : 0) : REFTIMES_PER_SEC;
			ShareMode = bExclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
			hr = pAudioClient->Initialize(ShareMode, StreamFlags, hnsRequested, 0, pwfx, nullptr);
			if (FAILED(hr) && bExclusive)
			{
				ShareMode = AUDCLNT_SHAREMODE_SHARED;
				hnsRequested = REFTIMES_PER_SEC;
				hr = pAudioClient->Initialize(ShareMode, StreamFlags, hnsRequested, 0, pwfx, nullptr);
			}
		}
		if (FAILED(hr))
		{
			CoTaskMemFree(pwfx);
			pAudioClient->Release();
			CoUninitialize();
			bPlaying = false;
			return;
		}

		HANDLE hEvent = nullptr;
		if (bUseEvent)
		{
			hEvent = CreateEvent(nullptr, 0, 0, nullptr);  // auto-reset, initially non-signaled
			if (hEvent)
			{
				hr = pAudioClient->SetEventHandle(hEvent);
				if (FAILED(hr))
				{
					CloseHandle(hEvent);
					hEvent = nullptr;
					bUseEvent = false;
				}
			}
			else
			{
				bUseEvent = false;
			}
		}

		UINT32 BufferFrameCount = 0;
		pAudioClient->GetBufferSize(&BufferFrameCount);

		hr = pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
		if (FAILED(hr) || !pRenderClient)
		{
			CoTaskMemFree(pwfx);
			pAudioClient->Release();
			CoUninitialize();
			bPlaying = false;
			return;
		}

		const int32 DevChannels = pwfx->nChannels;
		const int32 DevSampleRate = pwfx->nSamplesPerSec;
		const bool bDevFloat = (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
			? (((WAVEFORMATEXTENSIBLE*)pwfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
			: false;
		const int32 BytesPerFrame = (pwfx->wBitsPerSample / 8) * DevChannels;

		int64 ReadOffset = 0;
		const int64 TotalFrames = PCMCopy.Num() / FMath::Max(1, SourceChannels);
		if (TotalFrames <= 0)
		{
			CoTaskMemFree(pwfx);
			pRenderClient->Release();
			pAudioClient->Release();
			CoUninitialize();
			bPlaying = false;
			return;
		}

		auto WriteFrames = [&](BYTE* pData, UINT32 NumFrames, DWORD& OutFlags) -> void
		{
			OutFlags = 0;
			for (UINT32 f = 0; f < NumFrames; ++f)
			{
				int64 SampleOffset = ReadOffset * SourceChannels;
				if (bLoop && TotalFrames > 0)
				{
					ReadOffset = ReadOffset % TotalFrames;
					SampleOffset = ReadOffset * SourceChannels;
				}
				for (int32 ch = 0; ch < DevChannels; ++ch)
				{
					float Sample = 0.0f;
					if (OutputChannel == 0)
					{
						if (ch < SourceChannels && SampleOffset + ch < PCMCopy.Num())
						{
							Sample = PCMCopy[SampleOffset + ch];
						}
					}
					else if (ch + 1 == OutputChannel)
					{
						if (SampleOffset < PCMCopy.Num())
						{
							Sample = PCMCopy[SampleOffset];
						}
					}

					if (bDevFloat)
					{
						*((float*)pData) = Sample;
						pData += sizeof(float);
					}
					else
					{
						int16 Quantized = (int16)FMath::Clamp(Sample * 32767.0f, -32768.0f, 32767.0f);
						*((int16*)pData) = Quantized;
						pData += sizeof(int16);
					}
				}
				++ReadOffset;
				if (!bLoop && ReadOffset >= TotalFrames && OutFlags == 0)
				{
					OutFlags = AUDCLNT_BUFFERFLAGS_SILENT;
				}
			}
		};

		BYTE* pData = nullptr;
		UINT32 InitialFrames = bLoop ? BufferFrameCount : FMath::Min((UINT32)BufferFrameCount, (UINT32)FMath::Max(0ll, TotalFrames - ReadOffset));
		if (InitialFrames > 0)
		{
			hr = pRenderClient->GetBuffer(InitialFrames, &pData);
			if (SUCCEEDED(hr) && pData)
			{
				DWORD Flags = 0;
				WriteFrames(pData, InitialFrames, Flags);
				pRenderClient->ReleaseBuffer(InitialFrames, Flags);
			}
		}

		double hnsActualDuration = (double)REFTIMES_PER_SEC * BufferFrameCount / DevSampleRate;
		pAudioClient->Start();

		while (!bStopRequested)
		{
			if (bUseEvent && hEvent)
			{
				DWORD WaitResult = WaitForSingleObject(hEvent, 100);
				if (WaitResult != WAIT_OBJECT_0 && WaitResult != WAIT_TIMEOUT)
				{
					break;
				}
			}
			else
			{
				Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));
			}

			UINT32 numFramesPadding = 0;
			pAudioClient->GetCurrentPadding(&numFramesPadding);
			UINT32 numFramesAvailable = BufferFrameCount - numFramesPadding;

			if (numFramesAvailable == 0) continue;

			int64 FramesRemaining = bLoop ? numFramesAvailable : FMath::Max(0ll, TotalFrames - ReadOffset);
			UINT32 FramesToWrite = FMath::Min(numFramesAvailable, (UINT32)FramesRemaining);
			if (FramesToWrite == 0 && !bLoop)
			{
				hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
				if (SUCCEEDED(hr) && pData)
				{
					pRenderClient->ReleaseBuffer(numFramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT);
				}
				if (ReadOffset >= TotalFrames) break;
				continue;
			}
			if (bLoop && TotalFrames > 0)
			{
				FramesToWrite = numFramesAvailable;
			}

			hr = pRenderClient->GetBuffer(FramesToWrite, &pData);
			if (FAILED(hr) || !pData) continue;

			DWORD Flags = 0;
			WriteFrames(pData, FramesToWrite, Flags);
			pRenderClient->ReleaseBuffer(FramesToWrite, Flags);

			if (!bLoop && Flags == AUDCLNT_BUFFERFLAGS_SILENT && ReadOffset >= TotalFrames)
			{
				break;
			}
		}

		if (bUseEvent && hEvent)
		{
			WaitForSingleObject(hEvent, 100);
			CloseHandle(hEvent);
		}
		pAudioClient->Stop();
		CoTaskMemFree(pwfx);
		pRenderClient->Release();
		pAudioClient->Release();
		CoUninitialize();
		bPlaying = false;
	}
};
#elif PLATFORM_MAC
struct FCoreAudioPlaybackContext
{
	const float* PCM = nullptr;
	int64 TotalFrames = 0;
	int32 SourceChannels = 1;
	int32 OutputChannel = 0;
	bool bLoop = false;
	std::atomic<int64> ReadOffset{0};
	std::atomic<bool> bStopRequested{false};
	int32 DevChannels = 2;
};

static OSStatus MacIOProc(AudioObjectID inDevice, const AudioTimeStamp* inNow,
	const AudioBufferList* inInputData, const AudioTimeStamp* inInputTime,
	AudioBufferList* outOutputData, const AudioTimeStamp* inOutputTime, void* inClientData)
{
	FCoreAudioPlaybackContext* Ctx = (FCoreAudioPlaybackContext*)inClientData;
	if (!Ctx || !Ctx->PCM || Ctx->TotalFrames <= 0) return noErr;

	const int64 TotalFrames = Ctx->TotalFrames;
	const int32 SrcCh = FMath::Max(1, Ctx->SourceChannels);
	const int32 OutCh = Ctx->OutputChannel;
	const bool bLoop = Ctx->bLoop;
	const int32 DevCh = FMath::Max(1, Ctx->DevChannels);

	// CoreAudio uses non-interleaved layout: each buffer = one output channel.
	// Iterate frame-by-frame across all channels to keep ReadOffset in sync.
	UInt32 NumFrames = 0;
	if (outOutputData->mNumberBuffers > 0 && outOutputData->mBuffers[0].mData)
	{
		const UInt32 chPerBuf = outOutputData->mBuffers[0].mNumberChannels;
		NumFrames = outOutputData->mBuffers[0].mDataByteSize / (sizeof(float) * FMath::Max(1u, chPerBuf));
	}
	if (NumFrames == 0) return noErr;

	for (UInt32 f = 0; f < NumFrames; ++f)
	{
		if (Ctx->bStopRequested) break;

		int64 R = Ctx->ReadOffset.load(std::memory_order_relaxed);
		if (bLoop && TotalFrames > 0)
		{
			R = R % TotalFrames;
		}
		const int64 SampleOffset = R * SrcCh;

		UInt32 channelOffset = 0;
		for (UInt32 bufIdx = 0; bufIdx < outOutputData->mNumberBuffers; ++bufIdx)
		{
			AudioBuffer& buf = outOutputData->mBuffers[bufIdx];
			if (!buf.mData) continue;
			const UInt32 chInBuf = FMath::Max(1u, buf.mNumberChannels);
			float* pData = (float*)buf.mData;

			for (UInt32 subCh = 0; subCh < chInBuf; ++subCh)
			{
				const int32 logicalCh = static_cast<int32>(channelOffset + subCh);
				float Sample = 0.0f;
				if (OutCh == 0)
				{
					if (logicalCh < SrcCh && SampleOffset + logicalCh < Ctx->TotalFrames * SrcCh)
					{
						Sample = Ctx->PCM[SampleOffset + logicalCh];
					}
				}
				else if (logicalCh + 1 == OutCh)
				{
					if (SampleOffset < Ctx->TotalFrames * SrcCh)
					{
						Sample = Ctx->PCM[SampleOffset];
					}
				}
				// Non-interleaved: one sample per channel; buffer holds consecutive frames for this channel
				pData[f * chInBuf + subCh] = Sample;
			}
			channelOffset += chInBuf;
		}
		Ctx->ReadOffset.store(R + 1, std::memory_order_relaxed);
	}
	return noErr;
}

class FWasapiAudioBackendImpl
{
public:
	std::atomic<bool> bStopRequested{false};
	std::atomic<bool> bPlaying{false};
	std::atomic<bool> bLoop{false};
	TArray<float> PCMCopy;
	int32 SourceChannels = 1;
	int32 OutputChannel = 0;
	FString DeviceId;
	bool bExclusive = false;     // Unused on Mac; kept for Start() signature compatibility
	int32 BufferSizeMs = 0;      // Unused on Mac; kept for Start() signature compatibility
	std::thread PlaybackThread;

	void RunPlayback()
	{
		AudioDeviceID DevID = kAudioObjectUnknown;
		AudioObjectPropertyAddress Addr = {
			kAudioHardwarePropertyDefaultOutputDevice,
			kAudioObjectPropertyScopeGlobal,
			kAudioObjectPropertyElementMain
		};
		UInt32 DataSize = sizeof(AudioDeviceID);
		OSStatus Err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &Addr, 0, nullptr, &DataSize, &DevID);
		if (Err != noErr || DevID == kAudioObjectUnknown)
		{
			bPlaying = false;
			return;
		}

		if (DeviceId.Len() > 0)
		{
			Addr.mSelector = kAudioHardwarePropertyDevices;
			DataSize = 0;
			Err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &Addr, 0, nullptr, &DataSize);
			if (Err == noErr && DataSize > 0)
			{
				const UInt32 N = DataSize / sizeof(AudioDeviceID);
				TArray<AudioDeviceID> IDs;
				IDs.SetNum(N);
				Err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &Addr, 0, nullptr, &DataSize, IDs.GetData());
				if (Err == noErr)
				{
					for (UInt32 i = 0; i < N; ++i)
					{
						CFStringRef UIDRef = nullptr;
						Addr.mSelector = kAudioDevicePropertyDeviceUID;
						Addr.mScope = kAudioObjectPropertyScopeGlobal;
						UInt32 Size = sizeof(CFStringRef);
						if (AudioObjectGetPropertyData(IDs[i], &Addr, 0, nullptr, &Size, &UIDRef) == noErr && UIDRef)
						{
							CFIndex Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(UIDRef), kCFStringEncodingUTF8) + 1;
							TArray<char> Tmp;
							Tmp.SetNum(static_cast<int32>(Len));
							if (CFStringGetCString(UIDRef, Tmp.GetData(), Len, kCFStringEncodingUTF8))
							{
								if (FString(UTF8_TO_TCHAR(Tmp.GetData())) == DeviceId)
								{
									DevID = IDs[i];
									CFRelease(UIDRef);
									break;
								}
							}
							CFRelease(UIDRef);
						}
					}
				}
			}
		}

		Addr.mSelector = kAudioDevicePropertyStreamConfiguration;
		Addr.mScope = kAudioDevicePropertyScopeOutput;
		UInt32 StreamSize = 0;
		Err = AudioObjectGetPropertyDataSize(DevID, &Addr, 0, nullptr, &StreamSize);
		if (Err != noErr || StreamSize == 0) { bPlaying = false; return; }

		TArray<uint8> Buf;
		Buf.SetNum(StreamSize);
		Err = AudioObjectGetPropertyData(DevID, &Addr, 0, nullptr, &StreamSize, Buf.GetData());
		if (Err != noErr) { bPlaying = false; return; }

		AudioBufferList* pList = (AudioBufferList*)Buf.GetData();
		UInt32 DevChannels = 0;
		for (UInt32 j = 0; j < pList->mNumberBuffers; ++j) { DevChannels += pList->mBuffers[j].mNumberChannels; }
		if (DevChannels == 0) { bPlaying = false; return; }

		const int64 TotalFrames = PCMCopy.Num() / FMath::Max(1, SourceChannels);
		if (TotalFrames <= 0) { bPlaying = false; return; }

		FCoreAudioPlaybackContext Ctx;
		Ctx.PCM = PCMCopy.GetData();
		Ctx.TotalFrames = TotalFrames;
		Ctx.SourceChannels = FMath::Max(1, SourceChannels);
		Ctx.OutputChannel = OutputChannel;
		Ctx.bLoop = bLoop;
		Ctx.ReadOffset = 0;
		Ctx.bStopRequested = false;
		Ctx.DevChannels = static_cast<int32>(DevChannels);

		AudioDeviceIOProcID ProcID = nullptr;
		Err = AudioDeviceCreateIOProcID(DevID, MacIOProc, &Ctx, &ProcID);
		if (Err != noErr || !ProcID) { bPlaying = false; return; }

		Err = AudioDeviceStart(DevID, ProcID);
		if (Err != noErr) { AudioDeviceDestroyIOProcID(DevID, ProcID); bPlaying = false; return; }

		while (!bStopRequested)
		{
			if (!bLoop && Ctx.ReadOffset.load(std::memory_order_relaxed) >= TotalFrames) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		Ctx.bStopRequested = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		AudioDeviceStop(DevID, ProcID);
		AudioDeviceDestroyIOProcID(DevID, ProcID);
		bPlaying = false;
	}
};
#endif

FWasapiAudioBackend::FWasapiAudioBackend()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	Impl = MakeUnique<FWasapiAudioBackendImpl>();
#endif
}

FWasapiAudioBackend::~FWasapiAudioBackend()
{
	Stop();
}

void FWasapiAudioBackend::Start(const FString& InDeviceId, const TArray<float>& PCMData, int32 SourceChannels, int32 OutChannel, bool bInLoop, bool bExclusive, int32 InBufferSizeMs)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!Impl) return;
	Stop();
	Impl->bStopRequested = false;
	Impl->bPlaying = true;
	Impl->bLoop = bInLoop;
	Impl->bExclusive = bExclusive;
	Impl->BufferSizeMs = FMath::Max(0, InBufferSizeMs);
	Impl->PCMCopy = PCMData;
	Impl->SourceChannels = FMath::Max(1, SourceChannels);
	Impl->OutputChannel = OutChannel;
	Impl->DeviceId = InDeviceId;
	Impl->PlaybackThread = std::thread([this]() { Impl->RunPlayback(); });
#endif
}

void FWasapiAudioBackend::Stop()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!Impl) return;
	Impl->bStopRequested = true;
	if (Impl->PlaybackThread.joinable())
	{
		Impl->PlaybackThread.join();
	}
#endif
}

bool FWasapiAudioBackend::IsPlaying() const
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	return Impl && Impl->bPlaying;
#else
	return false;
#endif
}
