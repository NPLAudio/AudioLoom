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
#include "Windows/HideWindowsPlatformTypes.h"

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
			pProps->Release();
		}

		IAudioClient* pAudioClient = nullptr;
		hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
		if (SUCCEEDED(hr) && pAudioClient)
		{
			WAVEFORMATEX* pwfx = nullptr;
			hr = pAudioClient->GetMixFormat(&pwfx);
			if (SUCCEEDED(hr) && pwfx)
			{
				Info.NumChannels = pwfx->nChannels;
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
		pProps->Release();
	}

	IAudioClient* pAudioClient = nullptr;
	if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient)) && pAudioClient)
	{
		WAVEFORMATEX* pwfx = nullptr;
		if (SUCCEEDED(pAudioClient->GetMixFormat(&pwfx)) && pwfx)
		{
			Result.NumChannels = pwfx->nChannels;
			Result.SampleRate = pwfx->nSamplesPerSec;
			Result.BytesPerSample = pwfx->wBitsPerSample / 8;
			Result.bIsValid = true;
			CoTaskMemFree(pwfx);
		}
		pAudioClient->Release();
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
