// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CaptureSceneComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable )
class PARTICLEOUTPUT_API ACaptureSceneComponent : public ACharacter
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ACaptureSceneComponent();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Capture, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseTurnRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseLookUpRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LED_Output)
	class UTextureRenderTarget2D* PanelARenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LED_Output)
	class UTextureRenderTarget2D* PanelBRenderTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = LED_Output)
	class UDynamicTexture* BufferTexture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LED_Output)
	int FrameCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = LED_Output )
	bool bIsConnected;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = LED_Output)
	FString PanelAMessage;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = LED_Output)
	FString PanelBMessage;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = LED_Output)
	FString FrameMessage;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = LED_Output)
	FString TimeMessage;

	uint8* OutBufPanelA;
	uint8* OutBufPanelB;

	UFUNCTION(BlueprintImplementableEvent, Category = LED_Output)
	void MessageStored();

	UFUNCTION(BlueprintCallable, Category = LED_Output)
	void CaptureFrameIntoString(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWRite, Category = LED_Output)
	int FrameRateLimit;

	float LastFPS;
	double LastCapturedTime;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void FillFrameData(float DeltaTime, FString Name, UTextureRenderTarget2D* RenderTexture, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT, uint8* OutBuf);
	FString FillPanelMessage(FString PanelMessage, uint8* OutBuf, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT);
	FString FillFrameMessage(FString FillMessage, FString PanelA_In, FString PanelB_In, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT, int32 PANELS_IN_CHAIN);
	FString FillTimeMessage(FString FillMessage);
	FString uint64_to_string(uint64 value);
	bool SaveTexture(FString TextureName, UTexture2D* outTexture);
	UTexture2D* ACaptureSceneComponent::CreateNewTexture(FString TextureName, uint8* InBuf, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT);
	void MoveForward(float Value);
	void MoveRight(float Value);
	void MoveUp(float Value);

	// Called via input to turn at given rate
	// @param Rate This is a normalized rate, 1.0 means 100%
	void TurnAtRate(float Rate);

	// Called via input to turn at given rate
	// @param Rate This is a normalized rate, 1.0 means 100%
	void LookUpAtRate(float Rate);




	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
