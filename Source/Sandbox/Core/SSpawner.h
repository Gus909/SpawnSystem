// Illia Huskov 2024

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SSpawner.generated.h"

class UBoxComponent;

UCLASS()
class SANDBOX_API ASSpawner : public AActor
{
	GENERATED_BODY()
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFinishSpawnSignature, int32 /* Amount */, TArray<AActor*> /* Actors */)

public:
	UPROPERTY(EditAnywhere, Category = "SpawnParameters", meta = (DisplayName = "Actors To Spawn / Amount"))
	TMap<UClass*, int32> ActorsToSpawn;
	UPROPERTY(EditAnywhere, meta = (ClampMin = 100.f, ClampMax = 1000.f), Category = "SpawnParameters")
	float RadiusOfSpawn = 100.f;
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0.f, ClampMax = 300.f), Category = "SpawnParameters")
	float IntervalOfSpawn = 0.f;
	UPROPERTY(EditAnywhere, meta = (ClampMin = -1), Category = "SpawnParameters")
	int32 GlobalSpawnLimit = -1;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UBoxComponent> SpawnArea;

	FOnFinishSpawnSignature OnFinishSpawnDelegate;

private:

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypeToTrace;

	UClass* CurrentClassToSpawn;

	FTimerHandle SpawnTimer;

	TArray<AActor*> LastSpawnedActors;

	const float MinRadiusOfSpawn = 100.f;
	const float MaxRadiusOfSpawn = 1000.f;
	const float MaxIntervalOfSpawn = 300.f;
	const float MaxZBoxExtent = 50.f;
	const int32 SpawnPerFrame = 2;
	int32 CurrentCountToSpawn = 0;
	int32 ClassIndex = 0;
	int32 LoopIterator = 0;

public:
	// Sets default values for this actor's properties
	ASSpawner();
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	//Set basic spawn parameters
	void SetParams(TMap<UClass*, int32> NewActorsToSpawn, float NewRadiusOfSpawn = 100.f, float NewIntervalOfSpawn = 0.f);
	FORCEINLINE void SetRadiusOfSpawn(float NewRadius) { RadiusOfSpawn = FMath::Clamp(NewRadius, MinRadiusOfSpawn, MaxRadiusOfSpawn); }
	FORCEINLINE void SetIntervalOfSpawn(float NewInterval) { IntervalOfSpawn = FMath::Clamp(NewInterval, 0.f, MaxIntervalOfSpawn); }
	// Change amount of given class, add new if not exist
	FORCEINLINE void ChangeAmountToSpawn(UClass* ClassToChange, int32 NewAmount) { ActorsToSpawn.FindOrAdd(ClassToChange) = NewAmount; }
	// Add new class to spawn into existing map
	void AddClassToSpawn(TMap<UClass*, int32> NewActorsToSpawn);
	// Start spawning actors, setup timer to cyclic spawn if IntervalOfSpawn greater than 0
	UFUNCTION(BlueprintCallable)
	void StartSpawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	bool FindNextSpawnParameters();
	FORCEINLINE void StartTick()
	{
		LastSpawnedActors.Empty();
		SetActorTickEnabled(true);
	}
	void CheckClassValidity();
	void SpawnActorsLoop();
	void GetRandomLocationInBox();
	void CheckSpawnPosition(FVector Location);
	void StopSpawnTimer();
	void Spawn(FVector Location);
	void SpawnLoop();
	void AfterSpawn();
	void OnStartCheck();
};