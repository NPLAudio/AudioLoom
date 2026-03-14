// Copyright (c) 2026 AudioLoom Contributors.

#include "WasapiDeviceEnumerator.h"

#if PLATFORM_MAC
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <propsys.h>
#include "Windows/HideWindowsPlatformTypes.h"

// PKEY_AudioEngine_DeviceFormat - device native format (channel count). Not in all SDK link libs.
static const PROPERTYKEY PKEY_AudioEngine_DeviceFormat_Local = {
	{ 0xf19f064d, 0x082c, 0x4e27, { 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c } }, 0
};

/** Probe max channel count via IsFormatSupported in exclusive mode. */
static int32 GetDeviceMaxChannels(IMMDevice* pDevice)
{
	IAudioClient* pClient = nullptr;
	HRESULT hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pClient);
	if (FAILED(hr) || !pClient) return 2;

	static const int32 ChannelMasks[] = { 0, KSAUDIO_SPEAKER_STEREO, KSAUDIO_SPEAKER_QUAD, KSAUDIO_SPEAKER_5POINT1, KSAUDIO_SPEAKER_7POINT1_SURROUND };
	static const int32 ChannelCounts[] = { 1, 2, 4, 6, 8 };
	int32 MaxChannels = 2;

	for (int32 probeCh = 8; probeCh <= 32; probeCh += 2)
	{
		WAVEFORMATEXTENSIBLE wfx = {};
		wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		wfx.Format.nChannels = static_cast<WORD>(probeCh);
		wfx.Format.nSamplesPerSec = 48000;
		wfx.Format.wBitsPerSample = 32;
		wfx.Format.nBlockAlign = wfx.Format.nChannels * 4;
		wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
		wfx.Format.cbSize = 22;
		wfx.Samples.wValidBitsPerSample = 32;
		wfx.dwChannelMask = KSAUDIO_SPEAKER_DIRECTOUT;
		wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

		WAVEFORMATEX* pClosest = nullptr;
		hr = pClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wfx, &pClosest);
		if (pClosest) CoTaskMemFree(pClosest);
		if (hr == S_OK)
		{
			MaxChannels = probeCh;
		}
		else
		{
			break;
		}
	}

	if (MaxChannels == 2)
	{
		for (int32 i = 4; i >= 0; --i)
		{
			WAVEFORMATEXTENSIBLE wfx = {};
			wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			wfx.Format.nChannels = static_cast<WORD>(ChannelCounts[i]);
			wfx.Format.nSamplesPerSec = 48000;
			wfx.Format.wBitsPerSample = 32;
			wfx.Format.nBlockAlign = wfx.Format.nChannels * 4;
			wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
			wfx.Format.cbSize = 22;
			wfx.Samples.wValidBitsPerSample = 32;
			wfx.dwChannelMask = (ChannelCounts[i] == 1) ? KSAUDIO_SPEAKER_MONO : ChannelMasks[i];
			wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

			WAVEFORMATEX* pClosest = nullptr;
			hr = pClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wfx, &pClosest);
			if (pClosest) CoTaskMemFree(pClosest);
			if (hr == S_OK)
			{
				MaxChannels = ChannelCounts[i];
				break;
			}
		}
	}

	// Some drivers (e.g. virtual cables) don't support exclusive; try shared-mode probe for 4/6/8
	if (MaxChannels == 2)
	{
		for (int32 probeCh : { 8, 6, 4 })
		{
			WAVEFORMATEXTENSIBLE wfx = {};
			wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			wfx.Format.nChannels = static_cast<WORD>(probeCh);
			wfx.Format.nSamplesPerSec = 48000;
			wfx.Format.wBitsPerSample = 32;
			wfx.Format.nBlockAlign = wfx.Format.nChannels * 4;
			wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
			wfx.Format.cbSize = 22;
			wfx.Samples.wValidBitsPerSample = 32;
			wfx.dwChannelMask = (probeCh == 8) ? KSAUDIO_SPEAKER_7POINT1_SURROUND : (probeCh == 6) ? KSAUDIO_SPEAKER_5POINT1 : KSAUDIO_SPEAKER_QUAD;
			wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

			WAVEFORMATEX* pClosest = nullptr;
			hr = pClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&wfx, &pClosest);
			if (pClosest) CoTaskMemFree(pClosest);
			if (hr == S_OK)
			{
				MaxChannels = probeCh;
				break;
			}
		}
	}

	pClient->Release();
	return MaxChannels;
}

