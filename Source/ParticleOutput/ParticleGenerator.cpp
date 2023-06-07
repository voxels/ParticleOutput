// Fill out your copyright notice in the Description page of Project Settings.


#include "ParticleGenerator.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "UObject/Object.h"
#include "TauBuffer.h"
#include <Runtime/RenderCore/Public/RenderGraphBuilder.h>
#include "UObject/UObjectGlobals.h"
#include "Math/Vector.h"

// Classes below from circumcenter.cpp in MeshKit   https://bitbucket.org/fathomteam/meshkit.git
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <string>

#include <iostream>

using namespace std;

//
//Let a, b, c, d, and m be vectors in R^3.  Let ax, ay, and az be the components
//of a, and likewise for b, c, and d.  Let |a| denote the Euclidean norm of a,
//and let a x b denote the cross product of a and b.  Then
//
//    |                                                                       |
//    | |d-a|^2 [(b-a)x(c-a)] + |c-a|^2 [(d-a)x(b-a)] + |b-a|^2 [(c-a)x(d-a)] |
//    |                                                                       |
//r = -------------------------------------------------------------------------,
//                              | bx-ax  by-ay  bz-az |
//                            2 | cx-ax  cy-ay  cz-az |
//                              | dx-ax  dy-ay  dz-az |
//
//and
//
//        |d-a|^2 [(b-a)x(c-a)] + |c-a|^2 [(d-a)x(b-a)] + |b-a|^2 [(c-a)x(d-a)]
//m = a + ---------------------------------------------------------------------.
//                                | bx-ax  by-ay  bz-az |
//                              2 | cx-ax  cy-ay  cz-az |
//                                | dx-ax  dy-ay  dz-az |
//
//Some notes on stability:
//
//- Note that the expression for r is purely a function of differences between
//  coordinates.  The advantage is that the relative error incurred in the
//  computation of r is also a function of the _differences_ between the
//  vertices, and is not influenced by the _absolute_ coordinates of the
//  vertices.  In most applications, vertices are usually nearer to each other
//  than to the origin, so this property is advantageous.  If someone gives you
//  a formula that doesn't have this property, be wary.
//
//  Similarly, the formula for m incurs roundoff error proportional to the
//  differences between vertices, but does not incur roundoff error proportional
//  to the absolute coordinates of the vertices until the final addition.

//- These expressions are unstable in only one case:  if the denominator is
//  close to zero.  This instability, which arises if the tetrahedron is nearly
//  degenerate, is unavoidable.  Depending on your application, you may want
//  to use exact arithmetic to compute the value of the determinant.
//  Fortunately, this determinant is the basis of the well-studied 3D orientation
//  test, and several fast algorithms for providing accurate approximations to
//  the determinant are available.  Some resources are available from the
//  "Numerical and algebraic computation" page of Nina Amenta's Directory of
//  Computational Geometry Software:

//  http://www.geom.umn.edu/software/cglist/alg.html

//  If you're using floating-point inputs (as opposed to integers), one
//  package that can estimate this determinant somewhat accurately is my own:

//  http://www.cs.cmu.edu/~quake/robust.html

//- If you want to be even more aggressive about stability, you might reorder
//  the vertices of the tetrahedron so that the vertex `a' (which is subtracted
//  from the other vertices) is, roughly speaking, the vertex at the center of
//  the minimum spanning tree of the tetrahedron's four vertices.  For more
//  information about this idea, see Steven Fortune, "Numerical Stability of
//  Algorithms for 2D Delaunay Triangulations," International Journal of
//  Computational Geometry & Applications 5(1-2):193-213, March-June 1995.

//For completeness, here are stable expressions for the circumradius and
//circumcenter of a triangle, in R^2 and in R^3.  Incidentally, the expressions
//given here for R^2 are better behaved in terms of relative error than the
//suggestions currently given in the Geometry Junkyard
//(http://www.ics.uci.edu/~eppstein/junkyard/triangulation.html).

//Triangle in R^2:
//
//     |b-a| |c-a| |b-c|            < Note: You only want to compute one sqrt, so
//r = ------------------,             use sqrt{ |b-a|^2 |c-a|^2 |b-c|^2 }
//      | bx-ax  by-ay |
//    2 | cx-ax  cy-ay |
//
//          | by-ay  |b-a|^2 |
//          | cy-ay  |c-a|^2 |
//mx = ax - ------------------,
//            | bx-ax  by-ay |
//          2 | cx-ax  cy-ay |
//
//          | bx-ax  |b-a|^2 |
//          | cx-ax  |c-a|^2 |
//my = ay + ------------------.
//            | bx-ax  by-ay |
//          2 | cx-ax  cy-ay |
//
//Triangle in R^3:
//
//    |                                                           |
//    | |c-a|^2 [(b-a)x(c-a)]x(b-a) + |b-a|^2 (c-a)x[(b-a)x(c-a)] |
//    |                                                           |
//r = -------------------------------------------------------------,
//                         2 | (b-a)x(c-a) |^2
//
//        |c-a|^2 [(b-a)x(c-a)]x(b-a) + |b-a|^2 (c-a)x[(b-a)x(c-a)]
//m = a + ---------------------------------------------------------.
//                           2 | (b-a)x(c-a) |^2
//
//Finally, here's some C code that computes the vector m-a (whose norm is r) in
//each of these three cases.  Notice the #ifdef statements, which allow you to
//choose whether or not to use my aforementioned package to approximate the
//determinant.  (No attempt is made here to reorder the vertices to improve
//stability.)

/*****************************************************************************/
/*                                                                           */
/*  tetcircumcenter()   Find the circumcenter of a tetrahedron.              */
/*                                                                           */
/*  The result is returned both in terms of xyz coordinates and xi-eta-zeta  */
/*  coordinates, relative to the tetrahedron's point `a' (that is, `a' is    */
/*  the origin of both coordinate systems).  Hence, the xyz coordinates      */
/*  returned are NOT absolute; one must add the coordinates of `a' to        */
/*  find the absolute coordinates of the circumcircle.  However, this means  */
/*  that the result is frequently more accurate than would be possible if    */
/*  absolute coordinates were returned, due to limited floating-point        */
/*  precision.  In general, the circumradius can be computed much more       */
/*  accurately.                                                              */
/*                                                                           */
/*  The xi-eta-zeta coordinate system is defined in terms of the             */
/*  tetrahedron.  Point `a' is the origin of the coordinate system.          */
/*  The edge `ab' extends one unit along the xi axis.  The edge `ac'         */
/*  extends one unit along the eta axis.  The edge `ad' extends one unit     */
/*  along the zeta axis.  These coordinate values are useful for linear      */
/*  interpolation.                                                           */
/*                                                                           */
/*  If `xi' is NULL on input, the xi-eta-zeta coordinates will not be        */
/*  computed.                                                                */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/

