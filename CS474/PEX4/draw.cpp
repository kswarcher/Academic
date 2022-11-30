//define SQUARE
//#define TRI
#define FLAT
//#define MODEL

#include "draw.h"

struct Vertex {
	vec4f p;
	vec4f n;
	vec4f c;
};

struct ScanLine { // define a scanline
	vec4f	start;
	vec4f	end;
	vec4f	current;

	float	deltaX;
	float	deltaZ;

	vec4f currentColor;
	vec4f startColor;
	vec4f deltaColor;

	vec4f startNormal;
	vec4f currentNormal;
	vec4f deltaNormal;

}; // End Struct

std::vector<mat4f> matStack;
std::vector<mat4f> matNormalStack;

buf2d zBuf;
int imgWidth;
int imgHeight;

float thresh = 0.1;

std::vector<vec4f> rotatedLightDirs;


mat4f normMult(mat4f left, mat4f right)
{
	mat4f result;

	result = left * right;

	float K0, K1, K2;
	K0 = 1 / sqrt(pow(result[0][0], 2) + pow(result[0][1], 2) + pow(result[0][2], 2));
	K1 = 1 / sqrt(pow(result[1][0], 2) + pow(result[1][1], 2) + pow(result[1][2], 2));
	K2 = 1 / sqrt(pow(result[2][0], 2) + pow(result[2][1], 2) + pow(result[2][2], 2));


	result[0][0] = K0 * result[0][0]; result[0][1] = K0 * result[0][1]; result[0][2] = K0 * result[0][2]; result[3][0] = result[3][0];
	result[1][0] = K1 * result[1][0]; result[1][1] = K1 * result[1][1]; result[1][2] = K1 * result[1][2]; result[3][1] = result[3][1];
	result[2][0] = K2 * result[2][0]; result[2][1] = K2 * result[2][1]; result[2][2] = K2 * result[2][2]; result[3][2] = result[3][2];
	result[3][0] = 0.0; result[3][1] = 0.0; result[3][2] = 0.0; result[3][3] = 1.0;

	return result;
}

void swap(Vertex& left, Vertex& right)
{
	Vertex temp;
	temp = left;
	left = right;
	right = temp;
}

bool clip(Vertex& one, Vertex& two, Vertex& three)
{
	if ((one.p.x < 0 && two.p.x < 0 && three.p.x < 0) ||
		(one.p.x > imgWidth && two.p.x > imgWidth && three.p.x > imgWidth) ||
		(one.p.y < 0 && two.p.y < 0 && three.p.y < 0) ||
		(one.p.y > imgHeight && two.p.y > imgHeight && three.p.y > imgHeight))
		return true;
	return false;
}

bool sortTriangles(Vertex& one, Vertex& two, Vertex& three)
{
	bool longIsLeft = true;
	float deltaX;
	float deltaY;
	float slope;
	float yIntercept;

	// Check for a horizontal line -- morph all the cases to ether:
	//  1---2  or   1
	//    3       2---3

	// Case 1:
	//    3          3
	//  1---2  and 2---1
	if (abs(one.p.y - two.p.y) < thresh && one.p.y > three.p.y)
	{
		longIsLeft = false;
		swap(one, three);
		if (three.p.x < two.p.x)
		{
			swap(two, three);
		}// End if
	}
	// Case 2:
	//  1---2  and 2---1
	//    3          3
	else if (abs(one.p.y - two.p.y) < thresh && one.p.y < three.p.y)
	{
		if (one.p.x > two.p.x)
		{
			swap(one, two);
		}// End if
	}
	// Case 3:
	//    1          1
	//  3---2  and 2---3
	else if (abs(two.p.y - three.p.y) < thresh && one.p.y < three.p.y)
	{
		longIsLeft = false;
		if (three.p.x < two.p.x)
		{
			swap(two, three);
		}// End if
	}
	// Case 4:
	//  3---2  and 2---3
	//    1          1
	else if (abs(two.p.y - three.p.y) < thresh && one.p.y > three.p.y)
	{
		if (three.p.x < two.p.x)
		{
			swap(one, three);
		}
		else {
			swap(two, three);

			swap(one, three);
		}// End if	
	}
	// Case 5:
	//  3---1  and 1---3
	//    2          2	
	else if (abs(one.p.y - three.p.y) < thresh && one.p.y < two.p.y)
	{
		swap(two, three);
		if (one.p.x > two.p.x)
		{
			swap(one, two);
		}// End if
	}
	// Case 6:
	//    2          2
	//  3---1  and 1---3
	else if (abs(one.p.y - three.p.y) < thresh && one.p.y > two.p.y)
	{
		longIsLeft = false;
		swap(one, two);
		if (three.p.x < two.p.x)
		{
			swap(two, three);
		}// End if

		 // Start handling the general triangle cases -- catch verticle lines and set delta's to 0.
	}
	// Sort along Y-axis -- This ensures that there are only 2 cases of the general triangle:
	//  1		      1
	//     2   or   2
	//   3             3
	else
	{
		if (one.p.y > two.p.y)
		{
			swap(one, two);

		}// End If
		if (one.p.y > three.p.y)
		{
			swap(one, three);
		}// End If
		if (two.p.y > three.p.y)
		{
			swap(two, three);
		}// End If


		 // find where the horizontal line from 2 intersects 1-3
		 //   uses equation of a line y = mx + b
		deltaX = three.p.x - one.p.x;
		deltaY = (three.p.y - one.p.y);
		slope = deltaY / deltaX;
		yIntercept = three.p.y - slope * three.p.x;

		// Set whether or not the "long" edge is on the left 
		if (deltaX != 0.00)
		{
			float temp = (two.p.y - yIntercept) / slope;
			longIsLeft = temp < two.p.x;
		}
		// catch the horizontal line case
		else {
			longIsLeft = one.p.x < two.p.x;
		}// End If
	}// End If


	return longIsLeft;
}

