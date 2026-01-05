// Guardian & Seeker - Asset Loading Utility
// 소프트 레퍼런스 에셋을 비동기로 로드하기 위한 유틸리티 클래스

#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GS_AssetLoader.generated.h"

// 비동기 로드 핸들 타입 별칭
using FAsyncLoadHandle = TSharedPtr<struct FStreamableHandle>;

/**
 * 에셋 로드 완료 시 호출되는 델리게이트
 * @param LoadedAsset 로드된 에셋 (nullptr이면 로드 실패)
 */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAssetLoaded, UObject*, LoadedAsset);

/**
 * Soft Reference 에셋을 비동기로 로드하기 위한 유틸리티 클래스
 *
 * 사용 예시:
 * ```cpp
 * // 헤더 파일
 * UPROPERTY(EditDefaultsOnly)
 * TSoftObjectPtr<UNiagaraSystem> MyVFX;
 *
 * // cpp 파일
 * UGS_AssetLoader::AsyncLoadAsset(MyVFX, [this](UObject* LoadedAsset)
 * {
 *     if (UNiagaraSystem* VFX = Cast<UNiagaraSystem>(LoadedAsset))
 *     {
 *         // VFX 사용
 *     }
 * });
 * ```
 */
UCLASS()
class GAS_API UGS_AssetLoader : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Soft Object Pointer를 비동기로 로드
	 *
	 * @param SoftObjectPtr 로드할 소프트 오브젝트 포인터
	 * @param OnLoaded 로드 완료 시 호출될 콜백
	 * @return StreamableHandle (로드 취소를 위해 사용 가능)
	 */
	template <typename T>
	static TSharedPtr<FStreamableHandle> AsyncLoadAsset(
	    const TSoftObjectPtr<T>& SoftObjectPtr,
	    TFunction<void(T*)> OnLoaded)
	{
		if (SoftObjectPtr.IsNull())
		{
			// UE_LOG(LogTemp, Warning, TEXT("[GS_AssetLoader] AsyncLoadAsset: SoftObjectPtr is null"));
			if (OnLoaded)
			{
				OnLoaded(nullptr);
			}
			return nullptr;
		}

		// 이미 로드된 경우 즉시 반환
		if (T* LoadedAsset = SoftObjectPtr.Get())
		{
			if (OnLoaded)
			{
				OnLoaded(LoadedAsset);
			}
			return nullptr;
		}

		// 비동기 로드 시작
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		return Streamable.RequestAsyncLoad(
		    SoftObjectPtr.ToSoftObjectPath(),
		    [SoftObjectPtr, OnLoaded]()
		    {
			    if (T* LoadedAsset = SoftObjectPtr.Get())
			    {
				    if (OnLoaded)
				    {
					    OnLoaded(LoadedAsset);
				    }
			    }
			    else
			    {
				    UE_LOG(LogTemp, Error, TEXT("[GS_AssetLoader] Failed to load asset: %s"),
				           *SoftObjectPtr.ToSoftObjectPath().ToString());
				    if (OnLoaded)
				    {
					    OnLoaded(nullptr);
				    }
			    }
		    });
	}

	/**
	 * Soft Class Pointer를 비동기로 로드
	 *
	 * @param SoftClassPtr 로드할 소프트 클래스 포인터
	 * @param OnLoaded 로드 완료 시 호출될 콜백
	 * @return StreamableHandle
	 */
	template <typename T>
	static TSharedPtr<FStreamableHandle> AsyncLoadClass(
	    const TSoftClassPtr<T>& SoftClassPtr,
	    TFunction<void(TSubclassOf<T>)> OnLoaded)
	{
		if (SoftClassPtr.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[GS_AssetLoader] AsyncLoadClass: SoftClassPtr is null"));
			if (OnLoaded)
			{
				OnLoaded(nullptr);
			}
			return nullptr;
		}

		// 이미 로드된 경우 즉시 반환
		if (UClass* LoadedClass = SoftClassPtr.Get())
		{
			if (OnLoaded)
			{
				OnLoaded(TSubclassOf<T>(LoadedClass));
			}
			return nullptr;
		}

		// 비동기 로드 시작
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		return Streamable.RequestAsyncLoad(
		    SoftClassPtr.ToSoftObjectPath(),
		    [SoftClassPtr, OnLoaded]()
		    {
			    if (UClass* LoadedClass = SoftClassPtr.Get())
			    {
				    if (OnLoaded)
				    {
					    OnLoaded(TSubclassOf<T>(LoadedClass));
				    }
			    }
			    else
			    {
				    UE_LOG(LogTemp, Error, TEXT("[GS_AssetLoader] Failed to load class: %s"),
				           *SoftClassPtr.ToSoftObjectPath().ToString());
				    if (OnLoaded)
				    {
					    OnLoaded(nullptr);
				    }
			    }
		    });
	}

	/**
	 * 여러 에셋을 한꺼번에 비동기로 로드
	 *
	 * @param SoftObjectPaths 로드할 에셋 경로들
	 * @param OnAllLoaded 모든 에셋 로드 완료 시 호출될 콜백
	 * @return StreamableHandle
	 */
	static TSharedPtr<FStreamableHandle> AsyncLoadMultipleAssets(
	    const TArray<FSoftObjectPath>& SoftObjectPaths,
	    TFunction<void()> OnAllLoaded)
	{
		if (SoftObjectPaths.Num() == 0)
		{
			if (OnAllLoaded)
			{
				OnAllLoaded();
			}
			return nullptr;
		}

		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		return Streamable.RequestAsyncLoad(
		    SoftObjectPaths,
		    [OnAllLoaded]()
		    {
			    if (OnAllLoaded)
			    {
				    OnAllLoaded();
			    }
		    });
	}

	/**
	 * Soft Object Pointer를 동기로 로드 (주의: 게임 프리즈 가능)
	 * 긴급 상황이나 초기화 단계에서만 사용
	 *
	 * @param SoftObjectPtr 로드할 소프트 오브젝트 포인터
	 * @return 로드된 에셋 (실패 시 nullptr)
	 */
	template <typename T>
	static T* SyncLoadAsset(const TSoftObjectPtr<T>& SoftObjectPtr)
	{
		if (SoftObjectPtr.IsNull())
		{
			return nullptr;
		}

		// 이미 로드된 경우
		if (T* LoadedAsset = SoftObjectPtr.Get())
		{
			return LoadedAsset;
		}

		// 동기 로드 (블로킹)
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		Streamable.RequestSyncLoad(SoftObjectPtr.ToSoftObjectPath());

		return SoftObjectPtr.Get();
	}

	/**
	 * 블루프린트에서 호출 가능한 비동기 로드 함수
	 *
	 * @param SoftObjectPath 로드할 에셋 경로
	 * @param OnLoaded 로드 완료 시 호출될 델리게이트
	 */
	UFUNCTION(BlueprintCallable, Category = "GS|AssetLoader", meta = (AutoCreateRefTerm = "OnLoaded"))
	static void BP_AsyncLoadAsset(const FSoftObjectPath& SoftObjectPath, const FOnAssetLoaded& OnLoaded);
};
