// Copyright 2014 Allegorithmic All Rights Reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceCoreModule.h"

#include "SubstanceCoreClasses.h"

#include "AssetRegistryModule.h"
#include "ModuleManager.h"
#include "Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubstanceCoreModule, Log, All);

//Resolve library file name depending on build platform
#if defined (SUBSTANCE_LIB_DYNAMIC)
#	if PLATFORM_WINDOWS
#		if PLATFORM_64BITS
#			define SUBSTANCE_LIB_CPU_DYNAMIC_PATH	TEXT("DLLs/Win64/SubstanceCPU.dll")
#			define SUBSTANCE_LIB_GPU_DYNAMIC_PATH	TEXT("DLLs/Win64/SubstanceGPU.dll")
#		else
#			error Unsupported platform for dynamic substance loading
#		endif
#   elif PLATFORM_MAC
#       define SUBSTANCE_LIB_CPU_DYNAMIC_PATH TEXT("DLLs/Mac/libSubstanceCPU.dylib")
#       define SUBSTANCE_LIB_GPU_DYNAMIC_PATH TEXT("DLLs/Mac/libSubstanceGPU.dylib")
#   elif PLATFORM_LINUX
#		define SUBSTANCE_LIB_CPU_DYNAMIC_PATH TEXT("DLLs/Linux/libSubstanceCPU.so")
#		define SUBSTANCE_LIB_GPU_DYNAMIC_PATH TEXT("DLLs/Linux/libSubstanceGPU.so")
#   else
#       error Unsupported platform for dynamic substance loading
#	endif
#endif

//Substance API
namespace Substance
{
	ISubstanceAPI* gAPI = nullptr;
}

namespace
{
	static FWorldDelegates::FWorldInitializationEvent::FDelegate OnWorldInitDelegate;
	static FDelegateHandle OnWorldInitDelegateHandle;

	static FWorldDelegates::FOnLevelChanged::FDelegate OnLevelAddedDelegate;
	static FDelegateHandle OnLevelAddedDelegateHandle;
}

void FSubstanceCoreModule::StartupModule()
{
#if defined (SUBSTANCE_LIB_DYNAMIC)
	const TCHAR* libraryFileName =
		(GetDefault<USubstanceSettings>()->SubstanceEngine == ESubstanceEngineType::SET_CPU)
		? SUBSTANCE_LIB_CPU_DYNAMIC_PATH
		: SUBSTANCE_LIB_GPU_DYNAMIC_PATH;

	//The plugin can exist in a few different places, so lets try each one in turn
	FString prefixPaths[] = {
		FPaths::EnginePluginsDir(),
		FPaths::GamePluginsDir(),
		FPaths::Combine(*FPaths::EnginePluginsDir(), TEXT("Marketplace/")),
	};

	bool bLoaded = false;

	size_t numPaths = sizeof(prefixPaths) / sizeof(FString);
	for (size_t i = 0;i < numPaths;i++)
	{
		FString libraryPath = FPaths::Combine(*prefixPaths[i], TEXT("Substance/"), libraryFileName);
		if (FPaths::FileExists(libraryPath))
		{
			if (void* pLibraryHandle = FPlatformProcess::GetDllHandle(*libraryPath))
			{
				if (pfnGetSubstanceAPI pGetSubstanceAPI = (pfnGetSubstanceAPI)FPlatformProcess::GetDllExport(pLibraryHandle, TEXT("GetSubstanceAPI")))
				{
					unsigned int result = pGetSubstanceAPI(&Substance::gAPI);
					if (result == 0)
					{
						bLoaded = true;
						break;
					}
				}
			}
		}
	}

	if (!bLoaded)
	{
		UE_LOG(LogSubstanceCoreModule, Fatal, TEXT("Unable to load Substance Library."));
	}
#else
	GetSubstanceAPI(&gAPI);
#endif

	UE_LOG(LogSubstanceCoreModule, Log, TEXT("Substance %s Engine Loaded, Max Texture Size %i"), ANSI_TO_TCHAR(Substance::gAPI->SubstanceAPIName()), Substance::gAPI->SubstanceMaxTextureSize());

	Substance::Helpers::SetupSubstance();

	// Register tick function.
	TickDelegate = FTickerDelegate::CreateRaw(this, &FSubstanceCoreModule::Tick);
	FTicker::GetCoreTicker().AddTicker( TickDelegate );

	RegisterSettings();

	::OnWorldInitDelegate = FWorldDelegates::FWorldInitializationEvent::FDelegate::CreateStatic(&FSubstanceCoreModule::OnWorldInitialized);
	::OnWorldInitDelegateHandle = FWorldDelegates::OnPostWorldInitialization.Add(::OnWorldInitDelegate);
	
	::OnLevelAddedDelegate = FWorldDelegates::FOnLevelChanged::FDelegate::CreateStatic(&FSubstanceCoreModule::OnLevelAdded);
	::OnLevelAddedDelegateHandle = FWorldDelegates::LevelAddedToWorld.Add(::OnLevelAddedDelegate);

#if WITH_EDITOR
	FEditorDelegates::BeginPIE.AddRaw(this, &FSubstanceCoreModule::OnBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FSubstanceCoreModule::OnEndPIE);
#endif //WITH_EDITOR

	PIE = false;
}


void FSubstanceCoreModule::ShutdownModule()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(::OnWorldInitDelegateHandle);
	FWorldDelegates::LevelAddedToWorld.Remove(::OnLevelAddedDelegateHandle);

	UnregisterSettings();

	Substance::Helpers::TearDownSubstance();
}


bool FSubstanceCoreModule::Tick(float DeltaTime)
{
	Substance::Helpers::Tick();

	return true;
}

unsigned int FSubstanceCoreModule::GetMaxOutputTextureSize() const
{
	if (Substance::gAPI)
	{
		return Substance::gAPI->SubstanceMaxTextureSize();
	}

	return 0;
}

void FSubstanceCoreModule::OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS)
{
	Substance::Helpers::OnWorldInitialized();
}

void FSubstanceCoreModule::OnLevelAdded(ULevel* Level, UWorld* World)
{
	Substance::Helpers::OnLevelAdded();
}

#if WITH_EDITOR
void FSubstanceCoreModule::OnBeginPIE(const bool bIsSimulating)
{
	PIE = true;
}

void FSubstanceCoreModule::OnEndPIE(const bool bIsSimulating)
{
	PIE = false;
}
#endif

IMPLEMENT_MODULE( FSubstanceCoreModule, SubstanceCore );
