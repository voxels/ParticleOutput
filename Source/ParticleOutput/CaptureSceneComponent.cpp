// Fill out your copyright notice in the Description page of Project Settings.

#include "CaptureSceneComponent.h"
#include "DynamicTexture.h"
#include <chrono>
#include <sstream>
#include "Camera/CameraComponent.h"
#include "Engine/SceneCapture2D.h"
#include "GameFramework/SpringArmComponent.h"
#include "VolumetricSceneCapture2D.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ScriptMacros.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"

using namespace std::chrono;

typedef struct RgbColor
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
} RgbColor;

typedef struct HsvColor
{
	unsigned char h;
	unsigned char s;
	unsigned char v;
} HsvColor;

RgbColor HsvToRgb(HsvColor hsv)
{
	RgbColor rgb;
	unsigned char region, remainder, p, q, t;

	if (hsv.s == 0)
	{
		rgb.r = hsv.v;
		rgb.g = hsv.v;
		rgb.b = hsv.v;
		return rgb;
	}

	region = hsv.h / 43;
	remainder = (hsv.h - (region * 43)) * 6;

	p = (hsv.v * (255 - hsv.s)) >> 8;
	q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
	t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

	switch (region)
	{
	case 0:
		rgb.r = hsv.v; rgb.g = t; rgb.b = p;
		break;
	case 1:
		rgb.r = q; rgb.g = hsv.v; rgb.b = p;
		break;
	case 2:
		rgb.r = p; rgb.g = hsv.v; rgb.b = t;
		break;
	case 3:
		rgb.r = p; rgb.g = q; rgb.b = hsv.v;
		break;
	case 4:
		rgb.r = t; rgb.g = p; rgb.b = hsv.v;
		break;
	default:
		rgb.r = hsv.v; rgb.g = p; rgb.b = q;
		break;
	}

	return rgb;
}

HsvColor RgbToHsv(RgbColor rgb)
{
	HsvColor hsv;
	unsigned char rgbMin, rgbMax;

	rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
	rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

	hsv.v = rgbMax;
	if (hsv.v == 0)
	{
		hsv.h = 0;
		hsv.s = 0;
		return hsv;
	}

	hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
	if (hsv.s == 0)
	{
		hsv.h = 0;
		return hsv;
	}

	if (rgbMax == rgb.r)
		hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
	else if (rgbMax == rgb.g)
		hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
	else
		hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

	return hsv;
}

// Sets default values for this component's properties
ACaptureSceneComponent::ACaptureSceneComponent() {
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	BaseTurnRate = 65.0;
	BaseLookUpRate = 65.0;

	// Don't rotate when the controller rotates
	// Let that Just affect the camera
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Configure Character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;  // Character moves in the direction of input
	
	GetCapsuleComponent()->SetCapsuleHalfHeight(8);
	GetCapsuleComponent()->SetCapsuleRadius(8);
	GetCharacterMovement()->MovementMode = MOVE_Flying;
	LastCapturedTime = FApp::GetCurrentTime();
	FrameRateLimit = 2;
	
}


// Called when the game starts
void ACaptureSceneComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

// Called to bind functionality to input
void ACaptureSceneComponent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &ACaptureSceneComponent::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ACaptureSceneComponent::MoveRight);
	PlayerInputComponent->BindAxis("MoveUp", this, &ACaptureSceneComponent::MoveUp);

	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ACaptureSceneComponent::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ACaptureSceneComponent::LookUpAtRate);
}

// Called every frame
void ACaptureSceneComponent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UE_LOG(LogTemp, Warning, TEXT("Tick"));
	UE_LOG(LogTemp, Warning, TEXT("%f"), DeltaTime);
	double FPS = DeltaTime;
	double CurrentTime = FApp::GetCurrentTime();
	double FrameLimitDuration = (FPS + LastFPS) / 2 * double(FrameRateLimit);
		UE_LOG(LogTemp, Warning, TEXT("Checking Time: %f\t%f\t%f\t%f"), FrameLimitDuration, LastCapturedTime, CurrentTime, CurrentTime - LastCapturedTime);
	if (CurrentTime - LastCapturedTime > FrameLimitDuration) {
		UE_LOG(LogTemp, Warning, TEXT("Capturing Frame: %f\t%f\t%f\t%f"), FrameLimitDuration, LastCapturedTime, CurrentTime,CurrentTime - LastCapturedTime);
		CaptureFrameIntoString(DeltaTime);
		LastCapturedTime = CurrentTime;
	}

	LastFPS = FPS;
}

