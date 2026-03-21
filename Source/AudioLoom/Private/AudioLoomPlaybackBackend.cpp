// Copyright (c) 2026 AudioLoom Contributors.

/**
 * @file AudioLoomPlaybackBackend.cpp
 * @brief Platform audio output: **Windows** WASAPI render thread; **macOS** Core Audio IO callback.
 *
 * **Windows (`RunPlayback`)**: COM init, resolve device (default or match `DeviceId`), `IAudioClient`
 * shared/exclusive, optional event-driven buffer fill, `WriteFrames` maps source channels to device
 * (0 = mirror all source ch; N = route mono from first source channel to logical output N).
 *
 * **macOS (`RunPlayback` + `MacIOProc`)**: pick device by UID string, query stream layout,
 * non-interleaved float output per buffer; `ReadOffset` advances per frame across channels.
 *
 * **Looping**: `bLoop` wraps read offset; non-loop exits when buffer is flagged silent/end.
 *
 * To port to Linux (e.g. ALSA/Pulse), add a third `FAudioLoomPlaybackBackendImpl` variant and keep
 * `FAudioLoomPlaybackBackend` API stable for `UAudioLoomComponent`.
 */

#include "AudioLoomPlaybackBackend.h"

#include <atomic>

#if PLATFORM_MAC
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <thread>
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include "Windows/HideWindowsPlatformTypes.h"

#pragma comment(lib, "ole32.lib")

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

// ---------- Windows: IMMDevice → IAudioClient → IAudioRenderClient (see RunPlayback) ----------

class FAudioLoomPlaybackBackendImpl
{
public:
	std::atomic<bool> bStopRequested{false};
	std::atomic<bool> bPlaying{false};
	std::atomic<bool> bLoop{false};
	/** OS-reported / buffer-derived output latency (ms); written after stream init, cleared when thread exits. */
	std::atomic<float> OutputLatencyMs{0.f};
	TArray<float> PCMCopy;
	int32 SourceChannels = 1;
	int32 OutputChannel = 0;  // 0 = all channels
	FString DeviceId;
	bool bExclusive = false;
	int32 BufferSizeMs = 0;  // 0 = driver default (exclusive) or 1s (shared)
	std::thread PlaybackThread;

	void RunPlayback()
	{
		OutputLatencyMs.store(0.f, std::memory_order_relaxed);
		CoInitializeEx(nullptr, COINIT_MULTITHREADED); // this thread owns COM for device/audio calls

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

		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice); // starting point
		if (DeviceId.Len() > 0)
		{
			// User picked a specific endpoint — search collection by string ID from enumeration UI
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
		pDevice->Release(); // IAudioClient holds what it needs; device interface can go

		if (FAILED(hr) || !pAudioClient)
		{
			CoUninitialize();
			bPlaying = false;
			return;
		}

		hr = pAudioClient->GetMixFormat(&pwfx); // shared-mode mix format; we fill buffers to match this layout
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
		{
			const int32 SR = FMath::Max(1, (int32)pwfx->nSamplesPerSec);
			const float BufferDurationMs = BufferFrameCount * 1000.f / float(SR);
			REFERENCE_TIME hnsLatency = 0;
			float LatMs = BufferDurationMs;
			if (SUCCEEDED(pAudioClient->GetStreamLatency(&hnsLatency)))
			{
				LatMs = static_cast<float>(hnsLatency / 10000.0); // REFERENCE_TIME is 100-ns units
			}
			OutputLatencyMs.store(FMath::Max(0.f, LatMs), std::memory_order_relaxed);
		}

		hr = pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
		if (FAILED(hr) || !pRenderClient)
		{
			OutputLatencyMs.store(0.f, std::memory_order_relaxed);
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

		int64 ReadOffset = 0; // next source frame index (advances once per output frame written)
		const int64 TotalFrames = PCMCopy.Num() / FMath::Max(1, SourceChannels);
		if (TotalFrames <= 0)
		{
			OutputLatencyMs.store(0.f, std::memory_order_relaxed);
			CoTaskMemFree(pwfx);
			pRenderClient->Release();
			pAudioClient->Release();
			CoUninitialize();
			bPlaying = false;
			return;
		}

		// Pack one device buffer: interleaved output (ch0,ch1,…) per frame in *device* order
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
						// Route: source ch N → device ch N (up to min of counts)
						if (ch < SourceChannels && SampleOffset + ch < PCMCopy.Num())
						{
							Sample = PCMCopy[SampleOffset + ch];
						}
					}
					else if (ch + 1 == OutputChannel)
					{
						// Single channel: duplicate first source channel only to that output
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
					OutFlags = AUDCLNT_BUFFERFLAGS_SILENT; // tell driver rest of buffer is silence
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

		// Pull model: wait for driver (event) or sleep, then ask how many frames are free to fill
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
			pAudioClient->GetCurrentPadding(&numFramesPadding); // frames queued ahead of write cursor
			UINT32 numFramesAvailable = BufferFrameCount - numFramesPadding;

			if (numFramesAvailable == 0) continue; // buffer full, wait for next wake

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
		OutputLatencyMs.store(0.f, std::memory_order_relaxed);
		bPlaying = false;
	}
};

#elif PLATFORM_MAC
// ---------- macOS: AudioDeviceIOProc + per-frame channel mapping ----------

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