mat4f setupRHRCameraMat(Scene& scene, int width, int height, mat4f& normalMat)
{
	mat4f Xsp, Xpc, Xcw, result;

	normalMat = normalMat.Identity();
	result = result.Identity();

	// set up transfrom Xsp

	Xsp.SetAll(0.0);
	Xsp[0][0] = width / 2;	Xsp[0][1] = 0.0;		 Xsp[0][2] = 0.0;		Xsp[0][3] = width / 2;
	Xsp[1][0] = 0.0;		Xsp[1][1] = height / 2;  Xsp[1][2] = 0.0;		Xsp[1][3] = height / 2;
	Xsp[2][0] = 0.0;		Xsp[2][1] = 0.0;		 Xsp[2][2] = 1.0;		Xsp[2][3] = 0.0;
	Xsp[3][0] = 0.0;		Xsp[3][1] = 0.0;		 Xsp[3][2] = 0.0;		Xsp[3][3] = 1.0;

	// set up transform Xpc
	float aspect = width / height;
	float N = 0.01f;
	float F = 100.0f;
	float d = tan(scene.cameraFOV / 2.0f * (PI / 180.0f));
	float AR = aspect * d;


	Xpc.SetAll(0.0);
	Xpc[0][0] = 1.0 / AR;	Xpc[0][1] = 0.0;     Xpc[0][2] = 0.0;			     Xpc[0][3] = 0.0;
	Xpc[1][0] = 0.0;	    Xpc[1][1] = 1.0 / d; Xpc[1][2] = 0.0;			     Xpc[1][3] = 0.0;
	Xpc[2][0] = 0.0;	    Xpc[2][1] = 0.0;	 Xpc[2][2] = (N + F) / (F - N);  Xpc[2][3] = (2.0 * N * F) / (F - N);
	Xpc[3][0] = 0.0;	    Xpc[3][1] = 0.0;	 Xpc[3][2] = -1.0;		         Xpc[3][3] = 0.0;

	result = Xsp * Xpc;

	normalMat = normMult(Xpc, normalMat);

	// set up world to camera
	vec4f  z, y, x, worldUp, eye;

	worldUp = scene.cameraUp;
	worldUp.Normalize();

	eye = scene.cameraLocation;
	z = scene.cameraLocation - scene.cameraLookAt;
	z.Normalize();

	x = worldUp ^ z;
	x.Normalize();

	y = z ^ x; // worldUp -worldUp.dot(n) * n;
	y.Normalize();

	Xcw.SetAll(0.0);
	Xcw[0][0] = x[0]; Xcw[0][1] = x[1]; Xcw[0][2] = x[2]; Xcw[0][3] = -x.dot(scene.cameraLocation);
	Xcw[1][0] = y[0]; Xcw[1][1] = y[1]; Xcw[1][2] = y[2]; Xcw[1][3] = -y.dot(scene.cameraLocation);
	Xcw[2][0] = z[0]; Xcw[2][1] = z[1]; Xcw[2][2] = z[2]; Xcw[2][3] = -z.dot(scene.cameraLocation);
	Xcw[3][0] = 0.0;  Xcw[3][1] = 0.0;  Xcw[3][2] = 0.0;  Xcw[3][3] = 1.0;

	result = result * Xcw;
	normalMat = normMult(normalMat, Xcw);

	return result;
}