#pragma comment(lib, "ole32.lib")
#endif

TArray<FWasapiDeviceInfo> FWasapiDeviceEnumerator::GetOutputDevices()
{
	TArray<FWasapiDeviceInfo> Result;

#if PLATFORM_WINDOWS
	IMMDeviceEnumerator* pEnumerator = nullptr;
	IMMDeviceCollection* pDevices = nullptr;

	HRESULT hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		nullptr,
		CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void**)&pEnumerator);

	if (FAILED(hr) || !pEnumerator)
	{
		return Result;
	}

	hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
	if (FAILED(hr) || !pDevices)
	{
		pEnumerator->Release();
		return Result;
	}

	UINT Count = 0;
	hr = pDevices->GetCount(&Count);
	if (FAILED(hr))
	{
		pDevices->Release();
		pEnumerator->Release();
		return Result;
	}

	Result.Reserve(Count);

	for (UINT i = 0; i < Count; ++i)
	{
		IMMDevice* pDevice = nullptr;
		hr = pDevices->Item(i, &pDevice);
		if (FAILED(hr) || !pDevice)
		{
			continue;
		}

		LPWSTR pwszId = nullptr;
		hr = pDevice->GetId(&pwszId);
		if (FAILED(hr) || !pwszId)
		{
			pDevice->Release();
			continue;
		}

		FWasapiDeviceInfo Info;
		Info.DeviceId = pwszId;
		CoTaskMemFree(pwszId);

		IPropertyStore* pProps = nullptr;
		hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
		if (SUCCEEDED(hr) && pProps)
		{
			PROPVARIANT varName;
			PropVariantInit(&varName);
			hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
			if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR && varName.pwszVal)
			{
				Info.FriendlyName = varName.pwszVal;
			}
			PropVariantClear(&varName);

			// PKEY_AudioEngine_DeviceFormat - may still report stereo; probe with IsFormatSupported for actual max
			int32 FormatChannels = 2;
			PROPVARIANT varFormat;
			PropVariantInit(&varFormat);
			if (SUCCEEDED(pProps->GetValue(PKEY_AudioEngine_DeviceFormat_Local, &varFormat)) &&
			    varFormat.vt == VT_BLOB && varFormat.blob.pBlobData && varFormat.blob.cbSize >= sizeof(WAVEFORMATEX))
			{
				WAVEFORMATEX* pwfx = (WAVEFORMATEX*)varFormat.blob.pBlobData;
				FormatChannels = pwfx->nChannels;
				Info.SampleRate = pwfx->nSamplesPerSec;
				Info.BytesPerSample = pwfx->wBitsPerSample / 8;
				if (varFormat.blob.cbSize >= sizeof(WAVEFORMATEXTENSIBLE) && pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
				{
					WAVEFORMATEXTENSIBLE* pwfex = (WAVEFORMATEXTENSIBLE*)pwfx;
					Info.BytesPerSample = pwfex->Format.wBitsPerSample / 8;
				}
				Info.bIsValid = true;
			}
			PropVariantClear(&varFormat);
			pProps->Release();

			// Probe exclusive-mode max channels; use larger of format report vs probe
			int32 ProbeChannels = GetDeviceMaxChannels(pDevice);
			Info.NumChannels = FMath::Max(FormatChannels, ProbeChannels);
		}

		if (!Info.bIsValid)
		{
			int32 MixChannels = 2;
			IAudioClient* pAudioClient = nullptr;
			hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
			if (SUCCEEDED(hr) && pAudioClient)
			{
				WAVEFORMATEX* pwfx = nullptr;
				hr = pAudioClient->GetMixFormat(&pwfx);
				if (SUCCEEDED(hr) && pwfx)
				{
					MixChannels = pwfx->nChannels;
					Info.SampleRate = pwfx->nSamplesPerSec;
					Info.BytesPerSample = pwfx->wBitsPerSample / 8;
					if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
					{
						WAVEFORMATEXTENSIBLE* pwfex = (WAVEFORMATEXTENSIBLE*)pwfx;
						Info.BytesPerSample = pwfex->Format.wBitsPerSample / 8;
					}
					Info.bIsValid = true;
					CoTaskMemFree(pwfx);
				}
				pAudioClient->Release();
			}
			int32 ProbeCh = GetDeviceMaxChannels(pDevice);
			Info.NumChannels = FMath::Max(MixChannels, ProbeCh);
		}

		pDevice->Release();
		Result.Add(Info);
	}

	pDevices->Release();
	pEnumerator->Release();