void ACaptureSceneComponent::CaptureFrameIntoString(float DeltaTime)
{
	//PanelARenderTarget->UpdateResourceImmediate(true);
	//PanelBRenderTarget->UpdateResourceImmediate(true);

	if (PanelARenderTarget->RenderTargetFormat.GetValue() != 4) {
		PanelARenderTarget->OverrideFormat = EPixelFormat::PF_B8G8R8A8;
	}

	if (PanelBRenderTarget->RenderTargetFormat.GetValue() != 4) {
		PanelBRenderTarget->OverrideFormat = EPixelFormat::PF_B8G8R8A8;
	}

	if (PanelARenderTarget) {
		//UE_LOG(LogTemp, Warning, TEXT("Found Panel A"));
		//UE_LOG(LogTemp, Warning, TEXT("X: %i, Y:%i"), PanelARenderTarget->SizeX, PanelARenderTarget->SizeY);
		if (OutBufPanelA) {
			delete OutBufPanelA;
		}
		OutBufPanelA = new uint8[PanelARenderTarget->SizeX * PanelARenderTarget->SizeY * 3];
		FillFrameData(DeltaTime, "PanelA", PanelARenderTarget, PanelARenderTarget->SizeX, PanelARenderTarget->SizeY, OutBufPanelA);
		PanelAMessage = FillPanelMessage(PanelAMessage, OutBufPanelA, PanelARenderTarget->SizeX, PanelARenderTarget->SizeY);
	}

	if (PanelBRenderTarget) {
		//UE_LOG(LogTemp, Warning, TEXT("Found Panel B"));
		//UE_LOG(LogTemp, Warning, TEXT("X: %i, Y:%i"), PanelBRenderTarget->SizeX, PanelBRenderTarget->SizeY);

		if (OutBufPanelB) {
			delete OutBufPanelB;
		}
		OutBufPanelB = new uint8[PanelBRenderTarget->SizeX * PanelBRenderTarget->SizeY * 3];
		FillFrameData(DeltaTime, "PanelB", PanelBRenderTarget, PanelBRenderTarget->SizeX, PanelBRenderTarget->SizeY, OutBufPanelB);
		PanelBMessage = FillPanelMessage(PanelBMessage, OutBufPanelB, PanelARenderTarget->SizeX, PanelARenderTarget->SizeY);
	}

	if (PanelARenderTarget && PanelBRenderTarget)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Panel A Message"));
		//UE_LOG(LogTemp, Warning, TEXT("%s"), *PanelAMessage);
		//UE_LOG(LogTemp, Warning, TEXT("Panel B Message"));
		//UE_LOG(LogTemp, Warning, TEXT("%s"), *PanelBMessage);
		TimeMessage = FillTimeMessage(TimeMessage);
		FrameMessage = FillFrameMessage(FrameMessage, PanelAMessage, PanelBMessage, PanelARenderTarget->SizeX, PanelARenderTarget->SizeY, 2);
		//UE_LOG(LogTemp, Warning, TEXT("Frame Message"));
		//UE_LOG(LogTemp, Warning, TEXT("%s"), *FrameMessage);
		this->MessageStored();
	}
}