	// One output frame = one advance of ReadOffset; fill every logical output channel for this slice
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

/** macOS impl: same member layout as Windows for `FAudioLoomPlaybackBackend::Start` (exclusive/buffer ignored). */
class FAudioLoomPlaybackBackendImpl
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
	std::atomic<float> OutputLatencyMs{0.f};
	std::thread PlaybackThread;

	void RunPlayback()
	{
		OutputLatencyMs.store(0.f, std::memory_order_relaxed);
		AudioDeviceID DevID = kAudioObjectUnknown;
		// Default route: system’s current output device (same idea as Windows default endpoint)
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
			// Match saved UID string from enumeration (see AudioOutputDeviceEnumerator Mac path)
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
		// Variable-size AudioBufferList describing physical buffers / channel groups
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

		{
			AudioObjectPropertyAddress BufAddr = {
				kAudioDevicePropertyBufferFrameSize,
				kAudioDevicePropertyScopeOutput,
				kAudioObjectPropertyElementMain
			};
			UInt32 BufFrames = 512;
			UInt32 BufSz = sizeof(BufFrames);
			if (AudioObjectGetPropertyData(DevID, &BufAddr, 0, nullptr, &BufSz, &BufFrames) != noErr)
			{
				BufFrames = 512;
			}
			AudioObjectPropertyAddress RateAddr = {
				kAudioDevicePropertyNominalSampleRate,
				kAudioObjectPropertyScopeGlobal,
				kAudioObjectPropertyElementMain
			};
			Float64 SampleRate = 48000.0;
			UInt32 RateSz = sizeof(SampleRate);
			if (AudioObjectGetPropertyData(DevID, &RateAddr, 0, nullptr, &RateSz, &SampleRate) != noErr)
			{
				SampleRate = 48000.0;
			}
			const float LatMs = static_cast<float>((double)BufFrames / FMath::Max(1.0, SampleRate) * 1000.0);
			OutputLatencyMs.store(FMath::Max(0.f, LatMs), std::memory_order_relaxed);
		}

		// Mac: real work is in MacIOProc; this thread only waits for stop or end-of-file
		while (!bStopRequested)
		{
			if (!bLoop && Ctx.ReadOffset.load(std::memory_order_relaxed) >= TotalFrames) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		Ctx.bStopRequested = true; // signal callback to stop advancing
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // let last buffers drain
		AudioDeviceStop(DevID, ProcID);
		AudioDeviceDestroyIOProcID(DevID, ProcID);
		OutputLatencyMs.store(0.f, std::memory_order_relaxed);
		bPlaying = false;
	}
};
#endif

// ---------- Public PIMPL wrappers ----------

FAudioLoomPlaybackBackend::FAudioLoomPlaybackBackend()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	Impl = MakeUnique<FAudioLoomPlaybackBackendImpl>();
#endif
}

FAudioLoomPlaybackBackend::~FAudioLoomPlaybackBackend()
{
	Stop();
}

void FAudioLoomPlaybackBackend::Start(const FString& InDeviceId, const TArray<float>& PCMData, int32 SourceChannels, int32 OutChannel, bool bInLoop, bool bExclusive, int32 InBufferSizeMs)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!Impl) return;
	Stop(); // join previous thread if any — only one stream per backend instance
	Impl->bStopRequested = false;
	Impl->bPlaying = true; // set before thread start so IsPlaying() is true immediately
	Impl->bLoop = bInLoop;
	Impl->bExclusive = bExclusive;
	Impl->BufferSizeMs = FMath::Max(0, InBufferSizeMs);
	Impl->PCMCopy = PCMData;
	Impl->SourceChannels = FMath::Max(1, SourceChannels);
	Impl->OutputChannel = OutChannel;
	Impl->DeviceId = InDeviceId;
	Impl->PlaybackThread = std::thread([this]() { Impl->RunPlayback(); }); // detached lifetime until Stop()
#endif
}

void FAudioLoomPlaybackBackend::Stop()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!Impl) return;
	Impl->bStopRequested = true; // RunPlayback loops observe this
	if (Impl->PlaybackThread.joinable())
	{
		Impl->PlaybackThread.join(); // blocks until thread exits and sets bPlaying false
	}
	Impl->OutputLatencyMs.store(0.f, std::memory_order_relaxed);
#endif
}

bool FAudioLoomPlaybackBackend::IsPlaying() const
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	return Impl && Impl->bPlaying;
#else
	return false;
#endif
}

float FAudioLoomPlaybackBackend::GetOutputLatencyMs() const
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (!Impl) return 0.f;
	return Impl->OutputLatencyMs.load(std::memory_order_relaxed);
#else
	return 0.f;
#endif
}