#elif PLATFORM_MAC
	AudioObjectPropertyAddress Addr = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};
	UInt32 DataSize = 0;
	OSStatus Err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &Addr, 0, nullptr, &DataSize);
	if (Err != noErr || DataSize == 0) return Result;

	const UInt32 NumDevices = DataSize / sizeof(AudioDeviceID);
	TArray<AudioDeviceID> DeviceIDs;
	DeviceIDs.SetNum(NumDevices);
	Err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &Addr, 0, nullptr, &DataSize, DeviceIDs.GetData());
	if (Err != noErr) return Result;

	for (UInt32 i = 0; i < NumDevices; ++i)
	{
		AudioDeviceID DevID = DeviceIDs[i];
		Addr.mSelector = kAudioDevicePropertyStreamConfiguration;
		Addr.mScope = kAudioDevicePropertyScopeOutput;
		UInt32 StreamSize = 0;
		Err = AudioObjectGetPropertyDataSize(DevID, &Addr, 0, nullptr, &StreamSize);
		if (Err != noErr || StreamSize == 0) continue;

		TArray<uint8> Buf;
		Buf.SetNum(StreamSize);
		Err = AudioObjectGetPropertyData(DevID, &Addr, 0, nullptr, &StreamSize, Buf.GetData());
		if (Err != noErr) continue;

		AudioBufferList* pList = (AudioBufferList*)Buf.GetData();
		UInt32 OutChannels = 0;
		for (UInt32 j = 0; j < pList->mNumberBuffers; ++j)
		{
			OutChannels += pList->mBuffers[j].mNumberChannels;
		}
		if (OutChannels == 0) continue;

		FWasapiDeviceInfo Info;
		Info.NumChannels = static_cast<int32>(OutChannels);
		Info.SampleRate = 48000;
		Info.BytesPerSample = 4;

		Addr.mSelector = kAudioDevicePropertyDeviceUID;
		Addr.mScope = kAudioObjectPropertyScopeGlobal;
		CFStringRef UIDRef = nullptr;
		DataSize = sizeof(CFStringRef);
		Err = AudioObjectGetPropertyData(DevID, &Addr, 0, nullptr, &DataSize, &UIDRef);
		if (Err == noErr && UIDRef)
		{
			CFIndex Len = CFStringGetLength(UIDRef);
			Len = CFStringGetMaximumSizeForEncoding(Len, kCFStringEncodingUTF8) + 1;
			TArray<char> Tmp;
			Tmp.SetNum(static_cast<int32>(Len));
			if (CFStringGetCString(UIDRef, Tmp.GetData(), Len, kCFStringEncodingUTF8))
			{
				Info.DeviceId = UTF8_TO_TCHAR(Tmp.GetData());
			}
			CFRelease(UIDRef);
		}
		if (Info.DeviceId.IsEmpty())
		{
			Info.DeviceId = FString::Printf(TEXT("%u"), static_cast<unsigned>(DevID));
		}

		Addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
		CFStringRef NameRef = nullptr;
		DataSize = sizeof(CFStringRef);
		Err = AudioObjectGetPropertyData(DevID, &Addr, 0, nullptr, &DataSize, &NameRef);
		if (Err == noErr && NameRef)
		{
			CFIndex Len = CFStringGetLength(NameRef);
			Len = CFStringGetMaximumSizeForEncoding(Len, kCFStringEncodingUTF8) + 1;
			TArray<char> Tmp;
			Tmp.SetNum(static_cast<int32>(Len));
			if (CFStringGetCString(NameRef, Tmp.GetData(), Len, kCFStringEncodingUTF8))
			{
				Info.FriendlyName = UTF8_TO_TCHAR(Tmp.GetData());
			}
			CFRelease(NameRef);
		}
		if (Info.FriendlyName.IsEmpty())
		{
			Info.FriendlyName = Info.DeviceId;
		}
		Info.bIsValid = true;
		Result.Add(Info);
	}