void ACaptureSceneComponent::FillFrameData(float DeltaTime, FString Name, UTextureRenderTarget2D* RenderTexture, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT, uint8* OutBuf)
{
	BufferTexture = NewObject<UDynamicTexture>(this);
	BufferTexture->Initialize(128, 128, FLinearColor::Black);

	UTexture2D* Aux2DTex = RenderTexture->ConstructTexture2D(this, Name, EObjectFlags::RF_NoFlags, CTF_DeferCompression);
	//Make sure it won't be compressed (https://wiki.unrealengine.com/Procedural_Materials#Texture_Setup)
	//Make sure it won't be compressed (https://wiki.unrealengine.com/Procedural_Materials#Texture_Setup)
	//UE_LOG(LogTemp, Warning, TEXT("Render Target Format: %i"), RenderTexture->RenderTargetFormat.GetValue());
	Aux2DTex->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;

	//It seems that this field is only available in editor mode
#if WITH_EDITORONLY_DATA
	Aux2DTex->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	//Turn off Gamma-correction
	Aux2DTex->SRGB = 0;
	//Update the texture with new variable values.
	Aux2DTex->UpdateResource();
	//FPlatformProcess::Sleep(0.011);

	const FColor* FormattedImageData = static_cast<const FColor*>(Aux2DTex->GetPlatformData()->Mips[0].BulkData.LockReadOnly());
	//UE_LOG(LogTemp, Warning, TEXT("Aux2DText pixel format: %i"), Aux2DTex->GetPixelFormat());
	//UE_LOG(LogTemp, Warning, TEXT("Rendering Formatted Image Data"), Aux2DTex->GetSizeX(), Aux2DTex->GetSizeY());
	//UE_LOG(LogTemp, Warning, TEXT("Aud2DTex Size X: %i, Y:%i"), Aux2DTex->GetSizeX(), Aux2DTex->GetSizeY());
	// UE_LOG(LogTemp, Warning, TEXT("Buffer Texture Width: %i"), BufferTexture->GetWidth());
	//UE_LOG(LogTemp, Warning, TEXT("Clearing Buffer"));
	BufferTexture->Clear();

	if (!BufferTexture->bDidInitialize || BufferTexture->GetWidth() == 0) {
		Aux2DTex->GetPlatformData()->Mips[0].BulkData.Unlock();
		return;
	}
	for (int32 Y = 0; Y < ALPHA_MAP_HEIGHT; Y++)
	{
		for (int32 X = 0; X < ALPHA_MAP_WIDTH; X++)
		{
			int index = Y * ALPHA_MAP_WIDTH + X;
			FColor Color = FormattedImageData[index];
			BufferTexture->SetPixel(X, Y, Color.ReinterpretAsLinear());

			if (Y == 0 && (X == 0 || X == 1) && false) {
				if (Color.R > 0 || Color.B > 0 || Color.G > 0) {
					UE_LOG(LogTemp, Warning, TEXT("Buffer Texture Index: %i, X: %i, Y:%i"), index, X, Y);
					UE_LOG(LogTemp, Warning, TEXT("Red: %i"), static_cast<int>(Color.ReinterpretAsLinear().R * 255));
					UE_LOG(LogTemp, Warning, TEXT("Green, %i"), static_cast<int>(Color.ReinterpretAsLinear().G * 255));
					UE_LOG(LogTemp, Warning, TEXT("Blue: %i"), static_cast<int>(Color.ReinterpretAsLinear().B * 255));
					UE_LOG(LogTemp, Warning, TEXT("Alpha: %i"), static_cast<int>(Color.ReinterpretAsLinear().A * 255));
				}
			}
		}
	}
	BufferTexture->UpdateTexture();
	FDynamicTextureBuffer Buffer = BufferTexture->ExternalBuffer;
	TArray<uint8> PixelColorValues = Buffer.PixelBuffer;

	for (int32 Y = 0; Y < ALPHA_MAP_HEIGHT; Y++)
	{
		for (int32 X = 0; X < ALPHA_MAP_WIDTH; X++)
		{
			int index = Y * ALPHA_MAP_WIDTH + X;
			FColor FinalColor = FColor::Black;
			if (PixelColorValues.Num() == 0) {
				OutBuf[3 * index] = FinalColor.R;
				OutBuf[3 * index + 1] = FinalColor.G;
				OutBuf[3 * index + 2] = FinalColor.B;
			}
			else {
				if (4 * index + 4 < PixelColorValues.Num()) {
					// Texture is BGR ordered so we are swapping pixels here
					OutBuf[3 * index] = PixelColorValues[4 * index + 2];
					OutBuf[3 * index + 1] = PixelColorValues[4 * index + 1];
					OutBuf[3 * index + 2] = PixelColorValues[4 * index];
					if (Y == 0 && (X == 0 || X == 1) && false) {
						UE_LOG(LogTemp, Warning, TEXT("Pixel Values Count: %i"), PixelColorValues.Num());
						UE_LOG(LogTemp, Warning, TEXT("PixelIndex: %i, ValueIndex: %i, X: %i, Y:%i"), 3 * index, 4 * index, X, Y);
						UE_LOG(LogTemp, Warning, TEXT("Red: %i\tAdjusted: %i"), static_cast<int>(PixelColorValues[4 * index + 2]), static_cast<int>(OutBuf[3 * index]));
						UE_LOG(LogTemp, Warning, TEXT("Green, %i\tAdjusted: %i"), static_cast<int>(PixelColorValues[4 * index + 1]), static_cast<int>(OutBuf[3 * index + 1]));
						UE_LOG(LogTemp, Warning, TEXT("Blue: %i\tAdjusted: %i"), static_cast<int>(PixelColorValues[4 * index]), static_cast<int>(OutBuf[3 * index + 2]));
						UE_LOG(LogTemp, Warning, TEXT("Alpha: %i"), static_cast<int>(PixelColorValues[4 * index + 4]));
					}
				}
				else {
					//UE_LOG(LogTemp, Warning, TEXT("Pixel Values Count: %i"), PixelColorValues.Num());
					OutBuf[3 * index] = FinalColor.R;
					OutBuf[3 * index + 1] = FinalColor.G;
					OutBuf[3 * index + 2] = FinalColor.B;
				}
			}
		}
	}


	FString TextureName = FString::FromInt(FrameCount);
	
	Aux2DTex->GetPlatformData()->Mips[0].BulkData.Unlock();
}

