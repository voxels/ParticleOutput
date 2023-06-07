// Fill out your copyright notice in the Description page of Project Settings.


#include "BP_ParticleGenerator.h"

// Sets default values
ABP_ParticleGenerator::ABP_ParticleGenerator()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ABP_ParticleGenerator::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABP_ParticleGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABP_ParticleGenerator::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