#endif

	return Result;
}

FWasapiDeviceInfo FWasapiDeviceEnumerator::GetDeviceById(const FString& DeviceId)
{
	if (DeviceId.IsEmpty())
	{
		return GetDefaultOutputDevice();
	}
	TArray<FWasapiDeviceInfo> Devices = GetOutputDevices();
	for (const FWasapiDeviceInfo& D : Devices)
	{
		if (D.DeviceId == DeviceId)
		{
			return D;
		}
	}
	return FWasapiDeviceInfo();
}

FWasapiDeviceInfo FWasapiDeviceEnumerator::GetDefaultOutputDevice()
{
	FWasapiDeviceInfo Result;
#if PLATFORM_WINDOWS
	IMMDeviceEnumerator* pEnumerator = nullptr;
	IMMDevice* pDevice = nullptr;

	HRESULT hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
	if (FAILED(hr) || !pEnumerator) return Result;

	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	pEnumerator->Release();
	if (FAILED(hr) || !pDevice) return Result;

	LPWSTR pwszId = nullptr;
	hr = pDevice->GetId(&pwszId);
	if (SUCCEEDED(hr) && pwszId)
	{
		Result.DeviceId = pwszId;
		CoTaskMemFree(pwszId);
	}

	IPropertyStore* pProps = nullptr;
	if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps)) && pProps)
	{
		PROPVARIANT varName;
		PropVariantInit(&varName);
		if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.vt == VT_LPWSTR && varName.pwszVal)
		{
			Result.FriendlyName = varName.pwszVal;
		}
		PropVariantClear(&varName);

		// PKEY_AudioEngine_DeviceFormat - may report stereo; probe IsFormatSupported for actual max
		int32 FormatChannels = 2;
		PROPVARIANT varFormat;
		PropVariantInit(&varFormat);
		if (SUCCEEDED(pProps->GetValue(PKEY_AudioEngine_DeviceFormat_Local, &varFormat)) &&
		    varFormat.vt == VT_BLOB && varFormat.blob.pBlobData && varFormat.blob.cbSize >= sizeof(WAVEFORMATEX))
		{
			WAVEFORMATEX* pwfx = (WAVEFORMATEX*)varFormat.blob.pBlobData;
			FormatChannels = pwfx->nChannels;
			Result.SampleRate = pwfx->nSamplesPerSec;
			Result.BytesPerSample = pwfx->wBitsPerSample / 8;
			if (varFormat.blob.cbSize >= sizeof(WAVEFORMATEXTENSIBLE) && pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
			{
				WAVEFORMATEXTENSIBLE* pwfex = (WAVEFORMATEXTENSIBLE*)pwfx;
				Result.BytesPerSample = pwfex->Format.wBitsPerSample / 8;
			}
			Result.bIsValid = true;
		}
		PropVariantClear(&varFormat);
		pProps->Release();

		int32 ProbeCh = GetDeviceMaxChannels(pDevice);
		Result.NumChannels = FMath::Max(FormatChannels, ProbeCh);
	}

	if (!Result.bIsValid)
	{
		int32 MixChannels = 2;
		IAudioClient* pAudioClient = nullptr;
		if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient)) && pAudioClient)
		{
			WAVEFORMATEX* pwfx = nullptr;
			if (SUCCEEDED(pAudioClient->GetMixFormat(&pwfx)) && pwfx)
			{
				MixChannels = pwfx->nChannels;
				Result.SampleRate = pwfx->nSamplesPerSec;
				Result.BytesPerSample = pwfx->wBitsPerSample / 8;
				Result.bIsValid = true;
				CoTaskMemFree(pwfx);
			}
			pAudioClient->Release();
		}
		int32 ProbeCh = GetDeviceMaxChannels(pDevice);
		Result.NumChannels = FMath::Max(MixChannels, ProbeCh);
	}
	pDevice->Release();