FString ACaptureSceneComponent::FillPanelMessage(FString PanelMessage, uint8* OutBuf, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT)
{
	//UE_LOG(LogTemp, Warning, TEXT("Filling Panel Message"));
	int32 PixelValueCount = ALPHA_MAP_WIDTH * ALPHA_MAP_HEIGHT * 3;
	PanelMessage = "";

	if (OutBuf == NULL) {
		UE_LOG(LogTemp, Warning, TEXT("Buffer is null"));
		return PanelMessage;
	}

	int len = sizeof(OutBuf) / sizeof(OutBuf[0]);
	if (len == 0) {
		UE_LOG(LogTemp, Warning, TEXT("No values Found in the buffer"));
		return PanelMessage;
	}

	for (int32 i = 0; i < PixelValueCount; i++) {
		int32 Value = OutBuf[i];
		PanelMessage.AppendInt(Value);
		PanelMessage.Append(",");
	}
	//UE_LOG(LogTemp, Warning, TEXT("Filled Panel Message"));
	return PanelMessage;
}

FString ACaptureSceneComponent::FillFrameMessage(FString FillMessage, FString PanelA_In, FString PanelB_In, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT, int32 PANELS_IN_CHAIN)
{
	//UE_LOG(LogTemp, Warning, TEXT("Filling Frame Message"));
	FillMessage = "";

	TArray<FString> OutA;
	PanelA_In.ParseIntoArray(OutA, TEXT(","), true);

	TArray<FString> OutB;
	PanelB_In.ParseIntoArray(OutB, TEXT(","), true);

	//UE_LOG(LogTemp, Warning, TEXT("OutA length: %i"), OutA.Num());
	//UE_LOG(LogTemp, Warning, TEXT("OutB length: %i"), OutB.Num());
	//UE_LOG(LogTemp, Warning, TEXT("Check %i"), ALPHA_MAP_WIDTH * ALPHA_MAP_HEIGHT * 3);
	if (OutA.Num() == 0 || OutB.Num() == 0) {
		return FillMessage;
	}

	//UE_LOG(LogTemp, Warning, TEXT("Filling Message"));

	int countIndex = 0;
	for (int32 j = 0; j < ALPHA_MAP_HEIGHT; j++) {
		for (int32 i = 0; i < ALPHA_MAP_WIDTH * PANELS_IN_CHAIN; i++)
		{
			if (i < ALPHA_MAP_WIDTH) {
				int index = (j * ALPHA_MAP_WIDTH + i) * 3;
				//UE_LOG(LogTemp, Warning, TEXT("Filling Frame A: %i"), i);
				FillMessage.Append(OutA[index]);
				FillMessage.Append(",");
				FillMessage.Append(OutA[index + 1]);
				FillMessage.Append(",");
				FillMessage.Append(OutA[index + 2]);
				FillMessage.Append(",");
				if (j == 0 && ( i == 0 || i == 1) && false) {
					//UE_LOG(LogTemp, Warning, TEXT("A Index: %i, X: %i, Y:%i"), countIndex, i, j);
					//UE_LOG(LogTemp, Warning, TEXT("Red: %s"), *FString(OutA[index]));
					//UE_LOG(LogTemp, Warning, TEXT("Green, %s"), *FString(OutA[index + 1]));
					//UE_LOG(LogTemp, Warning, TEXT("Blue: %s"), *FString(OutA[index + 2]));
				}
			}
			else if (i >= ALPHA_MAP_WIDTH) {
				int index = (j * ALPHA_MAP_WIDTH + i - ALPHA_MAP_WIDTH) * 3;

				//UE_LOG(LogTemp, Warning, TEXT("Filling Frame B: %i"), i);
				FillMessage.Append(OutB[(index)]);
				FillMessage.Append(",");
				FillMessage.Append(OutB[(index) + 1]);
				FillMessage.Append(",");
				FillMessage.Append(OutB[(index) + 2]);
				FillMessage.Append(",");

				if (j == ALPHA_MAP_HEIGHT - 1  && (i - ALPHA_MAP_WIDTH == ALPHA_MAP_WIDTH - 1)  || (i - ALPHA_MAP_WIDTH == ALPHA_MAP_WIDTH - 2) && false) {
					//UE_LOG(LogTemp, Warning, TEXT("B Index: %i, X: %i, Y:%i"), countIndex, i, j);
					//UE_LOG(LogTemp, Warning, TEXT("Red: %s"), *FString(OutB[index]));
					//UE_LOG(LogTemp, Warning, TEXT("Green, %s"), *FString(OutB[index + 1]));
					//UE_LOG(LogTemp, Warning, TEXT("Blue: %s"), *FString(OutB[index + 2]));
				}
			}

			countIndex += 3;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Filled Frame Message %i"), countIndex);
	//UE_LOG(LogTemp, Warning, TEXT("%s"), *FillMessage);
	return FillMessage;
}

FString ACaptureSceneComponent::FillTimeMessage(FString FillMessage) {
	FillMessage = "";
	uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	UE_LOG(LogTemp, Warning, TEXT("Filled Time Message %s"),*FString(uint64_to_string(ms)));
	FillMessage.Append(uint64_to_string(ms));
	return FillMessage;
}

FString ACaptureSceneComponent::uint64_to_string(uint64 value) {
	std::ostringstream os;
	os << value;
	std::string out = os.str();
	return UTF8_TO_TCHAR(out.c_str());
}

bool ACaptureSceneComponent::SaveTexture(FString TextureName, UTexture2D* outTexture)
{
	FString PackageName = TEXT("/Game/ProceduralTextures/");
	PackageName += TextureName;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	FAssetRegistryModule::AssetCreated(outTexture);
	Package->MarkPackageDirty();


	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	bool bSaved = UPackage::SavePackage(Package, outTexture, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr, true, true, SAVE_NoError);
	return bSaved;
}

UTexture2D* ACaptureSceneComponent::CreateNewTexture(FString TextureName, uint8* InBuf, int32 ALPHA_MAP_WIDTH, int32 ALPHA_MAP_HEIGHT) {
	FString PackageName = TEXT("/Game/ProceduralTextures/");
	PackageName += TextureName;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	UTexture2D* NewTexture = NewObject<UTexture2D>(Package, *TextureName, RF_Public | RF_Standalone | RF_MarkAsRootSet);

	NewTexture->SetPlatformData(new FTexturePlatformData());	// Then we initialize the PlatformData
	NewTexture->GetPlatformData()->SizeX = ALPHA_MAP_WIDTH;
	NewTexture->GetPlatformData()->SizeY = ALPHA_MAP_HEIGHT;
	NewTexture->GetPlatformData()->SetNumSlices(1);
	NewTexture->GetPlatformData()->PixelFormat = EPixelFormat::PF_B8G8R8A8;
	FTexture2DMipMap* Mip = new(NewTexture->GetPlatformData()->Mips) FTexture2DMipMap();
	Mip->SizeX = ALPHA_MAP_WIDTH;
	Mip->SizeY = ALPHA_MAP_HEIGHT;

	// Lock the texture so it can be modified
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	uint8* TextureData = (uint8*)Mip->BulkData.Realloc(ALPHA_MAP_WIDTH * ALPHA_MAP_HEIGHT * 4);
	FMemory::Memcpy(TextureData, InBuf, sizeof(uint8) * ALPHA_MAP_HEIGHT * ALPHA_MAP_WIDTH * 4);
	Mip->BulkData.Unlock();

	NewTexture->Source.Init(ALPHA_MAP_WIDTH, ALPHA_MAP_HEIGHT, 1, 1, ETextureSourceFormat::TSF_BGRA8, InBuf);

	NewTexture->UpdateResource();
	FAssetRegistryModule::AssetCreated(NewTexture);
	Package->MarkPackageDirty();

	return NewTexture;
}



void ACaptureSceneComponent::MoveForward(float Value)
{

	if (Controller != nullptr && Value != 0.0f) {
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.0f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const float rate = 1.0;
		AddMovementInput(Direction, Value * rate);
	}

}
void ACaptureSceneComponent::MoveRight(float Value)
{
	if (Controller != nullptr && Value != 0.0f) {
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.0f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		const float rate = 1.0;
		AddMovementInput(Direction, Value * rate);
	}
}

void ACaptureSceneComponent::MoveUp(float Value)
{
	UE_LOG(LogTemp, Display, TEXT("Move Up"));
	if (Controller != nullptr && Value != 0.0f) {
		const FRotator Rotation = GetCapsuleComponent()->GetRelativeRotation();
		const FRotator PitchRotation(0.0f, 0.0, Rotation.Pitch);

		const FVector Direction = FRotationMatrix(PitchRotation).GetUnitAxis(EAxis::Z);
		const float rate = 10.0;
		AddMovementInput(Direction, Value * rate, true);
	}
}


void ACaptureSceneComponent::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ACaptureSceneComponent::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}