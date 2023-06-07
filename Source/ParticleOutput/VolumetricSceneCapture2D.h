// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCapture2D.h"
#include "VolumetricSceneCapture2D.generated.h"

/**
 * 
 */
UCLASS()
class PARTICLEOUTPUT_API AVolumetricSceneCapture2D : public ASceneCapture2D
{
	GENERATED_BODY()
	
public:
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutMinimalViewInfo) override;
};
