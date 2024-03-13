// Illia Huskov 2024


#include "Sandbox/Core/SSpawner.h"
#include "Components/BoxComponent.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values
ASSpawner::ASSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.1f;
	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(GetRootComponent());
	SpawnArea->SetBoxExtent(FVector(RadiusOfSpawn, RadiusOfSpawn, MaxZBoxExtent));
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetLineThickness(10.f);
	for (int32 Index = 0; Index != (int32)EObjectTypeQuery::ObjectTypeQuery_MAX; Index++)
	{
		ObjectTypeToTrace.Add(EObjectTypeQuery(Index));
	}
}

// Called when the game starts or when spawned
void ASSpawner::BeginPlay()
{
	Super::BeginPlay();
	OnStartCheck();
	if (bStartOnBeginPlay)
	{
		StartSpawn();
	}
}

void ASSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SpawnArea->SetBoxExtent(FVector(RadiusOfSpawn, RadiusOfSpawn, MaxZBoxExtent));
	SetActorScale3D(FVector(1.f, 1.f, 1.f));
}

// Called every frame
void ASSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SpawnActorsLoop();
}

void ASSpawner::SetParams(const TMap<UClass*, int32>& NewActorsToSpawn, float NewRadiusOfSpawn, float NewIntervalOfSpawn)
{
	ActorsToSpawn = NewActorsToSpawn;
	CheckClassValidity();
	RadiusOfSpawn = FMath::Clamp(NewRadiusOfSpawn, MinRadiusOfSpawn, MaxRadiusOfSpawn);
	IntervalOfSpawn = FMath::Clamp(NewIntervalOfSpawn, 0.f, MaxIntervalOfSpawn);
}

void ASSpawner::AddClassToSpawn(const TMap<UClass*, int32>& NewActorsToSpawn)
{
	ActorsToSpawn.Append(NewActorsToSpawn);
	CheckClassValidity();
}

void ASSpawner::StartSpawn()
{
	if (!ActorsToSpawn.IsEmpty())
	{
		if (GlobalSpawnLimit > 0 || GlobalSpawnLimit == -1)
		{
			if (IntervalOfSpawn > 0)
			{
				StopSpawnTimer();
				GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ASSpawner::StartTick, IntervalOfSpawn, true, IntervalOfSpawn);
			}
			StartTick();
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("End of limit in %s"), *GetActorNameOrLabel());
			StopSpawnTimer();
		}
	}
	else
	{
		StopSpawnTimer();
		UE_LOG(LogTemp, Warning, TEXT("No Actors to spawn in %s"), *GetActorNameOrLabel());
	}
}

void ASSpawner::CheckClassValidity()
{
	TArray<UClass*> InvalidClass;
	for (auto& Element : ActorsToSpawn)
	{
		if (Element.Key == nullptr || !Element.Key->IsChildOf(AActor::StaticClass()))
		{
			InvalidClass.Add(Element.Key);
		}
	}
	for (auto& Class : InvalidClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Remove invalid Actor from spawn list : %s in %s"), *Class->GetName(), *GetActorNameOrLabel());
		ActorsToSpawn.Remove(Class);
	}
}

void ASSpawner::SpawnActorsLoop()
{
	if (CurrentClassToSpawn != nullptr)
	{
		if (CurrentCountToSpawn > 0)
		{
			SpawnLoop();
		}
		else
		{
			if (FindNextSpawnParameters())
			{
				SpawnLoop();
			}
		}
	}
	else
	{
		ClassIndex = 0;
		TPair<UClass*, int32> FirstClass = ActorsToSpawn.Get(FSetElementId::FromInteger(0));
		CurrentClassToSpawn = FirstClass.Key;
		CurrentCountToSpawn = FirstClass.Value;
		SpawnLoop();
	}
}

void ASSpawner::GetRandomLocationInBox()
{
	//Breaking the cycle in this tick if more than 10 unsuccessful searches for free space
	if (LoopIterator > 10)
	{
		return;
	}
	FVector StartLocation = FMath::RandPointInBox(
		FBox(SpawnArea->Bounds.Origin - SpawnArea->Bounds.BoxExtent, SpawnArea->Bounds.Origin + SpawnArea->Bounds.BoxExtent));
	FVector EndLocation = StartLocation;
	StartLocation.Z += MaxZBoxExtent;
	EndLocation.Z -= MaxZBoxExtent;
	FHitResult Hit;
	GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECollisionChannel::ECC_WorldStatic);
	++LoopIterator;
	if (!Hit.ImpactPoint.Equals(FVector()))
	{
		CheckSpawnPosition(Hit.ImpactPoint);
	}
	else
	{
		GetRandomLocationInBox();
	}
}