void tetcircumcenter(double a[3], double b[3], double c[3], double d[3],
	double circumcenter[3], double* xi, double* eta, double* zeta)
{
	double xba, yba, zba, xca, yca, zca, xda, yda, zda;
	double balength, calength, dalength;
	double xcrosscd, ycrosscd, zcrosscd;
	double xcrossdb, ycrossdb, zcrossdb;
	double xcrossbc, ycrossbc, zcrossbc;
	double denominator;
	double xcirca, ycirca, zcirca;

	/* Use coordinates relative to point `a' of the tetrahedron. */
	xba = b[0] - a[0];
	yba = b[1] - a[1];
	zba = b[2] - a[2];
	xca = c[0] - a[0];
	yca = c[1] - a[1];
	zca = c[2] - a[2];
	xda = d[0] - a[0];
	yda = d[1] - a[1];
	zda = d[2] - a[2];
	/* Squares of lengths of the edges incident to `a'. */
	balength = xba * xba + yba * yba + zba * zba;
	calength = xca * xca + yca * yca + zca * zca;
	dalength = xda * xda + yda * yda + zda * zda;
	/* Cross products of these edges. */
	xcrosscd = yca * zda - yda * zca;
	ycrosscd = zca * xda - zda * xca;
	zcrosscd = xca * yda - xda * yca;
	xcrossdb = yda * zba - yba * zda;
	ycrossdb = zda * xba - zba * xda;
	zcrossdb = xda * yba - xba * yda;
	xcrossbc = yba * zca - yca * zba;
	ycrossbc = zba * xca - zca * xba;
	zcrossbc = xba * yca - xca * yba;

	/* Calculate the denominator of the formulae. */
#ifdef EXACT
  /* Use orient3d() from http://www.cs.cmu.edu/~quake/robust.html     */
  /*   to ensure a correctly signed (and reasonably accurate) result, */
  /*   avoiding any possibility of division by zero.                  */
	denominator = 0.5 / orient3d(b, c, d, a);
#else
  /* Take your chances with floating-point roundoff. */
	printf(" Warning: IEEE floating points used: Define -DEXACT in makefile \n");
	denominator = 0.5 / (xba * xcrosscd + yba * ycrosscd + zba * zcrosscd);
#endif

	/* Calculate offset (from `a') of circumcenter. */
	xcirca = (balength * xcrosscd + calength * xcrossdb + dalength * xcrossbc) *
		denominator;
	ycirca = (balength * ycrosscd + calength * ycrossdb + dalength * ycrossbc) *
		denominator;
	zcirca = (balength * zcrosscd + calength * zcrossdb + dalength * zcrossbc) *
		denominator;
	circumcenter[0] = xcirca;
	circumcenter[1] = ycirca;
	circumcenter[2] = zcirca;

	if (xi != (double*)NULL) {
		/* To interpolate a linear function at the circumcenter, define a    */
		/*   coordinate system with a xi-axis directed from `a' to `b',      */
		/*   an eta-axis directed from `a' to `c', and a zeta-axis directed  */
		/*   from `a' to `d'.  The values for xi, eta, and zeta are computed */
		/*   by Cramer's Rule for solving systems of linear equations.       */
		*xi = (xcirca * xcrosscd + ycirca * ycrosscd + zcirca * zcrosscd) *
			(2.0 * denominator);
		*eta = (xcirca * xcrossdb + ycirca * ycrossdb + zcirca * zcrossdb) *
			(2.0 * denominator);
		*zeta = (xcirca * xcrossbc + ycirca * ycrossbc + zcirca * zcrossbc) *
			(2.0 * denominator);
	}
}

/*****************************************************************************/
/*****************************************************************************/
/*                                                                           */
/*  tricircumcenter()   Find the circumcenter of a triangle.                 */
/*                                                                           */
/*  The result is returned both in terms of x-y coordinates and xi-eta       */
/*  coordinates, relative to the triangle's point `a' (that is, `a' is       */
/*  the origin of both coordinate systems).  Hence, the x-y coordinates      */
/*  returned are NOT absolute; one must add the coordinates of `a' to        */
/*  find the absolute coordinates of the circumcircle.  However, this means  */
/*  that the result is frequently more accurate than would be possible if    */
/*  absolute coordinates were returned, due to limited floating-point        */
/*  precision.  In general, the circumradius can be computed much more       */
/*  accurately.                                                              */
/*                                                                           */
/*  The xi-eta coordinate system is defined in terms of the triangle.        */
/*  Point `a' is the origin of the coordinate system.  The edge `ab' extends */
/*  one unit along the xi axis.  The edge `ac' extends one unit along the    */
/*  eta axis.  These coordinate values are useful for linear interpolation.  */
/*                                                                           */
/*  If `xi' is NULL on input, the xi-eta coordinates will not be computed.   */
/*                                                                           */
/*****************************************************************************/


/*****************************************************************************/
void tricircumcenter(double a[2], double b[2], double c[2], double circumcenter[2],
	double* xi, double* eta)
{
	double xba, yba, xca, yca;
	double balength, calength;
	double denominator;
	double xcirca, ycirca;

	/* Use coordinates relative to point `a' of the triangle. */
	xba = b[0] - a[0];
	yba = b[1] - a[1];
	xca = c[0] - a[0];
	yca = c[1] - a[1];
	/* Squares of lengths of the edges incident to `a'. */
	balength = xba * xba + yba * yba;
	calength = xca * xca + yca * yca;

	/* Calculate the denominator of the formulae. */
#ifdef EXACT
  /* Use orient2d() from http://www.cs.cmu.edu/~quake/robust.html     */
  /*   to ensure a correctly signed (and reasonably accurate) result, */
  /*   avoiding any possibility of division by zero.                  */
	denominator = 0.5 / orient2d(b, c, a);
#else
  /* Take your chances with floating-point roundoff. */
	denominator = 0.5 / (xba * yca - yba * xca);
#endif

	/* Calculate offset (from `a') of circumcenter. */
	xcirca = (yca * balength - yba * calength) * denominator;
	ycirca = (xba * calength - xca * balength) * denominator;
	circumcenter[0] = xcirca;
	circumcenter[1] = ycirca;

	if (xi != (double*)NULL) {
		/* To interpolate a linear function at the circumcenter, define a     */
		/*   coordinate system with a xi-axis directed from `a' to `b' and    */
		/*   an eta-axis directed from `a' to `c'.  The values for xi and eta */
		/*   are computed by Cramer's Rule for solving systems of linear      */
		/*   equations.                                                       */
		*xi = (xcirca * yca - ycirca * xca) * (2.0 * denominator);
		*eta = (ycirca * xba - xcirca * yba) * (2.0 * denominator);
	}
}

/****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  tricircumcenter3d()   Find the circumcenter of a triangle in 3D.         */
/*                                                                           */
/*  The result is returned both in terms of xyz coordinates and xi-eta       */
/*  coordinates, relative to the triangle's point `a' (that is, `a' is       */
/*  the origin of both coordinate systems).  Hence, the xyz coordinates      */
/*  returned are NOT absolute; one must add the coordinates of `a' to        */
/*  find the absolute coordinates of the circumcircle.  However, this means  */
/*  that the result is frequently more accurate than would be possible if    */
/*  absolute coordinates were returned, due to limited floating-point        */
/*  precision.  In general, the circumradius can be computed much more       */
/*  accurately.                                                              */
/*                                                                           */
/*  The xi-eta coordinate system is defined in terms of the triangle.        */
/*  Point `a' is the origin of the coordinate system.  The edge `ab' extends */
/*  one unit along the xi axis.  The edge `ac' extends one unit along the    */
/*  eta axis.  These coordinate values are useful for linear interpolation.  */
/*                                                                           */
/*  If `xi' is NULL on input, the xi-eta coordinates will not be computed.   */
/*                                                                           */
/*****************************************************************************/
/*****************************************************************************/
void tricircumcenter3d(double a[3], double b[3], double c[3], double circumcenter[3],
	double* xi, double* eta)
{
	double xba, yba, zba, xca, yca, zca;
	double balength, calength;
	double xcrossbc, ycrossbc, zcrossbc;
	double denominator;
	double xcirca, ycirca, zcirca;

	/* Use coordinates relative to point `a' of the triangle. */
	xba = b[0] - a[0];
	yba = b[1] - a[1];
	zba = b[2] - a[2];
	xca = c[0] - a[0];
	yca = c[1] - a[1];
	zca = c[2] - a[2];
	/* Squares of lengths of the edges incident to `a'. */
	balength = xba * xba + yba * yba + zba * zba;
	calength = xca * xca + yca * yca + zca * zca;

	/* Cross product of these edges. */
#ifdef EXACT
  /* Use orient2d() from http://www.cs.cmu.edu/~quake/robust.html     */
  /*   to ensure a correctly signed (and reasonably accurate) result, */
  /*   avoiding any possibility of division by zero.                  */

	A[0] = b[1]; A[1] = b[2];
	B[0] = c[1]; B[1] = c[2];
	C[0] = a[1]; C[1] = a[2];
	xcrossbc = orient2d(A, B, C);

	A[0] = c[0]; A[1] = c[2];
	B[0] = b[0]; B[1] = b[2];
	C[0] = a[0]; C[1] = a[2];
	ycrossbc = orient2d(A, B, C);

	A[0] = b[0]; A[1] = b[1];
	B[0] = c[0]; B[1] = c[1];
	C[0] = a[0]; C[1] = a[1];
	zcrossbc = orient2d(A, B, C);

	/*
	xcrossbc = orient2d(b[1], b[2], c[1], c[2], a[1], a[2]);
	ycrossbc = orient2d(b[2], b[0], c[2], c[0], a[2], a[0]);
	zcrossbc = orient2d(b[0], b[1], c[0], c[1], a[0], a[1]);
	*/
#else
	printf(" Warning: IEEE floating points used: Define -DEXACT in makefile \n");
	/* Take your chances with floating-point roundoff. */
	xcrossbc = yba * zca - yca * zba;
	ycrossbc = zba * xca - zca * xba;
	zcrossbc = xba * yca - xca * yba;
#endif

	/* Calculate the denominator of the formulae. */
	denominator = 0.5 / (xcrossbc * xcrossbc + ycrossbc * ycrossbc +
		zcrossbc * zcrossbc);

	/* Calculate offset (from `a') of circumcenter. */
	xcirca = ((balength * yca - calength * yba) * zcrossbc -
		(balength * zca - calength * zba) * ycrossbc) * denominator;
	ycirca = ((balength * zca - calength * zba) * xcrossbc -
		(balength * xca - calength * xba) * zcrossbc) * denominator;
	zcirca = ((balength * xca - calength * xba) * ycrossbc -
		(balength * yca - calength * yba) * xcrossbc) * denominator;
	circumcenter[0] = xcirca;
	circumcenter[1] = ycirca;
	circumcenter[2] = zcirca;

	if (xi != (double*)NULL) {
		/* To interpolate a linear function at the circumcenter, define a     */
		/*   coordinate system with a xi-axis directed from `a' to `b' and    */
		/*   an eta-axis directed from `a' to `c'.  The values for xi and eta */
		/*   are computed by Cramer's Rule for solving systems of linear      */
		/*   equations.                                                       */

		/* There are three ways to do this calculation - using xcrossbc, */
		/*   ycrossbc, or zcrossbc.  Choose whichever has the largest    */
		/*   magnitude, to improve stability and avoid division by zero. */
		if (((xcrossbc >= ycrossbc) ^ (-xcrossbc > ycrossbc)) &&
			((xcrossbc >= zcrossbc) ^ (-xcrossbc > zcrossbc))) {
			*xi = (ycirca * zca - zcirca * yca) / xcrossbc;
			*eta = (zcirca * yba - ycirca * zba) / xcrossbc;
		}
		else if ((ycrossbc >= zcrossbc) ^ (-ycrossbc > zcrossbc)) {
			*xi = (zcirca * xca - xcirca * zca) / ycrossbc;
			*eta = (xcirca * zba - zcirca * xba) / ycrossbc;
		}
		else {
			*xi = (xcirca * yca - ycirca * xca) / zcrossbc;
			*eta = (ycirca * xba - xcirca * yba) / zcrossbc;
		}
	}
}
/****************************************************************************/
void TriCircumCenter2D(double* a, double* b, double* c, double* result,
	double* param)
{
	tricircumcenter(a, b, c, result, &param[0], &param[1]);

	result[0] += a[0];
	result[1] += a[1];
}
/****************************************************************************/
void TriCircumCenter3D(double* a, double* b, double* c, double* result,
	double* param)
{
	tricircumcenter3d(a, b, c, result, &param[0], &param[1]);
	result[0] += a[0];
	result[1] += a[1];
	result[2] += a[2];
}

