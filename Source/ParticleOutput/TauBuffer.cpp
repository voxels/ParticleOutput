#include "TauBuffer.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values for this component's properties
UTauBuffer::UTauBuffer()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	IsGrowing = false;
	MeasuringStick = FVector(0);
	BeginningPosition = FVector(0);
	EndingPosition = FVector(0);

	MotionPath.Emplace(BeginningPosition);

	BeginningTime = FApp::GetCurrentTime();
	LastReadingTime = FApp::GetCurrentTime();
	CurrentTime = FApp::GetCurrentTime();

	ElapsedSinceLastReadingTime = 0;
	ElapsedSinceBeginningGestureTime = 0;
}


// Called when the game starts
void UTauBuffer::BeginPlay()
{
	Super::BeginPlay();

}


// Called every frame
void UTauBuffer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UTauBuffer::CalculateIncrementalGestureChange()
{
	if (MotionPath.Num() < 1) {
		IncrementalGestureAngleChanges.Emplace(0);
		return;
	}

	FVector BeginningNormal = MotionPath[MotionPath.Num()-2];
	FVector EndingNormal = EndingPosition;

	BeginningNormal.Normalize();
	EndingNormal.Normalize();

	float CurrentGestureDotProduct = FVector::DotProduct(BeginningNormal, EndingNormal);
	float AngleChange = UKismetMathLibrary::Acos(CurrentGestureDotProduct);
	IncrementalGestureAngleChanges.Emplace(AngleChange);

	if (IncrementalGestureAngleChanges.Num() >= 2) {
		double EndAngle = IncrementalGestureAngleChanges.Last();
		double CheckAngle = IncrementalGestureAngleChanges.Last(1);

		double EndTime = CurrentTime;
		double CheckTime = LastReadingTime;

		if (EndTime - CheckTime > 0 && EndAngle - CheckAngle != 0) {
			double angleChange = EndAngle - CheckAngle;
			double velocity = angleChange / (EndTime - CheckTime);
			double IncrementalTau = 3.14159 / velocity;
			IncrementalTauSamples.Emplace(IncrementalTau);
			//UE_LOG(LogTemp, Display, TEXT("Incremental Tau: %f"), IncrementalTau);
		}
	}

	if (IncrementalTauSamples.Num() > 2) {
		double EndTau = IncrementalTauSamples.Last();
		double CheckTau = IncrementalTauSamples.Last(1);

		double EndTime = CurrentTime;
		double CheckTime = LastReadingTime;

		if (EndTime - CheckTime > 0) {
			double IncrementalTauDot = (EndTau - CheckTau);
			IncrementalTauDotSamples.Emplace(IncrementalTauDot);
			//UE_LOG(LogTemp, Display, TEXT("Incremental Tau Dot: %f"), IncrementalTauDot);
		}
	}

	if (IncrementalTauDotSamples.Num() > 4) {
		double EndTau = IncrementalTauDotSamples.Last();
		double ThirdTau = IncrementalTauDotSamples.Last(1);
		double SecondTau = IncrementalTauDotSamples.Last(2);

		double SecondMean = (EndTau + ThirdTau) / 2;
		double FirstMean = (ThirdTau + SecondTau) / 2;
		double Diff = SecondMean - FirstMean;
		IncrementalTauDotSmoothedDiffFromLastFrame.Emplace(Diff);

		//UE_LOG(LogTemp, Display, TEXT("Incremental Tau Dot Smoothed Mean: %f"), SecondMean);
	}

	if (IncrementalTauDotSmoothedDiffFromLastFrame.Num() > 2) {
		double EndDiff = IncrementalTauDotSmoothedDiffFromLastFrame.Last();
		double CheckDiff = IncrementalTauDotSmoothedDiffFromLastFrame.Last(1);
		if( abs(EndDiff - CheckDiff) < EndDiff * 0.15) {
			//UE_LOG(LogTemp, Display, TEXT("Incremental Diff is constant:\t%f\t%f"), EndDiff, CheckDiff);
		}
		else 
		{
			//UE_LOG(LogTemp, Display, TEXT("Diff is NOT constant:\t%f\t%f"), EndDiff, CheckDiff);
		}

		if (EndDiff - CheckDiff < 0) {
			IsGrowing = false;
		}
		else {
			IsGrowing = true;
		}

		//UE_LOG(LogTemp, Display, TEXT("Incremental Is Growing:\t%d"), IsGrowing);
	}
}