void ASSpawner::CheckSpawnPosition(FVector& Location)
{
	const float HalfCapsuleHeight = 90.f;
	const float CapsuleDiameter = 60.f;
	Location.Z += HalfCapsuleHeight;
	TArray<FHitResult> Hit;
	TArray<AActor*> ActorToIgnore;
	UKismetSystemLibrary::SphereTraceMultiForObjects(
		GetWorld(), Location, Location, CapsuleDiameter, ObjectTypeToTrace, false, ActorToIgnore, EDrawDebugTrace::None, Hit, true);
	if (Hit.Num() > 0)
	{
		for (FHitResult& HittedObject : Hit)
		{
			if (HittedObject.GetComponent()->GetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn) == ECollisionResponse::ECR_Block)
			{
				GetRandomLocationInBox();
				return;
			}
		}
	}
	Spawn(Location);
}

void ASSpawner::StopSpawnTimer()
{
	if (GetWorld()->GetTimerManager().IsTimerActive(SpawnTimer))
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	}
}

void ASSpawner::Spawn(const FVector& Location)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(CurrentClassToSpawn, Location, FRotator(0.f), SpawnParameters);
	if (SpawnedActor != nullptr)
	{
		LastSpawnedActors.Add(SpawnedActor);
		if (GlobalSpawnLimit > 0)
		{
			--GlobalSpawnLimit;
		}
		if (CurrentCountToSpawn > 0)
		{
			--CurrentCountToSpawn;
		}
	}
}

void ASSpawner::SpawnLoop()
{
	for (int32 i = 0; i < SpawnPerFrame; ++i)
	{
		if (CurrentCountToSpawn == 0)
		{
			if (!FindNextSpawnParameters())
			{
				return;
			}
		}
		if (GlobalSpawnLimit == -1 || GlobalSpawnLimit > 0)
		{
			LoopIterator = 0;
			GetRandomLocationInBox();
		}
		else
		{
			StopSpawnTimer();
			AfterSpawn();
			SetActorTickEnabled(false);
			UE_LOG(LogTemp, Display, TEXT("End of limit in %s"), *GetActorNameOrLabel());
			return;
		}
	}
}

void ASSpawner::AfterSpawn()
{
	CurrentClassToSpawn = nullptr;
	CurrentCountToSpawn = 0;
	ClassIndex = 0;
	OnFinishSpawnDelegate.Broadcast(LastSpawnedActors.Num(), LastSpawnedActors);
	UE_LOG(LogTemp, Display, TEXT("Spawn complete number of spawned actors %d"), LastSpawnedActors.Num());
}

void ASSpawner::OnStartCheck()
{
	TArray<TEnumAsByte<EObjectTypeQuery>> OverlapType;
	OverlapType.Add(EObjectTypeQuery::ObjectTypeQuery1);
	UClass* Actor = AActor::StaticClass();
	TArray<AActor*> ActorsToIgnore;
	TArray<AActor*> OutActors;
	UKismetSystemLibrary::BoxOverlapActors(
		GetWorld(), SpawnArea->GetComponentLocation(), SpawnArea->Bounds.BoxExtent, OverlapType, Actor, ActorsToIgnore, OutActors);
	if (OutActors.Num() == 0)
	{
		PrimaryActorTick.bCanEverTick = false;
		UE_LOG(LogTemp, Error, TEXT("Disable spawning. Invalid spawn position of %s"), *GetActorNameOrLabel());
		return;
	}
	CheckClassValidity();
}

bool ASSpawner::FindNextSpawnParameters()
{
	++ClassIndex;
	if (ActorsToSpawn.IsValidId(FSetElementId::FromInteger(ClassIndex)))
	{
		TPair<UClass*, int32> NextClass = ActorsToSpawn.Get(FSetElementId::FromInteger(ClassIndex));
		CurrentClassToSpawn = NextClass.Key;
		CurrentCountToSpawn = NextClass.Value;
		return true;
	}
	else
	{
		AfterSpawn();
		SetActorTickEnabled(false);
		return false;
	}
}
