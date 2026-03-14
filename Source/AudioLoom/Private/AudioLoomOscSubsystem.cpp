// Copyright (c) 2026 AudioLoom Contributors.

#include "AudioLoomOscSubsystem.h"
#include "AudioLoomComponent.h"
#include "AudioLoomOscSettings.h"
#include "OSCManager.h"
#include "OSCServer.h"
#include "OSCClient.h"
#include "OSCMessage.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

#define LOCTEXT_NAMESPACE "AudioLoomOsc"

static FString NormalizeOscAddress(const FString& Addr)
{
	FString S = Addr.TrimStartAndEnd();
	if (!S.StartsWith(TEXT("/")))
	{
		S = TEXT("/") + S;
	}
	return S;
}

bool UAudioLoomOscSubsystem::IsPortAvailable(int32 Port)
{
	if (Port <= 0 || Port > 65535) return false;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem) return false;

	FSocket* TestSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("AudioLoomPortCheck"), true);
	if (!TestSocket) return false;

	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	bool bValid = false;
	Addr->SetIp(TEXT("0.0.0.0"), bValid);
	Addr->SetPort(static_cast<uint16>(Port));

	bool bBound = TestSocket->Bind(*Addr);
	TestSocket->Close();
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(TestSocket);

	return bBound;
}

void UAudioLoomOscSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UAudioLoomOscSubsystem::Deinitialize()
{
	StopListening();
	Super::Deinitialize();
}

void UAudioLoomOscSubsystem::RebuildComponentRegistry()
{
	PlayTagToComponents.Empty();
	StopTagToComponents.Empty();
	LoopTagToComponents.Empty();

	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UAudioLoomComponent*> Comps;
		It->GetComponents(Comps);
		for (UAudioLoomComponent* Comp : Comps)
		{
			if (!IsValid(Comp)) continue;

			FString Base = NormalizeOscAddress(Comp->GetOscAddress());
			if (Base.IsEmpty()) continue;

			FString PlayTag = Base + TEXT("/play");
			FString StopTag = Base + TEXT("/stop");
			FString LoopTag = Base + TEXT("/loop");

			PlayTagToComponents.FindOrAdd(PlayTag).Add(Comp);
			StopTagToComponents.FindOrAdd(StopTag).Add(Comp);
			LoopTagToComponents.FindOrAdd(LoopTag).Add(Comp);
		}
	}
}

bool UAudioLoomOscSubsystem::StartListening()
{
	StopListening();

	const UAudioLoomOscSettings* Settings = GetDefault<UAudioLoomOscSettings>();
	const int32 Port = Settings->ListenPort;

	if (!IsPortAvailable(Port))
	{
		UE_LOG(LogTemp, Warning, TEXT("AudioLoom OSC: Port %d is not available"), Port);
		return false;
	}

	OSCServer = UOSCManager::CreateOSCServer(TEXT("0.0.0.0"), Port, false, true, TEXT("AudioLoom"), this);
	if (!OSCServer)
	{
		UE_LOG(LogTemp, Error, TEXT("AudioLoom OSC: Failed to create server on port %d"), Port);
		return false;
	}

	OSCServer->OnOscMessageReceivedNative.AddUObject(this, &UAudioLoomOscSubsystem::HandleOSCMessage);

	RebuildComponentRegistry();

	UpdateMonitoringTarget();

	UE_LOG(LogTemp, Log, TEXT("AudioLoom OSC: Listening on port %d"), Port);
	return true;
}

void UAudioLoomOscSubsystem::StopListening()
{
	if (OSCServer)
	{
		OSCServer->Stop();
		OSCServer->OnOscMessageReceivedNative.RemoveAll(this);
		OSCServer = nullptr;
	}
	if (OSCClient)
	{
		OSCClient = nullptr;
	}
}

bool UAudioLoomOscSubsystem::IsListening() const
{
	return OSCServer != nullptr;
}

void UAudioLoomOscSubsystem::UpdateMonitoringTarget()
{
	const UAudioLoomOscSettings* Settings = GetDefault<UAudioLoomOscSettings>();
	FString IP = Settings->SendIP;
	int32 Port = Settings->SendPort;
	if (IP.IsEmpty()) IP = TEXT("127.0.0.1");
	if (Port <= 0) Port = 9001;

	OSCClient = UOSCManager::CreateOSCClient(IP, Port, TEXT("AudioLoomMonitor"), this);
}

void UAudioLoomOscSubsystem::HandleOSCMessage(const FOSCMessage& Message, const FString& IPAddress, uint16 Port)
{
	const FString AddressStr = UOSCManager::GetOSCAddressFullPath(Message.GetAddress());
	FString Normalized = NormalizeOscAddress(AddressStr);

	float TriggerValue = 0.f;
	TArray<float> Floats;
	UOSCManager::GetAllFloats(Message, Floats);
	if (Floats.Num() > 0) TriggerValue = Floats[0];
	else
	{
		TArray<int32> Ints;
		UOSCManager::GetAllInt32s(Message, Ints);
		if (Ints.Num() > 0) TriggerValue = static_cast<float>(Ints[0]);
	}

	if (TArray<TWeakObjectPtr<UAudioLoomComponent>>* PlayComps = PlayTagToComponents.Find(Normalized))
	{
		const bool bTriggerPlay = (TriggerValue > 0.5f || Floats.Num() == 0);
		for (TWeakObjectPtr<UAudioLoomComponent>& W : *PlayComps)
		{
			if (UAudioLoomComponent* C = W.Get())
			{
				if (bTriggerPlay) C->Play();
			}
		}
	}

	if (TArray<TWeakObjectPtr<UAudioLoomComponent>>* StopComps = StopTagToComponents.Find(Normalized))
	{
		for (TWeakObjectPtr<UAudioLoomComponent>& W : *StopComps)
		{
			if (UAudioLoomComponent* C = W.Get())
			{
				C->Stop();
			}
		}
	}

	if (TArray<TWeakObjectPtr<UAudioLoomComponent>>* LoopComps = LoopTagToComponents.Find(Normalized))
	{
		const bool bEnableLoop = (TriggerValue > 0.5f);
		for (TWeakObjectPtr<UAudioLoomComponent>& W : *LoopComps)
		{
			if (UAudioLoomComponent* C = W.Get())
			{
				C->SetLoop(bEnableLoop);
			}
		}
	}
}

void UAudioLoomOscSubsystem::SendStateUpdate(UAudioLoomComponent* Component, bool bPlaying)
{
	if (!OSCClient || !OSCClient->IsActive() || !IsValid(Component)) return;

	FString Base = NormalizeOscAddress(Component->GetOscAddress());
	if (Base.IsEmpty()) return;

	FString Addr = Base + TEXT("/state");

	FOSCMessage Msg;
	Msg.SetAddress(UOSCManager::ConvertStringToOSCAddress(Addr));
	UOSCManager::AddFloat(Msg, bPlaying ? 1.f : 0.f);
	OSCClient->SendOSCMessage(Msg);
}

#undef LOCTEXT_NAMESPACE