ScanLine setupScanLineY(Vertex from, Vertex to)
{
	ScanLine result;
	// Scanline from one to three (the long edge)
	result.current = from.p;
	result.start = from.p;
	result.end = to.p;

	result.startColor = from.c;
	result.currentColor = from.c;

	result.startNormal = from.n;
	result.currentNormal = from.n;

	// Catch the horizontal line cases
	if (to.p.y - from.p.y != 0.0)
	{
		result.deltaX = (to.p.x - from.p.x) / (to.p.y - from.p.y);
		result.deltaZ = (to.p.z - from.p.z) / (to.p.y - from.p.y);
		result.deltaColor = (to.c - from.c) / (to.p.y - from.p.y);
		result.deltaNormal = (to.n - from.n) / (to.p.y - from.p.y);
	}
	else {
		result.deltaX = 0.0;
		result.deltaZ = 0.0;
		result.deltaColor = 0.0;
		result.deltaNormal = 0.0;
	}// End If

	return result;
}

ScanLine setupScanLineX(ScanLine left, ScanLine right)
{
	ScanLine result;

	result.current = left.current;
	result.start = left.current;
	result.end = right.current;
	result.currentColor = left.currentColor;
	result.currentNormal = left.currentNormal;

	// Catch the cases where there is no change in Z
	if (right.current.z == left.current.z)
	{
		result.deltaZ = 0.0;
	}
	else {
		result.deltaZ = (right.current.z - left.current.z) / (right.current.x - left.current.x);
	}// End if

	result.deltaColor = (right.currentColor - left.currentColor) / (right.current.x - left.current.x);
	result.deltaNormal = (right.currentNormal - left.currentNormal) / (right.current.x - left.current.x);

	return result;
}

void fillScanLineFlat(ScanLine leftRight, vec4f color, HDC& img, Scene scene)
{

	vec4f ambientL = scene.ambientLight;
	vec4f ambientReflec = scene.ambientReflection;
	vec4f diffuseReflec = scene.diffuseReflection;
	diffuseReflec.Normalize();
	vec4f specularReflec = scene.specularReflection;
	vec4f lightDirection;
	vec4f lightColor;
	//vec4f materialColor = 
	float specularRough = scene.specularRoughness;
	float ambient = ambientReflec.dot(ambientL);
	//float diffuse;
	float lightIntensity;
	vec4f H;
	vec4f normalVal;
	//float specular;
	vec4f specular;
	vec4f diffuse;
	float difSpecLight;

	lightIntensity = ambient;

	// Scan along the x pixel line, include the last one if it falls on the line.
	while (leftRight.current.x <= (int)leftRight.end.x)
	{

		int x, y;
		//float r, g, b, a;
		float z;

		x = static_cast<int> (leftRight.current.x);
		y = static_cast<int> (leftRight.current.y);

		vec4f tempColor = leftRight.currentColor;

		

		if (x > 0 && x < imgWidth && y > 0 && y < imgHeight)
		{
			z = zBuf[y][x];

			// If the current triangle is closer then set the right color
			//if (z > leftRight.current.z) // LHR			
			if (z < leftRight.current.z) // RHR			
			{
				diffuse = 0;
				specular = 0;

				normalVal = leftRight.currentNormal;

				

				normalVal.Normalize();
				normalVal.w = 1.0;

				for (int i = 0; i < scene.lightDirections.size(); i++) {
					lightDirection = scene.lightDirections[i];
					lightDirection.Normalize();
					lightDirection.w = 1.0;
					lightColor = scene.lightColors[i];

					H = lightDirection / 2;

					specular = (specularReflec * (pow(normalVal.dot(H),specularRough)));
					specular.Normalize();


					//if (normalVal.dot(lightDirection) < 0) {
						//diffuse = (lightColor * (diffuseReflec * (0))) + diffuse;
					//}
					//else {
						diffuse = ((diffuseReflec * (normalVal.dot(lightDirection)))) ;
						diffuse.Normalize();
					//}

						difSpecLight = lightColor * (specular + diffuse);
				}
				
				//tempColor.Normalize();
				tempColor = (tempColor * lightIntensity) + (tempColor * difSpecLight);
				tempColor.Normalize();
				tempColor.w = 1.0;

				tempColor *= 255;

				tempColor.x = (tempColor.x < 0 ? 0 : (tempColor.x > MAX_COLOR_VALUE ? MAX_COLOR_VALUE : tempColor.x));
				tempColor.y = (tempColor.y < 0 ? 0 : (tempColor.y > MAX_COLOR_VALUE ? MAX_COLOR_VALUE : tempColor.y));
				tempColor.z = (tempColor.z < 0 ? 0 : (tempColor.z > MAX_COLOR_VALUE ? MAX_COLOR_VALUE : tempColor.z));

				int r = (int)tempColor.x;
				int g = (int)tempColor.y;
				int b = (int)tempColor.z;

				SetPixelV(img, x, imgWidth - y, RGB(r, g, b));
				zBuf[y][x] = leftRight.current.z;
			}// End If
		}

		// Advance to the next pixel and interpolate Z
		leftRight.current.x += 1.0;
		leftRight.current.z += leftRight.deltaZ;
		leftRight.currentColor += leftRight.deltaColor;
		leftRight.currentNormal += leftRight.deltaNormal;
		leftRight.currentNormal.Normalize();

	}// End Loop
}