void UTauBuffer::CalculateFullGestureChange()
{
	if (MotionPath.Num() < 1) {
		FullGestureAngleChanges.Emplace(0);
		return;
	}

	FVector BeginningNormal = BeginningPosition;
	FVector EndingNormal = EndingPosition;

	BeginningNormal.Normalize();
	EndingNormal.Normalize();

	float CurrentGestureDotProduct = FVector::DotProduct(BeginningNormal, EndingNormal);
	float AngleChange = UKismetMathLibrary::Acos(CurrentGestureDotProduct);
	FullGestureAngleChanges.Emplace(AngleChange);

	if (FullGestureAngleChanges.Num() >= 2) {
		double EndAngle = FullGestureAngleChanges.Last();
		double CheckAngle = 0;

		double IncrementalEndAngle = IncrementalGestureAngleChanges.Last();
		double IncrementalCheckAngle = IncrementalGestureAngleChanges.Last(1);

		double EndTime = CurrentTime;
		double CheckTime = LastReadingTime;
		if (EndTime - CheckTime > 0  && EndAngle - CheckAngle  > 0) {
			double Distance = (EndAngle - CheckAngle);
			double RateOfClosure = ((IncrementalEndAngle - IncrementalCheckAngle) / (EndTime - CheckTime));
			double FullGestureTau = 3.14159 / RateOfClosure ;
			FullGestureTauSamples.Emplace(FullGestureTau);
		}
	}

	if (FullGestureTauSamples.Num() > 2) {
		double EndTau = FullGestureTauSamples.Last();
		double CheckTau = FullGestureTauSamples.Last(1);

		double EndTime = CurrentTime;
		double CheckTime = LastReadingTime;

		if (EndTime - CheckTime > 0) {
			double FullGestureTauDot = (EndTau - CheckTau);
			FullGestureTauDotSamples.Emplace(FullGestureTauDot);
		}
	}


	if (FullGestureTauDotSamples.Num() > 4) {
		double EndTau = FullGestureTauDotSamples.Last();
		double ThirdTau = FullGestureTauDotSamples.Last(1);
		double SecondTau = FullGestureTauDotSamples.Last(2);

		double SecondMean = (EndTau + ThirdTau) / 2;
		double FirstMean = (ThirdTau + SecondTau) / 2;
		double Diff = SecondMean - FirstMean;
		FullGestureTauDotSmoothedDiffFromLastFrame.Emplace(Diff);
		//UE_LOG(LogTemp, Display, TEXT("FullGesture Tau Dot Smoothed Mean: %f"), SecondMean);
	}

	if (FullGestureTauDotSmoothedDiffFromLastFrame.Num() > 2) {
		double EndDiff = FullGestureTauDotSmoothedDiffFromLastFrame.Last();
		double CheckDiff = FullGestureTauDotSmoothedDiffFromLastFrame.Last(1);
		if (abs(EndDiff - CheckDiff) < EndDiff * 0.15) {
			//UE_LOG(LogTemp, Display, TEXT("Full Gesture Diff is constant:\t%f\t$f"),EndDiff, CheckDiff);
		}
		else {
			//UE_LOG(LogTemp, Display, TEXT("Full Gesture Diff is NOT constant:\t%f\t$f"), EndDiff, CheckDiff);
		}

		if (EndDiff - CheckDiff < 0) {
			FullGestureIsGrowing = false;
		}
		else {
			FullGestureIsGrowing = true;
		}

		//UE_LOG(LogTemp, Display, TEXT("Full Gesture Is Growing:\t%d"), FullGestureIsGrowing);
	}
} 