/****************************************************************************/
void TriCircumCenter3D(double* a, double* b, double* c, double* result)
{
	double xi, eta;
	tricircumcenter3d(a, b, c, result, &xi, &eta);
	result[0] += a[0];
	result[1] += a[1];
	result[2] += a[2];
}
// Sets default values
AParticleGenerator::AParticleGenerator()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AParticleGenerator::BeginPlay()
{
	Super::BeginPlay();
	LocationSamples = { {} };
	RotationSamples = { {} };
	SmoothingSamplesCount = 20;
}

// Called every frame
void AParticleGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (LastReadingTime == 0) {
		LastReadingTime = DeltaTime;
	}

	UpdateSocketRawData();
	
	UpdateTriangles(SocketBoneNames, SocketLocations, SocketRotations);
	
	if (false) {
		int index = 0;
		for (FName Name : TriangleIndexBoneNames) {
			FVector IndexLocation = TrianglePositions[index];
			FRotator IndexRotation = TriangleRotations[index];
			UE_LOG(LogTemp, Display, TEXT("%i\t%s"), index, *Name.ToString());
			//UE_LOG(LogTemp, Display, TEXT("%i\tx:%f\ty:%f\tz:%f"), index, IndexLocation.X, IndexLocation.Y, IndexLocation.Z);
			//UE_LOG(LogTemp, Display, TEXT("%i\tr:%f\tp:%f\ty:%f"), index, IndexRotation.Roll, IndexRotation.Pitch, IndexRotation.Yaw);
			index++;
		}
	}

	UpdateDebugLines();
	UpdateEulerLines();
	
	if (DeltaTime - LastReadingTime < 0.1 && LastReadingTime != DeltaTime) {
		return;
	}

	UpdateTracking();

	if (false) {
		int index = 0;
		for (UTauBuffer* TauBuffer : TriangleTauBuffers) {
			for (double IncrementalTau : TauBuffer->IncrementalTauSamples) {
				UE_LOG(LogTemp, Display,TEXT("Incremental: %i\t%f"), index, IncrementalTau);
			}
			index++;
		}
		index = 0;
		for (UTauBuffer* TauBuffer : TriangleTauBuffers) {
			for (double FullGestureTau : TauBuffer->FullGestureTauSamples) {
				UE_LOG(LogTemp, Display, TEXT("Full: %i\t%f"), index, FullGestureTau);
			}
			index++;
		}
	}

	if (false) {
		int index = 0;
		for (UTauBuffer* TauBuffer : TriangleTauBuffers) {
			for (double IncrementalTauDot : TauBuffer->IncrementalTauDotSamples) {
				UE_LOG(LogTemp, Display, TEXT("Incremental Tau Dot: %i\t%f"), index, IncrementalTauDot);
			}
			index++;
		}
		index = 0;
		for (UTauBuffer* TauBuffer : TriangleTauBuffers) {
			for (double FullGestureTauDot : TauBuffer->FullGestureTauDotSamples) {
				UE_LOG(LogTemp, Display, TEXT("Full Tau Dot: %i\t%f"), index, FullGestureTauDot);
			}
			index++;
		}
	}

	if (false) {
		int index = 0;
		int logIndex = 0;
		for (UTauBuffer* TauBuffer : TriangleTauBuffers) {
			for (double IncrementalTauDot : TauBuffer->IncrementalTauDotSmoothedDiffFromLastFrame) {
				if (logIndex > TauBuffer->IncrementalTauDotSmoothedDiffFromLastFrame.Num() - TriangleTauBuffers.Num()) {
					UE_LOG(LogTemp, Display, TEXT("Incremental Tau Dot Smoothed Diff: %i\t%f"), index, IncrementalTauDot);
				}
				logIndex++;
			}
			index++;
		}
		index = 0;
		logIndex = 0;
		for (UTauBuffer* TauBuffer : TriangleTauBuffers) {
			for (double FullGestureTauDot : TauBuffer->FullGestureTauDotSmoothedDiffFromLastFrame) {
				if (logIndex > TauBuffer->FullGestureTauDotSmoothedDiffFromLastFrame.Num() - TriangleTauBuffers.Num()) {
					UE_LOG(LogTemp, Display, TEXT("Full Tau Dot Smoothed Diff: %i\t%f"), index, FullGestureTauDot);
				}
			}
			index++;
		}
	}

	LastReadingTime = DeltaTime;
}

// Called to bind functionality to input
void AParticleGenerator::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
}

void AParticleGenerator::UpdateSocketRawData()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	SocketNames = MeshComponent->GetAllSocketNames();
	SocketBoneNames.Reset();
	SocketLocations.Reset();
	SocketRotations.Reset();
	std::vector<float> RawLocations = {};
	std::vector<float> RawRotations = {};
	for (auto& Name : SocketNames) {
		FName BoneName = Name;
		FRotator Rotation = MeshComponent->GetSocketRotation(Name);
		FVector Location = MeshComponent->GetSocketLocation(Name);

		SocketBoneNames.Emplace(BoneName);
		SocketRotations.Emplace(Rotation);
		SocketLocations.Emplace(Location);

		RawLocations.push_back(Location.X);
		RawLocations.push_back(Location.Y);
		RawLocations.push_back(Location.Z);

		RawRotations.push_back(Rotation.Roll);
		RawRotations.push_back(Rotation.Pitch);
		RawRotations.push_back(Rotation.Yaw);
	}

	LocationSamples.push_back(RawLocations);
	RotationSamples.push_back(RawRotations);

	if (LocationSamples.size() > 10)
	{
		LocationSamples.erase(LocationSamples.begin());
		LocationSamples.shrink_to_fit();
	}

	if (RotationSamples.size() > 10)
	{
		RotationSamples.erase(RotationSamples.begin());
		RotationSamples.shrink_to_fit();
	}
}