void draw(int width, int height, HDC& img, Scene scene)
{

	vec4f ambientL = scene.ambientLight;
	vec4f ambientReflec = scene.ambientReflection;
	vec4f diffuseReflec = scene.diffuseReflection;
	vec4f specularReflec = scene.specularReflection;
	float specularRough = scene.specularRoughness;
	float ambient = ambientReflec.dot(ambientL);
	vec4f lightIntensity;

	//vec4f direction0 = scene.lightDirections[0];
	//vec4f direction1 = scene.lightDirections[1];
	//vec4f direction2 = scene.lightDirections[2];
	//vec4f direction3 = scene.lightDirections[3];
	//vec4f direction4 = scene.lightDirections[4];
	//vec4f lightColor0 = scene.lightColors[0];
	//vec4f lightColor1 = scene.lightColors[1];
	//vec4f lightColor2 = scene.lightColors[2];
	//vec4f lightColor3 = scene.lightColors[3];
	//vec4f lightColor4 = scene.lightColors[4];
	
	//vec4f diffuseIntensity = 
	//ambient.Normalize();


	mat4f transMat, normTransMat;
	transMat = setupRHRCameraMat(scene, width, height, normTransMat);

	for (int i = 0; i < scene.lightDirections.size(); i++)
	{
		vec4f temp = normTransMat * scene.lightDirections[i];
		rotatedLightDirs.push_back(temp);
	}

	zBuf.init(width, height, -1e25f);  //RHR
	imgHeight = height;
	imgWidth = width;

	Vertex vOne, vTwo, vThree;
	int count = 0;
	for (int i = 0; i < scene.model.face.size(); i++)
	{
		vOne.p = scene.model.vertex[scene.model.face[i][0]];
		vTwo.p = scene.model.vertex[scene.model.face[i][1]];
		vThree.p = scene.model.vertex[scene.model.face[i][2]];

		vOne.p = transMat * vOne.p;
		vOne.p /= vOne.p.w;

		vTwo.p = transMat * vTwo.p;
		vTwo.p /= vTwo.p.w;

		vThree.p = transMat * vThree.p;
		vThree.p /= vThree.p.w;

		vOne.c = 0.75;
		vTwo.c = 0.75;
		vThree.c = 0.75;

		vOne.c = scene.model.faceColor[scene.model.face[i][0]];
		//vOne.c = vOne.c * ambient;
		//vOne.c = vOne.c.dot(ambient);
		//vOne.c = ambient*vOne.c;
		//vOne.c.Normalize();

		//vOne.c /= vOne.c.w;
		//vOne.c.w = 1.0;
		

		vTwo.c = scene.model.faceColor[scene.model.face[i][1]];
		//vTwo.c = vTwo.c * ambient;
		//vTwo.c = ambient*vTwo.c;
		//vTwo.c /= vTwo.c.w;
		//vTwo.c.Normalize();
		//vTwo.c.w = 1.0;
		

		vThree.c = scene.model.faceColor[scene.model.face[i][2]];
		//vThree.c = vThree.c * ambient;
		//vThree.c = ambient*vThree.c;
		//vThree.c /= vThree.c.w;
		//vThree.c.Normalize();
		//vThree.c.w = 1.0;
		

		bool longIsLeft = sortTriangles(vOne, vTwo, vThree);

		ScanLine oneTwo, oneThree, twoThree;

		// Scanline from one to three (the long edge)
		oneThree = setupScanLineY(vOne, vThree);

		// Scanline from one to two 
		oneTwo = setupScanLineY(vOne, vTwo);

		// Scanline from two to three 
		twoThree = setupScanLineY(vTwo, vThree);

		// Advance 1-3 and 1-2 to first pixel inside the triangle
		//deltaY = ceil(one.p.y) - one.p.y;

		// Interpolate the x and z values make sure we only do right and bottom values.
		//if (deltaY > 0.0)
		//{
		//	oneThree.current.x += oneThree.deltaX * deltaY;
		//	oneThree.current.y += deltaY;
		//	oneThree.current.z += oneThree.deltaZ * deltaY;
		//	oneThree.currentColor += oneThree.deltaColor * deltaY;
		//	oneThree.currentNormal += oneThree.deltaNormal * deltaY;
		//	oneThree.currentNormal.Normalize();

		//	oneTwo.current.x += oneTwo.deltaX * deltaY;
		//	oneTwo.current.y += deltaY;
		//	oneTwo.current.z += oneTwo.deltaZ * deltaY;
		//	oneTwo.currentColor += oneTwo.deltaColor * deltaY;
		//	oneTwo.currentNormal += oneTwo.deltaNormal * deltaY;
		//	oneTwo.currentNormal.Normalize();
		//}
		//else {
		//	oneThree.current.x += oneThree.deltaX;
		//	oneThree.current.y += 1.0;
		//	oneThree.current.z += oneThree.deltaZ;
		//	oneThree.currentColor += oneThree.deltaColor;
		//	oneThree.currentNormal += oneThree.deltaNormal;
		//	oneThree.currentNormal.Normalize();

		//	oneTwo.current.x += oneTwo.deltaX;
		//	oneTwo.current.y += 1.0;
		//	oneTwo.current.z += oneTwo.deltaZ;
		//	oneTwo.currentColor += oneTwo.deltaColor;
		//	oneTwo.currentNormal += oneTwo.deltaNormal;
		//	oneTwo.currentNormal.Normalize();
		//}

		//Set pixels for the 1-2 edge -- including bottom and right edges
		while (oneTwo.current.y <= (int)oneTwo.end.y)
		{
			// Set up the left-right scanline
			ScanLine leftRight;

			// The long side is on the left
			if (longIsLeft)
			{
				leftRight = setupScanLineX(oneThree, oneTwo);
			}
			// The long is on the right
			else {
				leftRight = setupScanLineX(oneTwo, oneThree);
			}// End If

			fillScanLineFlat(leftRight, vOne.c, img, scene);

			// Advance to the next scanline and interpolate x and z
			oneThree.current.x += oneThree.deltaX;
			oneThree.current.y += 1.0;
			oneThree.current.z += oneThree.deltaZ;
			oneThree.currentColor += oneThree.deltaColor;
			oneThree.currentNormal += oneThree.deltaNormal;
			oneThree.currentNormal.Normalize();

			oneTwo.current.x += oneTwo.deltaX;
			oneTwo.current.y += 1.0;
			oneTwo.current.z += oneTwo.deltaZ;
			oneTwo.currentColor += oneTwo.deltaColor;
			oneTwo.currentNormal += oneTwo.deltaNormal;
			oneTwo.currentNormal.Normalize();


		}// End Loop

		/*deltaY = ceil(vTwo.p.y) - vTwo.p.y;
		twoThree.current.x += twoThree.deltaX * deltaY;
		twoThree.current.y += deltaY;
		twoThree.current.z += twoThree.deltaZ * deltaY;
		twoThree.currentColor += twoThree.deltaColor * deltaY;
		twoThree.currentNormal += twoThree.deltaNormal * deltaY;
		twoThree.currentNormal.Normalize();*/


		//Set pixels for the 2-3 edge -- including bottom and right edges
		while (twoThree.current.y <= (int)twoThree.end.y)
		{
			// Set up the left-right scanline
			ScanLine leftRight;
			// The long side is on the left
			if (longIsLeft)
			{
				leftRight = setupScanLineX(oneThree, twoThree);
			}
			// The long side is on the right
			else {
				leftRight = setupScanLineX(twoThree, oneThree);
			}// End If


			fillScanLineFlat(leftRight, vOne.c, img, scene);

			// Advance to the next scanline and interpolate x and z
			oneThree.current.x += oneThree.deltaX;
			oneThree.current.y += 1.0;
			oneThree.current.z += oneThree.deltaZ;
			oneThree.currentColor += oneThree.deltaColor;
			oneThree.currentNormal += oneThree.deltaNormal;
			oneThree.currentNormal.Normalize();

			twoThree.current.x += twoThree.deltaX;
			twoThree.current.y += 1.0;
			twoThree.current.z += twoThree.deltaZ;
			twoThree.currentColor += twoThree.deltaColor;
			twoThree.currentNormal += twoThree.deltaNormal;
			twoThree.currentNormal.Normalize();

		}// End Loop
	}
	zBuf.Release();
}


