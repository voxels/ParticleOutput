// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/Vector4.h"
#include "Math/Vector.h"
#include "TauBuffer.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PARTICLEOUTPUT_API UTauBuffer : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UTauBuffer();

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	bool IsGrowing;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	bool FullGestureIsGrowing;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	FVector MeasuringStick;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	FVector4 BeginningPosition;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	FVector4 EndingPosition;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	TArray<FVector4> MotionPath;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	TArray<float> FullGestureAngleChanges;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	TArray<float> IncrementalGestureAngleChanges;
	
	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	TArray<double> IncrementalTauSamples;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	TArray<double> FullGestureTauSamples;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<float> IncrementalTauDotSamples;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<float> FullGestureTauDotSamples;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<float> IncrementalTauDotSmoothedDiffFromLastFrame;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<float> FullGestureTauDotSmoothedDiffFromLastFrame;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	double BeginningTime;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	double LastReadingTime;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	double CurrentTime;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	double ElapsedSinceLastReadingTime;

	UPROPERTY(VisibleAnywhere, Category = "TauBuffer")
	double ElapsedSinceBeginningGestureTime;

	UFUNCTION(BlueprintCallable, Category = "TauBuffer")
	void CalculateIncrementalGestureChange();

	UFUNCTION(BlueprintCallable, Category = "TauBuffer")
	void CalculateFullGestureChange();



protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