void AParticleGenerator::UpdateTriangles(TArray<FName>BoneNames, TArray<FVector>Locations, TArray<FRotator>Rotations)
{
	TriangleIndexBoneNames =
	{
		// Center Symmetrical	
		BoneNames[5],	// Head 
		BoneNames[55],	// Left Up Leg
		BoneNames[60],	// Right Up Leg

		BoneNames[5],	// Head
		BoneNames[58],	// Left Top Base
		BoneNames[63],	// Right Toe Base

		BoneNames[2],	// Spine 1
		BoneNames[7],	// Left Shoulder
		BoneNames[31],	// Right Shoulder

		BoneNames[2],	// Spine 1
		BoneNames[10],	// Left Hand
		BoneNames[34],	// Right Hand

		BoneNames[2],	// Spine 1
		BoneNames[55],	// Left Up Leg
		BoneNames[60],	// Right Up Leg

		// Right Side Head
		BoneNames[5],		// Head
		BoneNames[31],	// Right Shoulder 
		BoneNames[4],		// Neck

		BoneNames[5],		// Head
		BoneNames[31],	// Right Shoulder
		BoneNames[2],		// Spine 1

		BoneNames[5],		// Head
		BoneNames[32],	// Right Arm
		BoneNames[34],	// Right Hand 

		BoneNames[5],		// Head
		BoneNames[32],	// Right Arm 
		BoneNames[62],	// Right Foot 


		//Right Side Chest
		BoneNames[2],		// Spine 1
		BoneNames[31],	// Right Shoulder 
		BoneNames[4],		// Neck

		BoneNames[2],		// Spine 1 
		BoneNames[31],	// Right Shoulder 
		BoneNames[34],	// Right Hand

		BoneNames[2],		// Spine 1 
		BoneNames[60],	// Right Up Leg 
		BoneNames[62],	// Right Foot

		BoneNames[2],		// Spine 1 
		BoneNames[32],	// Right Arm
		BoneNames[34],	// Right Hand 

		// Right Side Hip
		BoneNames[60],	// Right Up Leg 
		BoneNames[31],	// Right Shoulder 
		BoneNames[55],	// Left Up Leg 

		BoneNames[60],	// Right Up Leg 
		BoneNames[31],	// Right Shoulder 
		BoneNames[32],	// Right Arm 

		BoneNames[60],	// Right Up Leg
		BoneNames[31],	// Right Shoulder 
		BoneNames[34],	// Right Hand 

		BoneNames[60],	// Right Up Leg
		BoneNames[4],		// Neck
		BoneNames[31],	// Right Shoulder

		BoneNames[60],	// Right Up Leg
		BoneNames[32],	// Right Arm
		BoneNames[34],	// Right Hand

		BoneNames[60],	// Right Up Leg
		BoneNames[61],	// Right Leg 
		BoneNames[56],	// Left Knee

		BoneNames[60],	// Right Up Leg
		BoneNames[61],	// Right Leg 
		BoneNames[62],	// Right Foot

		// Right Side Knee
		BoneNames[61],	// Right Leg 
		BoneNames[31],	// Right Shoulder
		BoneNames[32],	// Right Elbow

		BoneNames[61],	// Right Leg 
		BoneNames[31],	// Right Shoulder
		BoneNames[56],	// Left Leg

		BoneNames[61],	// Right Leg 
		BoneNames[60],	// Right Hip 
		BoneNames[55],	// Left Hip 

		BoneNames[61],	// Right Leg 
		BoneNames[62],	// Right Foot
		BoneNames[57],	// Left Foot

		// Right Side Ankle
		BoneNames[62],	// Right Foot 
		BoneNames[31],	// Right Shoulder
		BoneNames[7],		// Left Shoulder

		BoneNames[62],	// Right Foot 
		BoneNames[61],	// Right Leg
		BoneNames[56],	// Left Leg


		// Left Side Head
		BoneNames[5],		// Head
		BoneNames[7],		// Left Shoulder
		BoneNames[4],		// Neck

		BoneNames[5],		// Head
		BoneNames[7],		// Left Shoulder
		BoneNames[2],		// Spine 1

		BoneNames[5],		// Head
		BoneNames[8],		// Left Arm
		BoneNames[10],	// Left Hand

		BoneNames[5],		// Head
		BoneNames[8],		// Left Arm
		BoneNames[57],	// Left Foot


		//Left Side Chest
		BoneNames[2],		// Spine 1
		BoneNames[7],		// Left Shoulder
		BoneNames[8],		// Left Arm

		BoneNames[2],		// Spine 1
		BoneNames[7],		// Left Shoulder
		BoneNames[10],		// Left Hand

		BoneNames[2],		// Spine 1
		BoneNames[55],	// Left Leg Up
		BoneNames[57],	// Left Foot

		BoneNames[2],		// Spine 1
		BoneNames[8],		// Left Arm
		BoneNames[10],	// Left Hand

		// Left Side Hip
		BoneNames[55],	// Left Leg Up 
		BoneNames[7],		// Left Shoulder
		BoneNames[60],	// Right Leg Up

		BoneNames[55],	// Left Leg Up 
		BoneNames[7],		// Left Shoulder
		BoneNames[8],		// Left Arm

		BoneNames[55],	// Left Leg Up 
		BoneNames[7],		// Left Shoulder
		BoneNames[10],	// Left Hand

		BoneNames[55],	// Left Leg Up  
		BoneNames[4],		// Neck
		BoneNames[7],		// Left Shoulder

		BoneNames[55],	// Left Leg Up 
		BoneNames[8],		// Left Arm
		BoneNames[10],	// Left Hand

		BoneNames[55],	// Left Leg Up 
		BoneNames[56],	// Left Leg
		BoneNames[61],	// Right Leg

		BoneNames[55],	// Left Leg Up 
		BoneNames[56],	// Left Leg
		BoneNames[57],	// Left Foot 

		// Left Side Leg
		BoneNames[56],	// Left Leg
		BoneNames[7],		// Left Shoulder
		BoneNames[8],		// Left Arm

		BoneNames[56],	// Left Leg
		BoneNames[7],		// Left Shoulder 
		BoneNames[61],	// Right Leg

		BoneNames[56],	// Left Leg
		BoneNames[55],	// Left Leg Up
		BoneNames[60],	// Right Leg Up

		BoneNames[56],	// Left Leg
		BoneNames[57],	// Left Foot 
		BoneNames[62],	// Right Foot

		// Left Side Ankle
		BoneNames[57],	// Left Foot
		BoneNames[7],		// Left Shoulder
		BoneNames[31],	// Right Shoulder

		BoneNames[57],	// Left Foot
		BoneNames[56],	// Left Leg
		BoneNames[61],	// Right Leg

		// Cross Center
		BoneNames[5],		// Head
		BoneNames[32],	// Right Elbow
		BoneNames[57],	// Left Foot

		BoneNames[5],		// Head
		BoneNames[8],		// Left Elbow
		BoneNames[62],	// Right Foot

		BoneNames[5],		// Head
		BoneNames[32],	// Right Elbow
		BoneNames[10],	// Left Hand

		BoneNames[5],		// Head
		BoneNames[8],		// Left Elbow
		BoneNames[34],	// Right Hand

		BoneNames[2],		// Spine 1
		BoneNames[60],	// Right Hip
		BoneNames[10],	// Left Hand

		BoneNames[2],		// Spine 1
		BoneNames[55],	// Left Hip
		BoneNames[34],	// Right Hand

		BoneNames[2],		// Spine 1
		BoneNames[31],	// Right Shoulder
		BoneNames[10],	// Left Hand

		BoneNames[2],		// Spine 1
		BoneNames[7],		// Left Shoulder
		BoneNames[34],	// Right Hand

		BoneNames[60],	// Right Up Leg 
		BoneNames[7],		// Left Shoulder
		BoneNames[32],	// Right Arm

		BoneNames[55],	// Left Leg Up 
		BoneNames[31],	// Right Shoulder
		BoneNames[8],		// Left Arm

		BoneNames[60],	// Right Up Leg 
		BoneNames[7],		// Left Shoulder
		BoneNames[34],	// Right Hand


		BoneNames[55],	// Left Leg Up 
		BoneNames[31],	// Right Shoulder
		BoneNames[10],	// Left Hand

		BoneNames[60],	// Right Up Leg 
		BoneNames[8],		// Left Arm
		BoneNames[34],	// Right Hand

		BoneNames[55],	// Left Leg Up 
		BoneNames[32],	// Right Arm
		BoneNames[10],	// Left Hand

		BoneNames[60],	// Right Up Leg 
		BoneNames[56],	// Left Leg
		BoneNames[34],	// Right Hand

		BoneNames[55],	// Left Leg Up 
		BoneNames[61],	// Right Leg
		BoneNames[10],	// Left Hand


		BoneNames[60],	// Right Up Leg 
		BoneNames[57],	// Left Foot
		BoneNames[61],	// Right Leg

		BoneNames[55],	// Left Leg Up 
		BoneNames[62],	// Right Root
		BoneNames[56],	// Left Leg

		BoneNames[61],	// Right Leg
		BoneNames[7],	// Left Shoulder
		BoneNames[32],	// Right Arm

		BoneNames[56],	// Left Leg
		BoneNames[31],	// Right Shoulder
		BoneNames[8]	// Left Arm
	};

	PreviousTrianglePositions = TrianglePositions;
		TrianglePositions =
	{
		// Center Symmetrical	
		Locations[5],		// Head 
		Locations[55],	// Left Up Leg
		Locations[60],	// Right Up Leg

		Locations[5],		// Head
		Locations[58],	// Left Top Base
		Locations[63],	// Right Toe Base

		Locations[2],		// Spine 1
		Locations[7],		// Left Shoulder
		Locations[31],	// Right Shoulder

		Locations[2],		// Spine 1
		Locations[10],	// Left Hand
		Locations[34],	// Right Hand

		Locations[2],		// Spine 1
		Locations[55],	// Left Up Leg
		Locations[60],	// Right Up Leg

		// Right Side Head
		Locations[5],		// Head
		Locations[31],	// Right Shoulder 
		Locations[4],		// Neck

		Locations[5],		// Head
		Locations[31],	// Right Shoulder
		Locations[2],		// Spine 1

		Locations[5],		// Head
		Locations[32],	// Right Arm
		Locations[34],	// Right Hand 

		Locations[5],		// Head
		Locations[32],	// Right Arm 
		Locations[62],	// Right Foot 


		//Right Side Chest
		Locations[2],		// Spine 1
		Locations[31],	// Right Shoulder 
		Locations[4],		// Neck

		Locations[2],		// Spine 1 
		Locations[31],	// Right Shoulder 
		Locations[34],	// Right Hand

		Locations[2],		// Spine 1 
		Locations[60],	// Right Up Leg 
		Locations[62],	// Right Foot

		Locations[2],		// Spine 1 
		Locations[32],	// Right Arm
		Locations[34],	// Right Hand 

		// Right Side Hip
		Locations[60],	// Right Up Leg 
		Locations[31],	// Right Shoulder 
		Locations[55],	// Left Up Leg 

		Locations[60],	// Right Up Leg 
		Locations[31],	// Right Shoulder 
		Locations[32],	// Right Arm 

		Locations[60],	// Right Up Leg
		Locations[31],	// Right Shoulder 
		Locations[34],	// Right Hand 

		Locations[60],	// Right Up Leg
		Locations[4],		// Neck
		Locations[31],	// Right Shoulder

		Locations[60],	// Right Up Leg
		Locations[32],	// Right Arm
		Locations[34],	// Right Hand

		Locations[60],	// Right Up Leg
		Locations[61],	// Right Leg 
		Locations[56],	// Left Knee

		Locations[60],	// Right Up Leg
		Locations[61],	// Right Leg 
		Locations[62],	// Right Foot

		// Right Side Knee
		Locations[61],	// Right Up Leg 
		Locations[31],	// Right Shoulder
		Locations[32],	// Right Elbow

		Locations[61],	// Right Up Leg 
		Locations[31],	// Right Shoulder
		Locations[56],	// Right Leg

		Locations[61],	// Right Up Leg 
		Locations[60],	// Right Hip 
		Locations[55],	// Left Hip 

		Locations[61],	// Right Up Leg 
		Locations[62],	// Right Foot
		Locations[57],	// Left Foot

		// Right Side Ankle
		Locations[62],	// Right Foot 
		Locations[31],	// Right Shoulder
		Locations[7],		// Left Shoulder

		Locations[62],	// Right Foot 
		Locations[61],	// Right Leg
		Locations[56],	// Left Leg


		// Left Side Head
		Locations[5],		// Head
		Locations[7],		// Left Shoulder
		Locations[4],		// Neck

		Locations[5],		// Head
		Locations[7],		// Left Shoulder
		Locations[2],		// Spine 1

		Locations[5],		// Head
		Locations[8],		// Left Arm
		Locations[10],	// Left Hand

		Locations[5],		// Head
		Locations[8],		// Left Arm
		Locations[57],	// Left Foot


		//Left Side Chest
		Locations[2],		// Spine 1
		Locations[7],		// Left Shoulder
		Locations[8],		// Left Arm

		Locations[2],		// Spine 1
		Locations[7],		// Left Shoulder
		Locations[10],		// Left Hand

		Locations[2],		// Spine 1
		Locations[55],	// Left Leg Up
		Locations[57],	// Left Foot

		Locations[2],		// Spine 1
		Locations[8],		// Left Arm
		Locations[10],	// Left Hand

		// Left Side Hip
		Locations[55],	// Left Leg Up 
		Locations[7],		// Left Shoulder
		Locations[60],	// Right Leg Up

		Locations[55],	// Left Leg Up 
		Locations[7],		// Left Shoulder
		Locations[8],		// Left Arm

		Locations[55],	// Left Leg Up 
		Locations[7],		// Left Shoulder
		Locations[10],	// Left Hand

		Locations[55],	// Left Leg Up  
		Locations[4],		// Neck
		Locations[7],		// Left Shoulder

		Locations[55],	// Left Leg Up 
		Locations[8],		// Left Arm
		Locations[10],	// Left Hand

		Locations[55],	// Left Leg Up 
		Locations[56],	// Left Leg
		Locations[61],	// Right Leg

		Locations[55],	// Left Leg Up 
		Locations[56],	// Left Leg
		Locations[57],	// Left Foot 

		// Left Side Leg
		Locations[56],	// Left Leg
		Locations[7],		// Left Shoulder
		Locations[8],		// Left Arm

		Locations[56],	// Left Leg
		Locations[7],		// Left Shoulder 
		Locations[61],	// Right Leg

		Locations[56],	// Left Leg
		Locations[55],	// Left Leg Up
		Locations[60],	// Right Leg Up

		Locations[56],	// Left Leg
		Locations[57],	// Left Foot 
		Locations[62],	// Right Foot

		// Left Side Ankle
		Locations[57],	// Left Foot
		Locations[7],		// Left Shoulder
		Locations[31],	// Right Shoulder

		Locations[57],	// Left Foot
		Locations[56],	// Left Leg
		Locations[61],	// Right Leg

		// Cross Center
		Locations[5],		// Head
		Locations[32],	// Right Elbow
		Locations[57],	// Left Foot

		Locations[5],		// Head
		Locations[8],		// Left Elbow
		Locations[62],	// Right Foot

		Locations[5],		// Head
		Locations[33],	// Right Elbow
		Locations[10],	// Left Hand

		Locations[5],		// Head
		Locations[8],		// Left Elbow
		Locations[34],	// Right Hand

		Locations[2],		// Spine 1
		Locations[60],	// Right Hip
		Locations[10],	// Left Hand

		Locations[2],		// Spine 1
		Locations[55],	// Left Hip
		Locations[34],	// Right Hand

		Locations[2],		// Spine 1
		Locations[31],	// Right Shoulder
		Locations[10],	// Left Hand

		Locations[2],		// Spine 1
		Locations[7],		// Left Shoulder
		Locations[34],	// Right Hand

		Locations[60],	// Right Up Leg 
		Locations[7],		// Left Shoulder
		Locations[32],	// Right Arm

		Locations[55],	// Left Leg Up 
		Locations[31],	// Right Shoulder
		Locations[8],		// Left Arm

		Locations[60],	// Right Up Leg 
		Locations[7],		// Left Shoulder
		Locations[34],	// Right Hand


		Locations[55],	// Left Leg Up 
		Locations[31],	// Right Shoulder
		Locations[10],	// Left Hand

		Locations[60],	// Right Up Leg 
		Locations[8],		// Left Arm
		Locations[34],	// Right Hand

		Locations[55],	// Left Leg Up 
		Locations[32],	// Right Arm
		Locations[10],	// Left Hand

		Locations[60],	// Right Up Leg 
		Locations[56],	// Left Leg
		Locations[34],	// Right Hand

		Locations[55],	// Left Leg Up 
		Locations[61],	// Right Leg
		Locations[10],	// Left Hand


		Locations[60],	// Right Up Leg 
		Locations[57],	// Left Foot
		Locations[61],	// Right Leg

		Locations[55],	// Left Leg Up 
		Locations[62],	// Right Root
		Locations[56],	// Left Leg

		Locations[61],	// Right Leg
		Locations[7],		// Left Shoulder
		Locations[32],	// Right Arm

		Locations[56],	// Left Leg
		Locations[31],	// Right Shoulder
		Locations[8]		// Left Arm
	};

	PreviousTriangleRotations = TriangleRotations;

	TriangleRotations = 
	{
		// Center Symmetrical	
		Rotations[5],		// Head 
		Rotations[55],	// Left Up Leg
		Rotations[60],	// Right Up Leg

		Rotations[5],		// Head
		Rotations[58],	// Left Top Base
		Rotations[63],	// Right Toe Base

		Rotations[2],		// Spine 1
		Rotations[7],		// Left Shoulder
		Rotations[31],	// Right Shoulder

		Rotations[2],		// Spine 1
		Rotations[10],	// Left Hand
		Rotations[34],	// Right Hand

		Rotations[2],		// Spine 1
		Rotations[55],	// Left Up Leg
		Rotations[60],	// Right Up Leg

		// Right Side Head
		Rotations[5],		// Head
		Rotations[31],	// Right Shoulder 
		Rotations[4],		// Neck

		Rotations[5],		// Head
		Rotations[31],	// Right Shoulder
		Rotations[2],		// Spine 1

		Rotations[5],		// Head
		Rotations[32],	// Right Arm
		Rotations[34],	// Right Hand 

		Rotations[5],		// Head
		Rotations[32],	// Right Arm 
		Rotations[62],	// Right Foot 


		//Right Side Chest
		Rotations[2],		// Spine 1
		Rotations[31],	// Right Shoulder 
		Rotations[4],		// Neck

		Rotations[2],		// Spine 1 
		Rotations[31],	// Right Shoulder 
		Rotations[34],	// Right Hand

		Rotations[2],		// Spine 1 
		Rotations[60],	// Right Up Leg 
		Rotations[62],	// Right Foot

		Rotations[2],		// Spine 1 
		Rotations[32],	// Right Arm
		Rotations[34],	// Right Hand 

		// Right Side Hip
		Rotations[60],	// Right Up Leg 
		Rotations[31],	// Right Shoulder 
		Rotations[55],	// Left Up Leg 

		Rotations[60],	// Right Up Leg 
		Rotations[31],	// Right Shoulder 
		Rotations[32],	// Right Arm 

		Rotations[60],	// Right Up Leg
		Rotations[31],	// Right Shoulder 
		Rotations[34],	// Right Hand 

		Rotations[60],	// Right Up Leg
		Rotations[4],		// Neck
		Rotations[31],	// Right Shoulder

		Rotations[60],	// Right Up Leg
		Rotations[32],	// Right Arm
		Rotations[34],	// Right Hand

		Rotations[60],	// Right Up Leg
		Rotations[61],	// Right Leg 
		Rotations[56],	// Left Knee

		Rotations[60],	// Right Up Leg
		Rotations[61],	// Right Leg 
		Rotations[62],	// Right Foot

		// Right Side Knee
		Rotations[61],	// Right Up Leg 
		Rotations[31],	// Right Shoulder
		Rotations[32],	// Right Elbow

		Rotations[61],	// Right Up Leg 
		Rotations[31],	// Right Shoulder
		Rotations[56],	// Right Leg

		Rotations[61],	// Right Up Leg 
		Rotations[60],	// Right Hip 
		Rotations[55],	// Left Hip 

		Rotations[61],	// Right Up Leg 
		Rotations[62],	// Right Foot
		Rotations[57],	// Left Foot

		// Right Side Ankle
		Rotations[62],	// Right Foot 
		Rotations[31],	// Right Shoulder
		Rotations[7],		// Left Shoulder

		Rotations[62],	// Right Foot 
		Rotations[61],	// Right Leg
		Rotations[56],	// Left Leg


		// Left Side Head
		Rotations[5],		// Head
		Rotations[7],		// Left Shoulder
		Rotations[4],		// Neck

		Rotations[5],		// Head
		Rotations[7],		// Left Shoulder
		Rotations[2],		// Spine 1

		Rotations[5],		// Head
		Rotations[8],		// Left Arm
		Rotations[10],	// Left Hand

		Rotations[5],		// Head
		Rotations[8],		// Left Arm
		Rotations[57],	// Left Foot


		//Left Side Chest
		Rotations[2],		// Spine 1
		Rotations[7],		// Left Shoulder
		Rotations[8],		// Left Arm

		Rotations[2],		// Spine 1
		Rotations[7],		// Left Shoulder
		Rotations[10],		// Left Hand

		Rotations[2],		// Spine 1
		Rotations[55],	// Left Leg Up
		Rotations[57],	// Left Foot

		Rotations[2],		// Spine 1
		Rotations[8],		// Left Arm
		Rotations[10],	// Left Hand

		// Left Side Hip
		Rotations[55],	// Left Leg Up 
		Rotations[7],		// Left Shoulder
		Rotations[60],	// Right Leg Up

		Rotations[55],	// Left Leg Up 
		Rotations[7],		// Left Shoulder
		Rotations[8],		// Left Arm

		Rotations[55],	// Left Leg Up 
		Rotations[7],		// Left Shoulder
		Rotations[10],	// Left Hand

		Rotations[55],	// Left Leg Up  
		Rotations[4],		// Neck
		Rotations[7],		// Left Shoulder

		Rotations[55],	// Left Leg Up 
		Rotations[8],		// Left Arm
		Rotations[10],	// Left Hand

		Rotations[55],	// Left Leg Up 
		Rotations[56],	// Left Leg
		Rotations[61],	// Right Leg

		Rotations[55],	// Left Leg Up 
		Rotations[56],	// Left Leg
		Rotations[57],	// Left Foot 

		// Left Side Leg
		Rotations[56],	// Left Leg
		Rotations[7],		// Left Shoulder
		Rotations[8],		// Left Arm

		Rotations[56],	// Left Leg
		Rotations[7],		// Left Shoulder 
		Rotations[61],	// Right Leg

		Rotations[56],	// Left Leg
		Rotations[55],	// Left Leg Up
		Rotations[60],	// Right Leg Up

		Rotations[56],	// Left Leg
		Rotations[57],	// Left Foot 
		Rotations[62],	// Right Foot

		// Left Side Ankle
		Rotations[57],	// Left Foot
		Rotations[7],		// Left Shoulder
		Rotations[31],	// Right Shoulder

		Rotations[57],	// Left Foot
		Rotations[56],	// Left Leg
		Rotations[61],	// Right Leg

		// Cross Center
		Rotations[5],		// Head
		Rotations[32],	// Right Elbow
		Rotations[57],	// Left Foot

		Rotations[5],		// Head
		Rotations[8],		// Left Elbow
		Rotations[62],	// Right Foot

		Rotations[5],		// Head
		Rotations[33],	// Right Elbow
		Rotations[10],	// Left Hand

		Rotations[5],		// Head
		Rotations[8],		// Left Elbow
		Rotations[34],	// Right Hand

		Rotations[2],		// Spine 1
		Rotations[60],	// Right Hip
		Rotations[10],	// Left Hand

		Rotations[2],		// Spine 1
		Rotations[55],	// Left Hip
		Rotations[34],	// Right Hand

		Rotations[2],		// Spine 1
		Rotations[31],	// Right Shoulder
		Rotations[10],	// Left Hand

		Rotations[2],		// Spine 1
		Rotations[7],		// Left Shoulder
		Rotations[34],	// Right Hand

		Rotations[60],	// Right Up Leg 
		Rotations[7],		// Left Shoulder
		Rotations[32],	// Right Arm

		Rotations[55],	// Left Leg Up 
		Rotations[31],	// Right Shoulder
		Rotations[8],		// Left Arm

		Rotations[60],	// Right Up Leg 
		Rotations[7],		// Left Shoulder
		Rotations[34],	// Right Hand


		Rotations[55],	// Left Leg Up 
		Rotations[31],	// Right Shoulder
		Rotations[10],	// Left Hand

		Rotations[60],	// Right Up Leg 
		Rotations[8],		// Left Arm
		Rotations[34],	// Right Hand

		Rotations[55],	// Left Leg Up 
		Rotations[32],	// Right Arm
		Rotations[10],	// Left Hand

		Rotations[60],	// Right Up Leg 
		Rotations[56],	// Left Leg
		Rotations[34],	// Right Hand

		Rotations[55],	// Left Leg Up 
		Rotations[61],	// Right Leg
		Rotations[10],	// Left Hand


		Rotations[60],	// Right Up Leg 
		Rotations[57],	// Left Foot
		Rotations[61],	// Right Leg

		Rotations[55],	// Left Leg Up 
		Rotations[62],	// Right Root
		Rotations[56],	// Left Leg

		Rotations[61],	// Right Leg
		Rotations[7],		// Left Shoulder
		Rotations[32],	// Right Arm

		Rotations[56],	// Left Leg
		Rotations[31],	// Right Shoulder
		Rotations[8]		// Left Arm
	};
}