/*#include "draw.h"
#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

/*
THIS IS THE HELPER DRAW FILE FROM PEX 1


//Doc Statement: C1C Meixsell helped me with Tcw. He told me that I needed to use the dot product to get the right hand 
//column to have the correct values. He also helped me realize I was using values that weren't normalize 
//properly for the rest of the matrix.

#ifndef VERTEX
#define VERTEX
struct Vertex {
	vec4f p;
	vec4f n;
	vec4f c;
};
#endif // !VERTEX


#define EPS 0.001 // Used for finding vertices nearly horizontal or vertical

/////////////////
// Function   : swap()
// Parameters : Vertex &left   - one of the vertices that need to be swapped
//              Vertex &right   - one of the vertices that need to be swapped
//
/////////////////
void swap(Vertex& left, Vertex& right)
{
	Vertex temp;
	temp = left;
	left = right;
	right = temp;
}

/////////////////
// Function   : sortTriangles()
// Parameters : Vertex &one   - one of the vertices that need to be sorted
//              Vertex &two   - one of the vertices that need to be sorted
//              Vertex &three - one of the vertices that need to be sorted
//
// Return     : bool that tells whether the triangle points left or right				
//              vertices are also sorted.
//
/////////////////
bool sortTriangles(Vertex& one, Vertex& two, Vertex& three)
{
	bool rightPointingTriangle = true;
	float deltaX;
	float deltaY;
	float slope;
	float yIntercept;

	// Check for a horizontal line -- change all the cases to ether:
	//  1---2  or   1
	//    3       2---3

	// Case 1:							
	//    3          3     change to    1
	//  1---2  and 2---1              2---3
	if (abs(one.p.y - two.p.y) < EPS && one.p.y > three.p.y)
	{
		rightPointingTriangle = false;
		swap(one, three);
		if (three.p.x < two.p.x)
		{
			swap(two, three);
		}// End if
	}
	// Case 2:
	//  1---2  and 2---1   change to  1---2
	//    3          3	                3  
	else if (abs(one.p.y - two.p.y) < EPS && one.p.y < three.p.y)
	{
		if (one.p.x > two.p.x)
		{
			swap(one, two);
		}// End if
	}
	// Case 3:
	//    1          1     change to    1
	//  3---2  and 2---3              2---3
	else if (abs(two.p.y - three.p.y) < EPS && one.p.y < three.p.y)
	{
		rightPointingTriangle = false;
		if (three.p.x < two.p.x)
		{
			swap(two, three);
		}// End if
	}
	// Case 4:
	//  3---2  and 2---3   change to  1---2
	//    1          1	                3  
	else if (abs(two.p.y - three.p.y) < EPS && one.p.y > three.p.y)
	{
		if (three.p.x < two.p.x)
		{
			swap(one, three);
		}
		else {
			swap(two, three);

			swap(one, three);
		}// End if	
	}
	// Case 5:
	//  3---1  and 1---3   change to  1---2
	//    2          2	                3  
	else if (abs(one.p.y - three.p.y) < EPS && one.p.y < two.p.y)
	{
		swap(two, three);
		if (one.p.x > two.p.x)
		{
			swap(one, two);
		}// End if
	}
	// Case 6:
	//    2          2     change to    1
	//  3---1  and 1---3              2---3
	else if (abs(one.p.y - three.p.y) < EPS && one.p.y > two.p.y)
	{
		rightPointingTriangle = false;
		swap(one, two);
		if (three.p.x < two.p.x)
		{
			swap(two, three);
		}// End if		 
	}
	// Start handling the general triangle cases -- catch vertical lines and set delta's to 0.
	// Sort along Y-axis -- This ensures that there are only 2 cases of the general triangle:
	//  1		      1
	//     2   or   2
	//   3             3
	else
	{
		if (one.p.y > two.p.y)
		{
			swap(one, two);

		}// End If
		if (one.p.y > three.p.y)
		{
			swap(one, three);
		}// End If
		if (two.p.y > three.p.y)
		{
			swap(two, three);
		}// End If


		 // find where the horizontal line from 2 intersects 1-3
		 //   uses equation of a line y = mx + b
		deltaX = three.p.x - one.p.x;
		deltaY = (three.p.y - one.p.y);
		slope = deltaY / deltaX;
		yIntercept = three.p.y - slope * three.p.x;

		// Set whether or not the "long" edge is on the left 
		if (deltaX != 0.00)
		{
			float temp = (two.p.y - yIntercept) / slope;
			rightPointingTriangle = temp < two.p.x;
		}
		// catch the horizontal line case
		else {
			rightPointingTriangle = one.p.x < two.p.x;
		}// End If
	}// End If


	return rightPointingTriangle;
}

/////////////////
// Function   : draw()
// Parameters : int width	- number of columns
//              int height	- number of rows
//              HDC &img	- windows image buffer
//				Scene scene	- information about the scene (see scene.h)
//
/////////////////
void draw(
	int width,
	int height,
	HDC& img,
	Scene scene)
{
	buf2d zBuf;
	Vertex one, two, three;
	mat4f Tcw, Tpc, Tip;
	vec4f lookat, x, y, z, worldup;
	float nearPoint, farPoint, FOV, pointDif;


	zBuf.init(width, height, -1e25f);

	nearPoint = 0.01;
	farPoint = 100;
	pointDif = farPoint - nearPoint;
	
	lookat = scene.cameraLookAt;
	worldup = scene.cameraUp;

	z = scene.cameraLocation - lookat;
	z.Normalize();

	x = worldup ^ z;
	x.Normalize();

	y = z ^ x;
	y.Normalize();

	FOV = scene.cameraFOV * (PI / 180);  //In rads
	FOV = FOV / 2;


	//Tcw Matrix
	Tcw[0][0] = x.x;			Tcw[0][1] = x.y;			Tcw[0][2] = x.z;								Tcw[0][3] = -x.dot(scene.cameraLocation);
	Tcw[1][0] = y.x;			Tcw[1][1] = y.y;			Tcw[1][2] = y.z;								Tcw[1][3] = -y.dot(scene.cameraLocation);
	Tcw[2][0] = z.x;			Tcw[2][1] = z.y;			Tcw[2][2] = z.z;								Tcw[2][3] = -z.dot(scene.cameraLocation);
	Tcw[3][0] = 0;				Tcw[3][1] = 0;				Tcw[3][2] = 0;									Tcw[3][3] = 1;


	//Tpc Matrix
	Tpc[0][0] = 1 / (tan(FOV));	Tpc[0][1] = 0;				Tpc[0][2] = 0;									Tpc[0][3] = 0;
	Tpc[1][0] = 0;				Tpc[1][1] = 1 / (tan(FOV));	Tpc[1][2] = 0;									Tpc[1][3] = 0;
	Tpc[2][0] = 0;				Tpc[2][1] = 0;				Tpc[2][2] = (farPoint + nearPoint) / pointDif;	Tpc[2][3] = (2 * farPoint * nearPoint) / pointDif;
	Tpc[3][0] = 0;				Tpc[3][1] = 0;				Tpc[3][2] = -1; Tpc[3][3] = 0;


	//Tip Matrix
	Tip[0][0] = 0.5 * width;	Tip[0][1] = 0;				Tip[0][2] = 0;									Tip[0][3] = 0.5 * width;
	Tip[1][0] = 0;				Tip[1][1] = 0.5 * height;	Tip[1][2] = 0;									Tip[1][3] = 0.5 * height;
	Tip[2][0] = 0;				Tip[2][1] = 0;				Tip[2][2] = 1;									Tip[2][3] = 0;
	Tip[3][0] = 0;				Tip[3][1] = 0;				Tip[3][2] = 0;									Tip[3][3] = 1;


	for (int i = 0; i < scene.model.face.size(); i++)
	{
		// Pull vertices from the scene
		one.p = scene.model.vertex[scene.model.face[i][0]];
		two.p = scene.model.vertex[scene.model.face[i][1]];
		three.p = scene.model.vertex[scene.model.face[i][2]];

		one.p = Tip * Tpc * Tcw * one.p;
		one.p /= one.p.w;
		one.p.z = one.p.z * -1;//This line and the ones like it flip the z buffer.

		two.p = Tip * Tpc * Tcw * two.p;
		two.p /= two.p.w;
		two.p.z = two.p.z * -1;

		three.p = Tip * Tpc* Tcw * three.p;
		three.p /= three.p.w;
		three.p.z = three.p.z * -1;


		// Pull color from vertices
		one.c = scene.model.faceColor[scene.model.face[i][0]];
		two.c = scene.model.faceColor[scene.model.face[i][1]];
		three.c = scene.model.faceColor[scene.model.face[i][2]];

		// SORT!
		bool rightPointing = sortTriangles(one, two, three);

		// The deltas for the different slopes
		float oneTwoDeltaX, oneThreeDeltaX, twoThreeDeltaX;

		if (abs(two.p.y - one.p.y) != 0.0)  // Divide by zero avoidance
		{
			oneTwoDeltaX = (two.p.x - one.p.x) / (two.p.y - one.p.y);
		}
		else {
			oneTwoDeltaX = 0.0;
		}

		if (abs(three.p.y - one.p.y) != 0.0) // Divide by zero avoidance
		{
			oneThreeDeltaX = (three.p.x - one.p.x) / (three.p.y - one.p.y);
		}
		else {
			oneThreeDeltaX = 0.0;
		}

		if (abs(three.p.y - two.p.y) != 0.0) // Divide by zero avoidance
		{
			twoThreeDeltaX = (three.p.x - two.p.x) / (three.p.y - two.p.y);
		}
		else {
			twoThreeDeltaX = 0.0;
		}

		float Ax, Bx, deltaA, deltaB;

		Ax = one.p.x; // The starting x value
		Bx = one.p.x; // The ending x value

		// Set up deltas for drawing long edge on top triangles
		if (rightPointing)
		{
			deltaA = oneThreeDeltaX;
			deltaB = oneTwoDeltaX;
		}
		else {
			deltaA = oneTwoDeltaX;
			deltaB = oneThreeDeltaX;
		}

		// Draw long edge on top triangles
		for (float y = one.p.y; y <= two.p.y; y += 1.0)
		{
			for (float x = Ax; x <= Bx; x += 1.0)
			{
				if (zBuf[(int)y][(int)x] < one.p.z) // Test if the value of the pixel is closer to the viewing plane than the stored value
				{
					zBuf[(int)y][(int)x] = one.p.z; // If closer, store in zBuffer and draw the pixel
					SetPixelV(img, (int)x, height - (int)y, RGB(one.c.x * 255, one.c.y * 255, one.c.z * 255));
				}
			}
			Ax += deltaA;
			Bx += deltaB;
		}

		// Set up deltas for drawing long edge on bottom triangles
		if (rightPointing)
		{
			Bx = two.p.x;
			deltaB = twoThreeDeltaX;
		}
		else {
			deltaA = twoThreeDeltaX;
		}

		// Draw long edge on bottom triangles
		for (float y = two.p.y; y <= three.p.y; y += 1.0)
		{
			for (float x = Ax; x <= Bx; x += 1.0)
			{
				if (zBuf[(int)y][(int)x] < one.p.z) // Test if the value of the pixel is closer to the viewing plane than the stored value
				{
					zBuf[(int)y][(int)x] = one.p.z; // If closer, store in zBuffer and draw the pixel
					SetPixelV(img, (int)x, height - (int)y, RGB(one.c.x * 255, one.c.y * 255, one.c.z * 255));

				}
			}
			Ax += deltaA;
			Bx += deltaB;
		}
	}
	zBuf.Release();
}// End of draw()
*/