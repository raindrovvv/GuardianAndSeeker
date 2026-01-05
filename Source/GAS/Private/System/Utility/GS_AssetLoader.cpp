// Guardian & Seeker - Asset Loading Utility

#include "System/Utility/GS_AssetLoader.h"

void UGS_AssetLoader::BP_AsyncLoadAsset(const FSoftObjectPath& SoftObjectPath, const FOnAssetLoaded& OnLoaded)
{
	if (!SoftObjectPath.IsValid())
	{
		OnLoaded.ExecuteIfBound(nullptr);
		return;
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(
		SoftObjectPath,
		[SoftObjectPath, OnLoaded]()
		{
			UObject* LoadedAsset = SoftObjectPath.ResolveObject();
			OnLoaded.ExecuteIfBound(LoadedAsset);
		}
	);
}