void AParticleGenerator::UpdateDebugLines()
{

	// Update debug vector arrays
	int TriangleCount = 0;
	for (int i = 0; i < TrianglePositions.Num() / 3; i++) {
		/*
		Directional vector is D1 normalized
		T is the midpoint of the side of the triangle
		<rx,ry,rz> -> slopes of the line / Parallel Directional Vector
		t is any parameter (real value) will give us a point on the line
		x = x0 + rx * t
		y = y0 + ry * t
		z = z0 + rz * t

		For AB
		x = ABmid.x + D1.x * t
		y = ABmid.y + D1.y * t
		z = ABmid.z + D1.z * t

		For BC
		x = BCmid.x + D2.x * s
		y = BCmid.y + D2.y * s
		z = BCmid.z + D2.z * s

		For CA
		x = CAmid.x + D3.x * q
		y = CAmid.y + D3.y * q
		z = CAmid.z + D3.z * q

		Set each component equal to each other

		ABBC
		ABmid.x + D1.x * t = BCmid.x + D2.x * s
		ABmid.y + D1.y * t = BCmid.y + D2.y * s
		ABmid.z + D1.z * t = BCmid.z + D2.z * s

		BCCA
		BCmid.x + D2.x * s = CAmid.x + D3.x * q
		BCmid.y + D2.y * s = CAmid.y + D3.y * q
		BCmid.z + D2.z * s = CAmid.z + D3.z * q

		CAAB
		CAmid.x + D3.x * q = ABmid.x + D1.x * t
		CAmid.y + D3.y * q = ABmid.y + D1.y * t
		CAmid.z + D3.z * q = ABmid.z + D1.z * t

		Solve for s
		s = ( ABmid.x + D1.x * t - BCmid.x ) / ( D2.x )
		Subsititute into the y equation
		ABmid.y + D1.y * t = BCmid.y + D2.y * ( ( ABmid.x + D1.x * t - BCmid.x ) / ( D2.x ) )    // Substitution
		D1.y * t - D2.y * ( ( ABmid.x + D1.x * t - BCmid.x ) / ( D2.x ) ) = BCmid.y - ABmid.y    // Bring T to the same side
		( D1.y * t ) - ( D2.y * ABmid.x + D2.y * D1.x * t - D2.y * BCmid.x ) / ( D2.x ) = ( BCmid.y - ABmid.y ) // Expand
		( D1.y * t ) * ( D2.x ) - ( D2.y * ABmid.x + D2.y * D1.x * t - D2.y * BCmid.x ) = ( BCmid.y - ABmid.y ) * ( D2.x ) // Multiply out the divisor
		( D1.y * t ) * ( D2.x ) - ( D2.y * D1.x * t ) =  ( BCmid.y - ABmid.y ) * ( D2.x ) + ( D2.y * ABmid.x ) - ( D2.y * BCmid.x )  // Add
		t ( D1.y * D2.x - D2.y * D1.x ) = ( BCmid.y - ABmid.y ) * ( D2.x ) + ( D2.y * ABmid.x ) - ( D2.y * BCmid.x )  // Factor
		t = ( ( BCmid.y - ABmid.y ) * ( D2.x ) + ( D2.y * ABmid.x ) - ( D2.y * BCmid.x ) ) / ( D1.y * D2.x - D2.y * D1.x )  // Divide

		PVector ABBCx = new PVector();

		ABBCx.x = ABmid.x + D1.x * ( ( ( BCmid.y - ABmid.y ) * ( D2.x ) + ( D2.y * ABmid.x ) - ( D2.y * BCmid.x ) ) / ( D1.y * D2.x - D2.y * D1.x ) );
		ABBCx.y = ABmid.y + D1.y * ( ( ( BCmid.y - ABmid.y ) * ( D2.x ) + ( D2.y * ABmid.x ) - ( D2.y * BCmid.x ) ) / ( D1.y * D2.x - D2.y * D1.x ) );
		ABBCx.z = ABmid.z + D1.z * ( ( ( BCmid.y - ABmid.y ) * ( D2.x ) + ( D2.y * ABmid.x ) - ( D2.y * BCmid.x ) ) / ( D1.y * D2.x - D2.y * D1.x ) );
		*/

		int StartIndex = TrianglePositions.Num() - 201;

		FVector A(TrianglePositions[StartIndex + TriangleCount * 3].X, TrianglePositions[StartIndex + TriangleCount * 3].Y, TrianglePositions[StartIndex + TriangleCount * 3].Z);
		FVector B(TrianglePositions[StartIndex + TriangleCount * 3 + 1].X, TrianglePositions[StartIndex + TriangleCount * 3 + 1].Y, TrianglePositions[StartIndex + TriangleCount * 3 + 1].Z);
		FVector C(TrianglePositions[StartIndex + TriangleCount * 3 + 2].X, TrianglePositions[StartIndex + TriangleCount * 3 + 2].Y, TrianglePositions[StartIndex + TriangleCount * 3 + 2].Z);

		FVector _AB = A - B;
		FVector _ABmid((A.X + B.X) / 2, (A.Y + B.Y) / 2, (A.Z + B.Z) / 2);
		FVector _BC = B - C;
		FVector _BCmid((B.X + C.X) / 2, (B.Y + C.Y) / 2, (B.Z + C.Z) / 2);
		FVector _CA = C - A;
		FVector _CAmid((C.X + A.X) / 2, (C.Y + A.Y) / 2, (C.Z + A.Z) / 2);

		FVector _V = FVector::CrossProduct(_AB, _BC);
		FVector _D1 = FVector::CrossProduct(_V, _AB);
		FVector _D2 = FVector::CrossProduct(_V, _BC);
		FVector _D3 = FVector::CrossProduct(_V, _CA);

		_V.Normalize();

		_V = _V * 50;

		_D1.Normalize();
		_D2.Normalize();
		_D3.Normalize();

		_D1 = _D1 * 150;
		_D2 = _D2 * 150;
		_D3 = _D3 * 150;

		AB.Emplace(_AB);
		ABmid.Emplace(_ABmid);
		BC.Emplace(_BC);
		BCmid.Emplace(_BCmid);
		CA.Emplace(_CA);
		CAmid.Emplace(_CAmid);

		V.Emplace(_V);
		D1.Emplace(_D1);
		D2.Emplace(_D2);
		D3.Emplace(_D3);

		float CircumcenterX = _ABmid.X + _D1.X * (((_BCmid.Y - _ABmid.Y) * (_D2.X) + (_D2.Y * _ABmid.X) - (_D2.Y * _BCmid.X)) / (_D1.Y * _D2.X - _D2.Y * _D1.X));
		float CircumcenterY = _ABmid.Y + _D1.Y * (((_BCmid.Y - _ABmid.Y) * (_D2.X) + (_D2.Y * _ABmid.X) - (_D2.Y * _BCmid.X)) / (_D1.Y * _D2.X - _D2.Y * _D1.X));
		float CircumcenterZ = _ABmid.Z + _D1.Z * (((_BCmid.Y - _ABmid.Y) * (_D2.X) + (_D2.Y * _ABmid.X) - (_D2.Y * _BCmid.X)) / (_D1.Y * _D2.X - _D2.Y * _D1.X));
		FVector _ABBC = FVector(CircumcenterX, CircumcenterY, CircumcenterZ);
		ABBC.Emplace(_ABBC);
		TriangleCount++;
	}

	if (AB.Num() >  TriangleCount) {
		for (int i = 0; i < AB.Num() - TriangleCount; i++) {
			AB.RemoveAt(i);
			ABmid.RemoveAt(i);
			BC.RemoveAt(i);
			BCmid.RemoveAt(i);
			CA.RemoveAt(i);
			CAmid.RemoveAt(i);

			V.RemoveAt(i);
			D1.RemoveAt(i);
			D2.RemoveAt(i);
			D3.RemoveAt(i);
			ABBC.RemoveAt(i);
		}
	}
}

