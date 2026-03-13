// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomPcmLoader.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"

#pragma pack(push, 1)
struct FWavRiffHeader
{
	uint8 Riff[4];
	uint32 ChunkSize;
	uint8 Wave[4];
};

struct FWavChunkHeader
{
	uint8 Id[4];
	uint32 Size;
};

struct FWavFmtChunk
{
	uint16 AudioFormat;
	uint16 NumChannels;
	uint32 SampleRate;
	uint32 ByteRate;
	uint16 BlockAlign;
	uint16 BitsPerSample;
};
#pragma pack(pop)

static bool ParseWavToFloat(const uint8* Data, int64 DataSize, FAudioLoomPcmResult& Out)
{
	if (!Data || DataSize < 12)
	{
		return false;
	}

	if (FMemory::Memcmp(Data, "RIFF", 4) != 0 || FMemory::Memcmp(Data + 8, "WAVE", 4) != 0)
	{
		return false;
	}

	int32 NumChannels = 0;
	int32 SampleRate = 48000;
	int32 BitsPerSample = 16;
	int32 AudioFormat = 1;
	const uint8* PcmData = nullptr;
	int64 PcmSize = 0;

	int64 Offset = 12;
	while (Offset + 8 <= DataSize)
	{
		const FWavChunkHeader* Chunk = (const FWavChunkHeader*)(Data + Offset);
		Offset += 8;
		int64 ChunkDataOffset = Offset;
		Offset += Chunk->Size;

		if (FMemory::Memcmp(Chunk->Id, "fmt ", 4) == 0 && Chunk->Size >= 16)
		{
			const FWavFmtChunk* Fmt = (const FWavFmtChunk*)(Data + ChunkDataOffset);
			AudioFormat = Fmt->AudioFormat;
			NumChannels = Fmt->NumChannels;
			SampleRate = Fmt->SampleRate;
			BitsPerSample = Fmt->BitsPerSample;
		}
		else if (FMemory::Memcmp(Chunk->Id, "data", 4) == 0)
		{
			PcmData = Data + ChunkDataOffset;
			PcmSize = FMath::Min((int64)Chunk->Size, DataSize - ChunkDataOffset);
		}
	}

	if (!PcmData || PcmSize <= 0 || NumChannels <= 0)
	{
		return false;
	}

	const int32 BytesPerSample = BitsPerSample / 8;
	const int64 TotalSamples = PcmSize / (BytesPerSample * NumChannels);
	Out.PCM.SetNumUninitialized(TotalSamples * NumChannels);
	Out.NumChannels = NumChannels;
	Out.SampleRate = SampleRate;

	if (AudioFormat == 1) // PCM
	{
		if (BitsPerSample == 16)
		{
			const int16* Src = (const int16*)PcmData;
			float* Dst = Out.PCM.GetData();
			for (int64 i = 0; i < TotalSamples * NumChannels; ++i)
			{
				Dst[i] = Src[i] / 32768.0f;
			}
		}
		else if (BitsPerSample == 32)
		{
			const int32* Src = (const int32*)PcmData;
			float* Dst = Out.PCM.GetData();
			for (int64 i = 0; i < TotalSamples * NumChannels; ++i)
			{
				Dst[i] = Src[i] / 2147483648.0f;
			}
		}
		else
		{
			return false;
		}
	}
	else if (AudioFormat == 3) // IEEE float
	{
		if (BitsPerSample == 32)
		{
			FMemory::Memcpy(Out.PCM.GetData(), PcmData, PcmSize);
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	Out.bSuccess = true;
	return true;
}

FAudioLoomPcmResult FAudioLoomPcmLoader::LoadFromFile(const FString& FilePath)
{
	FAudioLoomPcmResult Result;
	TArray<uint8> RawBytes;
	if (!FFileHelper::LoadFileToArray(RawBytes, *FilePath))
	{
		return Result;
	}
	Result.bSuccess = ParseWavToFloat(RawBytes.GetData(), RawBytes.Num(), Result);
	return Result;
}

FAudioLoomPcmResult FAudioLoomPcmLoader::LoadFromSoundWave(USoundWave* SoundWave)
{
	FAudioLoomPcmResult Result;
	if (!SoundWave)
	{
		return Result;
	}

	TArray<uint8> RawPCMBytes;
	uint32 SampleRate = 0;
	uint16 NumChannels = 0;

#if WITH_EDITOR
	if (SoundWave->GetImportedSoundWaveData(RawPCMBytes, SampleRate, NumChannels) && RawPCMBytes.Num() > 0)
	{
		// GetImportedSoundWaveData returns 16-bit PCM. Convert to float.
		const int32 NumSamples = RawPCMBytes.Num() / sizeof(int16);
		Result.PCM.SetNumUninitialized(NumSamples);
		const int16* Src = reinterpret_cast<const int16*>(RawPCMBytes.GetData());
		float* Dst = Result.PCM.GetData();
		for (int32 i = 0; i < NumSamples; ++i)
		{
			Dst[i] = Src[i] / 32768.0f;
		}
		Result.NumChannels = NumChannels;
		Result.SampleRate = SampleRate;
		Result.bSuccess = true;
		return Result;
	}
#endif

	// Fallback: try loading from file path if asset has a source
	FString PackagePath = SoundWave->GetPathName();
	FString PackageFilename;
	if (FPackageName::DoesPackageExist(PackagePath, &PackageFilename))
	{
		// Try common audio extensions next to the package
		TArray<FString> Extensions = { TEXT(".wav"), TEXT(".WAV") };
		FString BasePath = FPaths::GetPath(PackageFilename) / FPaths::GetBaseFilename(PackageFilename);
		for (const FString& Ext : Extensions)
		{
			FString CandidatePath = BasePath + Ext;
			if (FPaths::FileExists(CandidatePath))
			{
				Result = LoadFromFile(CandidatePath);
				if (Result.bSuccess) return Result;
			}
		}
	}

	return Result;
}
