// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "GameFramework/Character.h"
#include "TauBuffer.h"
#include "Containers/CircularBuffer.h"
#include "ParticleGenerator.generated.h"

UCLASS()
class PARTICLEOUTPUT_API AParticleGenerator : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AParticleGenerator();

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	double LastReadingTime;

	UPROPERTY(EditAnywhere, Category = "ParticleGenerator")
	int SmoothingSamplesCount;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FName> SocketNames;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FName> SocketBoneNames;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FVector> SocketLocations;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FVector> PreviousSocketLocations;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FRotator> SocketRotations;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FVector> TrianglePositions;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FName> TriangleIndexBoneNames;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FRotator> TriangleRotations;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FVector> PreviousTrianglePositions;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FRotator> PreviousTriangleRotations;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FVector> TriangleCentroids;
	
	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FVector> TriangleCircumcenters;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<FVector> EulerLines;

	UPROPERTY(VisibleAnywhere, Category = "ParticleGenerator")
	TArray<UTauBuffer *> TriangleTauBuffers;

	UFUNCTION(BlueprintCallable, Category = "ParticleGenerator")
	void UpdateSocketRawData();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	std::vector < std::vector<float> > LocationSamples;
	std::vector < std::vector<float> > RotationSamples;


	TArray<FVector> AB;
	TArray<FVector> ABmid;
	TArray<FVector> BC;
	TArray<FVector> BCmid;
	TArray<FVector> CA;
	TArray<FVector> CAmid;
	TArray<FVector> V;
	TArray<FVector> D1;
	TArray<FVector> D2;
	TArray<FVector> D3;
	TArray<FVector> ABBC;


	UFUNCTION(BlueprintCallable, Category = "ParticleGenerator")
	void UpdateTriangles(TArray<FName>BoneNames, TArray<FVector>Locations, TArray<FRotator>Rotations);

	UFUNCTION(BlueprintCallable, Category = "ParticleGenerator")
	void UpdateDebugLines();

	UFUNCTION(BlueprintCallable, Category = "ParticleGenerator")
	void UpdateEulerLines();

	UFUNCTION(BlueprintCallable, Category = "ParticleGenerator")
	void UpdateTracking();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