void AParticleGenerator::UpdateEulerLines()
{
	EulerLines.Reset(0);
	TriangleCircumcenters.Reset(0);
	TriangleCentroids.Reset(0);
	// Update debug vector arrays
	int TriangleCount = 0;
	for (int i = 0; i < TrianglePositions.Num() / 3; i++) {

		int StartIndex = TrianglePositions.Num() - 201;

		FVector A(TrianglePositions[StartIndex + TriangleCount * 3].X, TrianglePositions[StartIndex + TriangleCount * 3].Y, TrianglePositions[StartIndex + TriangleCount * 3].Z);
		FVector B(TrianglePositions[StartIndex + TriangleCount * 3 + 1].X, TrianglePositions[StartIndex + TriangleCount * 3 + 1].Y, TrianglePositions[StartIndex + TriangleCount * 3 + 1].Z);
		FVector C(TrianglePositions[StartIndex + TriangleCount * 3 + 2].X, TrianglePositions[StartIndex + TriangleCount * 3 + 2].Y, TrianglePositions[StartIndex + TriangleCount * 3 + 2].Z);

		FVector Centroid((A.X + B.X + C.X) / 3, (A.Y + B.Y + C.Y) / 3, (A.Z + B.Z + C.Z) / 3);
		double _a[3] = {double(A.X), double(A.Y), double(A.Z)};
		double _b[3] = { double(B.X), double(B.Y), double(B.Z) };
		double _c[3] = { double(C.X), double(C.Y), double(C.Z) };
		double _result[3] = { 0,0,0 };
		TriCircumCenter3D(_a, _b, _c, _result);
		FVector Circumcenter = FVector(_result[0], _result[1], _result[2]);
		FVector Final = Centroid - Circumcenter;
		TriangleCentroids.Emplace(Centroid);
		TriangleCircumcenters.Emplace(Circumcenter);
		EulerLines.Emplace(Final);
		TriangleCount++;
	}

	//UE_LOG(LogTemp, Display, TEXT("Euler Lines Created"));
}