#elif PLATFORM_MAC
	AudioObjectPropertyAddress Addr = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};
	AudioDeviceID DevID = kAudioObjectUnknown;
	UInt32 DataSize = sizeof(AudioDeviceID);
	OSStatus Err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &Addr, 0, nullptr, &DataSize, &DevID);
	if (Err != noErr || DevID == kAudioObjectUnknown) return Result;

	Addr.mSelector = kAudioDevicePropertyStreamConfiguration;
	Addr.mScope = kAudioDevicePropertyScopeOutput;
	UInt32 StreamSize = 0;
	Err = AudioObjectGetPropertyDataSize(DevID, &Addr, 0, nullptr, &StreamSize);
	if (Err == noErr && StreamSize > 0)
	{
		TArray<uint8> Buf;
		Buf.SetNum(StreamSize);
		Err = AudioObjectGetPropertyData(DevID, &Addr, 0, nullptr, &StreamSize, Buf.GetData());
		if (Err == noErr)
		{
			AudioBufferList* pList = (AudioBufferList*)Buf.GetData();
			UInt32 OutChannels = 0;
			for (UInt32 j = 0; j < pList->mNumberBuffers; ++j)
			{
				OutChannels += pList->mBuffers[j].mNumberChannels;
			}
			Result.NumChannels = static_cast<int32>(OutChannels);
		}
	}
	Result.SampleRate = 48000;
	Result.BytesPerSample = 4;

	Addr.mSelector = kAudioDevicePropertyDeviceUID;
	Addr.mScope = kAudioObjectPropertyScopeGlobal;
	CFStringRef UIDRef = nullptr;
	DataSize = sizeof(CFStringRef);
	Err = AudioObjectGetPropertyData(DevID, &Addr, 0, nullptr, &DataSize, &UIDRef);
	if (Err == noErr && UIDRef)
	{
		CFIndex Len = CFStringGetLength(UIDRef);
		Len = CFStringGetMaximumSizeForEncoding(Len, kCFStringEncodingUTF8) + 1;
		TArray<char> Tmp;
		Tmp.SetNum(static_cast<int32>(Len));
		if (CFStringGetCString(UIDRef, Tmp.GetData(), Len, kCFStringEncodingUTF8))
		{
			Result.DeviceId = UTF8_TO_TCHAR(Tmp.GetData());
		}
		CFRelease(UIDRef);
	}
	if (Result.DeviceId.IsEmpty())
	{
		Result.DeviceId = FString::Printf(TEXT("%u"), static_cast<unsigned>(DevID));
	}

	Addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
	CFStringRef NameRef = nullptr;
	DataSize = sizeof(CFStringRef);
	Err = AudioObjectGetPropertyData(DevID, &Addr, 0, nullptr, &DataSize, &NameRef);
	if (Err == noErr && NameRef)
	{
		CFIndex Len = CFStringGetLength(NameRef);
		Len = CFStringGetMaximumSizeForEncoding(Len, kCFStringEncodingUTF8) + 1;
		TArray<char> Tmp;
		Tmp.SetNum(static_cast<int32>(Len));
		if (CFStringGetCString(NameRef, Tmp.GetData(), Len, kCFStringEncodingUTF8))
		{
			Result.FriendlyName = UTF8_TO_TCHAR(Tmp.GetData());
		}
		CFRelease(NameRef);
	}
	if (Result.FriendlyName.IsEmpty())
	{
		Result.FriendlyName = Result.DeviceId;
	}
	Result.bIsValid = true;
#endif
	return Result;
}