void AParticleGenerator::UpdateTracking()
{
	if ( TriangleTauBuffers.Num() == 0) {
		int TriangleCount = 0;
		for (int i = 0; i < TriangleIndexBoneNames.Num() / 3; i++) {
			FString Base = TriangleIndexBoneNames[TriangleCount * 3].ToString().Append(TriangleIndexBoneNames[TriangleCount * 3 + 1].ToString()).Append(TriangleIndexBoneNames[TriangleCount * 3 + 2].ToString());
			FName Name = FName(*Base);
			//UE_LOG(LogTemp, Display, TEXT("Tracking tau for %i"), TriangleCount);
			std::ostringstream s;
			s << TriangleCount;
			std::string CountIndex = s.str();
			std::string SName(TCHAR_TO_UTF8(*Name.ToString()));
			std::string BufferName = "Triangle Tau Buffer " + CountIndex + SName;
			FString FBufferName(BufferName.c_str());
			FName FinalName = FName(*FBufferName);
			UE_LOG(LogTemp, Display, TEXT("%s"), *FinalName.ToString());
			int StartIndex = TrianglePositions.Num() - 201;
			FVector A(TrianglePositions[StartIndex + TriangleCount * 3].X, TrianglePositions[StartIndex + TriangleCount * 3].Y, TrianglePositions[StartIndex + TriangleCount * 3].Z);
			FVector Circumcenter = TriangleCircumcenters[i];
			FVector Radius = Circumcenter - A;
			FVector EulerLine = EulerLines[i];
			UTauBuffer* TriangleBuffer = NewObject<UTauBuffer>(this, UTauBuffer::StaticClass(), FinalName);
			TriangleBuffer->RegisterComponent();
			TriangleBuffer->CurrentTime = FApp::GetCurrentTime();
			TriangleBuffer->BeginningTime = FApp::GetCurrentTime();
			TriangleBuffer->IsGrowing = false;
			TriangleBuffer->MeasuringStick = Radius;
			TriangleBuffer->BeginningPosition = FVector4(EulerLine.X, EulerLine.Y, EulerLine.Z, 0.0);
			TriangleBuffer->MotionPath.Emplace(TriangleBuffer->BeginningPosition);
			TriangleTauBuffers.Emplace(TriangleBuffer);
			TriangleCount++;
			
		}
		return;
	}

	int TriangleCount = 0;
	for (int i = 0; i < TriangleTauBuffers.Num(); i++) {
		FName Name = TriangleIndexBoneNames[TriangleCount * 3];
		//UE_LOG(LogTemp, Display, TEXT("Tracking tau for %i"), TriangleCount);
		UTauBuffer* Buffer = TriangleTauBuffers[TriangleCount];
		//UE_LOG(LogTemp, Display, TEXT("%s"), *Buffer->GetFName().ToString());
		Buffer->CurrentTime = FApp::GetCurrentTime();
		Buffer->ElapsedSinceLastReadingTime = Buffer->CurrentTime - Buffer->LastReadingTime;
		Buffer->ElapsedSinceBeginningGestureTime = Buffer->CurrentTime - Buffer->BeginningTime;
		FVector EulerLine = EulerLines[i];
		Buffer->MotionPath.Emplace(FVector4(EulerLine.X, EulerLine.Y, EulerLine.Z, Buffer->CurrentTime));
		Buffer->EndingPosition = FVector4(EulerLine.X, EulerLine.Y, EulerLine.Z, Buffer->CurrentTime);
		int StartIndex = TrianglePositions.Num() - 201;
		FVector A(TrianglePositions[StartIndex + TriangleCount * 3].X, TrianglePositions[StartIndex + TriangleCount * 3].Y, TrianglePositions[StartIndex + TriangleCount * 3].Z);
		Buffer->CalculateIncrementalGestureChange();
		Buffer->CalculateFullGestureChange();
		Buffer->LastReadingTime = Buffer->CurrentTime;
		if (Buffer->IncrementalTauSamples.Num() > SmoothingSamplesCount)
		{
			Buffer->IncrementalTauSamples.RemoveAt(0);
		}
		if (Buffer->FullGestureTauSamples.Num() > SmoothingSamplesCount)
		{
			Buffer->FullGestureTauSamples.RemoveAt(0);
		}
		if (Buffer->IncrementalTauDotSamples.Num() > SmoothingSamplesCount)
		{
			Buffer->IncrementalTauDotSamples.RemoveAt(0);
		}
		if (Buffer->FullGestureTauDotSamples.Num() > SmoothingSamplesCount)
		{
			Buffer->FullGestureTauDotSamples.RemoveAt(0);
		}
		if (Buffer->IncrementalTauDotSmoothedDiffFromLastFrame.Num() > SmoothingSamplesCount)
		{
			Buffer->IncrementalTauDotSmoothedDiffFromLastFrame.RemoveAt(0);
		}
		if( Buffer->FullGestureTauDotSmoothedDiffFromLastFrame.Num() > SmoothingSamplesCount ) 
		{
			Buffer->FullGestureTauDotSmoothedDiffFromLastFrame.RemoveAt(0);
		}

		if (true) {
			int index = 0;
			for (double Sample : Buffer->IncrementalTauSamples)
			{
				//UE_LOG(LogTemp, Display, TEXT("Tau Samples: %i\t%f"), index, Sample);
			}
			index = 0;
			for (double Sample : Buffer->IncrementalTauDotSamples)
			{
				//UE_LOG(LogTemp, Display, TEXT("Tau Dot Samples: %i\t%f"), index, Sample);
			}
			index = 0;
			for (double Sample : Buffer->IncrementalTauDotSmoothedDiffFromLastFrame)
			{
				//UE_LOG(LogTemp, Display, TEXT("Tau Dot Smoothed Diff: %i\t%f"), index, Sample);
			}
		}

		TriangleCount++;
	}



}