#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <Windows.h>

CHAR_INFO* screenBuffer;
CHAR_INFO* rasterBuffer;
COORD screenSize;
COORD home = { 0,0 };
HANDLE hStdOut;

float* zBuffer;
float ANGLE_INC;

void initScreen()
{
	screenBuffer = malloc(screenSize.X * screenSize.Y * sizeof(CHAR_INFO));
	rasterBuffer = malloc(screenSize.X * screenSize.Y * sizeof(CHAR_INFO));
	zBuffer = malloc(screenSize.X * screenSize.Y * sizeof(float));
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	// set console info
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	GetConsoleScreenBufferInfo(hStdOut, &consoleInfo);
	for (int i = 0; i < screenSize.X * screenSize.Y; i++)
	{
		screenBuffer[i].Attributes = consoleInfo.wAttributes;
		rasterBuffer[i].Attributes = consoleInfo.wAttributes;
	}

	system("cls");
	SetConsoleCursorPosition(hStdOut, home);
}

void clearScreen(CHAR_INFO* screen)
{
	for (int i = 0; i < screenSize.X * screenSize.Y; i++)
	{
		screen[i].Char.UnicodeChar = ' ';
	}
}

void clearZBuffer()
{
	for (int i = 0; i < screenSize.X * screenSize.Y; i++)
	{
		zBuffer[i] = FLT_MAX;
	}
}

void drawScreen()
{
	SMALL_RECT writeArea = { 0, 0, screenSize.X - 1, screenSize.Y - 1 };
	WriteConsoleOutput(hStdOut, screenBuffer, screenSize, home, &writeArea);
}

/*
	float *matrixA	first matrix
	float *matrixB	second matrix
	float *matrixC  output matrix
	int aSize		first matrix size in bytes
	int aXSize		first matrix x size in bytes
	int bSize		second matrix size in bytes
	int bXSize		second matrix x size in bytes
	matrix[y][x]
*/
void matMul(float* matrixA, float* matrixB, float* matrixC, int aSize, int aXSize, int bSize, int bXSize)
{
	int aYSize = aSize * sizeof(float) / aXSize;
	int bYSize = bSize * sizeof(float) / bXSize;

	if (aXSize != bYSize)
	{
		printf("Error: can't multiply a %dx%d matrix with a %dx%d matrix", aXSize / sizeof(float), aYSize / sizeof(float), bXSize / sizeof(float), bYSize / sizeof(float));
		exit(0);
	}

	for (int i = 0; i < aYSize / sizeof(float); i++)
	{
		for (int j = 0; j < bXSize / sizeof(float); j++)
		{
			float* cmatrixElement = &matrixC[i * bXSize / sizeof(float) + j];
			for (int k = 0; k < aXSize / sizeof(float); k++)
			{
				float amatrixElement = matrixA[i * aXSize / sizeof(float) + k];
				float bmatrixElement = matrixB[k * bXSize / sizeof(float) + j];
				*cmatrixElement += amatrixElement * bmatrixElement;
			}
		}
	}
}

int clamp(float x, int min, int max)
{
	if (x < min) {
		x = min;
	}
	else if (x > max) {
		x = max;
	}
	return (int)x;
}

// https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
void drawLine(float x0, float y0, float x1, float y1, CHAR_INFO* screen, char ch)
{
	x0 = (int)((screenSize.X / 2.0f) + x0 / 2.0f * screenSize.X);
	y0 = (int)((screenSize.Y / 2.0f) + y0 / -2.0f * screenSize.Y);
	x1 = (int)((screenSize.X / 2.0f) + x1 / 2.0f * screenSize.X);
	y1 = (int)((screenSize.Y / 2.0f) + y1 / -2.0f * screenSize.Y);

	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = (dx > dy ? dx : -dy) / 2, e2;

	for (;;)
	{
		if (x0 < screenSize.X && x0 >= 0 && y0 < screenSize.Y && y0 >= 0)
			screen[(int)y0 * screenSize.X + (int)x0].Char.UnicodeChar = ch;
		if (x0 == x1 && y0 == y1)
			break;
		e2 = err;
		if (e2 > -dx)
		{
			err -= dy;
			x0 += sx;
		}
		if (e2 < dy)
		{
			err += dx;
			y0 += sy;
		}
	}
}

void drawTriangle(float* vert3f_0, float* vert3f_1, float* vert3f_2, CHAR_INFO* screen, char ch)
{
	float projection[2][3] = {
		{1, 0, 0},
		{0, 1, 0},
	};

	float vert2f_0[2], vert2f_1[2], vert2f_2[2];

	for (int i = 0; i < 2; i++)
	{
		vert2f_0[i] = 0;
		vert2f_1[i] = 0;
		vert2f_2[i] = 0;
	}

	matMul(projection[0], vert3f_0, vert2f_0, sizeof(projection), sizeof(projection[0]), sizeof(float) * 3, sizeof(float));
	matMul(projection[0], vert3f_1, vert2f_1, sizeof(projection), sizeof(projection[0]), sizeof(float) * 3, sizeof(float));
	matMul(projection[0], vert3f_2, vert2f_2, sizeof(projection), sizeof(projection[0]), sizeof(float) * 3, sizeof(float));

	drawLine(vert2f_0[0], vert2f_0[1], vert2f_1[0], vert2f_1[1], screen, ch);
	drawLine(vert2f_1[0], vert2f_1[1], vert2f_2[0], vert2f_2[1], screen, ch);
	drawLine(vert2f_2[0], vert2f_2[1], vert2f_0[0], vert2f_0[1], screen, ch);
}

void fillTriangle(float* vert3f_0, float* vert3f_1, float* vert3f_2)
{
	clearScreen(rasterBuffer);

	// calculate normal
	float normal[] = {
		(((vert3f_1[1] - vert3f_0[1]) * (vert3f_2[2] - vert3f_0[2])) - ((vert3f_1[2] - vert3f_0[2]) * (vert3f_2[1] - vert3f_0[1]))),
		(((vert3f_1[2] - vert3f_0[2]) * (vert3f_2[0] - vert3f_0[0])) - ((vert3f_1[0] - vert3f_0[0]) * (vert3f_2[2] - vert3f_0[2]))),
		(((vert3f_1[0] - vert3f_0[0]) * (vert3f_2[1] - vert3f_0[1])) - ((vert3f_1[1] - vert3f_0[1]) * (vert3f_2[0] - vert3f_0[0]))),
	};

	float magnitude_normal = sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
	normal[0] /= magnitude_normal;
	normal[1] /= magnitude_normal;
	normal[2] /= magnitude_normal;

	// culling (still needs Z buffering)
	if (normal[2] > 0)
		return;

	// illumination
	char shading[] = { '.', ',', '-', '~', ':', ';', '=', '!', '*', '#', '$', '@' };
	float source[] = { 0, 0, -1 };

	float magnitude_light = sqrt(source[0] * source[0] + source[1] * source[1] + source[2] * source[2]);
	source[0] /= magnitude_light;
	source[1] /= magnitude_light;
	source[2] /= magnitude_light;

	float dot = normal[0] * source[0] + normal[1] * source[1] + normal[2] * source[2];
	char ch = shading[clamp((int)(dot * 12), 0, 11)];

	// rasterization
	drawTriangle(vert3f_0, vert3f_1, vert3f_2, rasterBuffer, ch);
	for (int i = 0; i < screenSize.Y; i++)
	{
		// find first and last character in row
		int first = -1;
		int last = -1;
		for (int j = 0; j < screenSize.X; j++)
		{
			if (rasterBuffer[i * screenSize.X + j].Char.UnicodeChar == ch)
			{
				first = j;
				break;
			}
		}
		for (int j = first + 1; j < screenSize.X; j++)
		{
			if (rasterBuffer[i * screenSize.X + j].Char.UnicodeChar == ch)
			{
				last = j;
			}
		}

		// fill the row
		if (first != -1 && last != -1)
		{
			for (int j = first; j < last; j++)
			{
				rasterBuffer[i * screenSize.X + j].Char.UnicodeChar = ch;
			}
		}
	}

	// copy the shaded polygon
	float avgZ = (vert3f_0[2] + vert3f_1[2] + vert3f_2[2]) / 3;
	for (int i = 0; i < screenSize.X * screenSize.Y; i++)
	{
		if (rasterBuffer[i].Char.UnicodeChar != ' ' && avgZ < zBuffer[i])
		{
			screenBuffer[i] = rasterBuffer[i];
			zBuffer[i] = avgZ;
		}
	}
}

void rotateX(float* vert3f_0, float* vert3f_1, float* vert3f_2, float angle)
{
	float rotationX[3][3] = {
		{1, 0, 0},
		{0, cos(angle), -sin(angle)},
		{0, sin(angle), cos(angle)},
	};

	float vert3f_x0[3], vert3f_x1[3], vert3f_x2[3];
	for (int i = 0; i < 3; i++)
	{
		vert3f_x0[i] = 0;
		vert3f_x1[i] = 0;
		vert3f_x2[i] = 0;
	}

	matMul(rotationX[0], vert3f_0, vert3f_x0, sizeof(rotationX), sizeof(rotationX[0]), sizeof(float) * 3, sizeof(float));
	matMul(rotationX[0], vert3f_1, vert3f_x1, sizeof(rotationX), sizeof(rotationX[0]), sizeof(float) * 3, sizeof(float));
	matMul(rotationX[0], vert3f_2, vert3f_x2, sizeof(rotationX), sizeof(rotationX[0]), sizeof(float) * 3, sizeof(float));

	for (int i = 0; i < 3; i++)
	{
		vert3f_0[i] = vert3f_x0[i];
		vert3f_1[i] = vert3f_x1[i];
		vert3f_2[i] = vert3f_x2[i];
	}
}

void rotateY(float* vert3f_0, float* vert3f_1, float* vert3f_2, float angle)
{
	float rotationY[3][3] = {
		{cos(angle), 0, sin(angle)},
		{0, 1, 0},
		{-sin(angle), 0, cos(angle)},
	};

	float vert3f_y0[3], vert3f_y1[3], vert3f_y2[3];
	for (int i = 0; i < 3; i++)
	{
		vert3f_y0[i] = 0;
		vert3f_y1[i] = 0;
		vert3f_y2[i] = 0;
	}

	matMul(rotationY[0], vert3f_0, vert3f_y0, sizeof(rotationY), sizeof(rotationY[0]), sizeof(float) * 3, sizeof(float));
	matMul(rotationY[0], vert3f_1, vert3f_y1, sizeof(rotationY), sizeof(rotationY[0]), sizeof(float) * 3, sizeof(float));
	matMul(rotationY[0], vert3f_2, vert3f_y2, sizeof(rotationY), sizeof(rotationY[0]), sizeof(float) * 3, sizeof(float));

	for (int i = 0; i < 3; i++)
	{
		vert3f_0[i] = vert3f_y0[i];
		vert3f_1[i] = vert3f_y1[i];
		vert3f_2[i] = vert3f_y2[i];
	}
}

void rotateZ(float* vert3f_0, float* vert3f_1, float* vert3f_2, float angle)
{
	float rotationZ[3][3] = {
		{cos(angle), -sin(angle), 0},
		{sin(angle), cos(angle), 0},
		{0, 0, 1},
	};

	float vert3f_z0[3], vert3f_z1[3], vert3f_z2[3];
	for (int i = 0; i < 3; i++)
	{
		vert3f_z0[i] = 0;
		vert3f_z1[i] = 0;
		vert3f_z2[i] = 0;
	}

	matMul(rotationZ[0], vert3f_0, vert3f_z0, sizeof(rotationZ), sizeof(rotationZ[0]), sizeof(float) * 3, sizeof(float));
	matMul(rotationZ[0], vert3f_1, vert3f_z1, sizeof(rotationZ), sizeof(rotationZ[0]), sizeof(float) * 3, sizeof(float));
	matMul(rotationZ[0], vert3f_2, vert3f_z2, sizeof(rotationZ), sizeof(rotationZ[0]), sizeof(float) * 3, sizeof(float));

	for (int i = 0; i < 3; i++)
	{
		vert3f_0[i] = vert3f_z0[i];
		vert3f_1[i] = vert3f_z1[i];
		vert3f_2[i] = vert3f_z2[i];
	}
}

void render(float* verts, int vSize)
{
	float angle = 0;

	while (1)
	{
		clearZBuffer();
		clearScreen(screenBuffer);
		for (int i = 0; i < vSize / sizeof(float); i += 9)
		{
			float vert3f_0[3], vert3f_1[3], vert3f_2[3];

			for (int j = 0; j < 3; j++)
			{
				vert3f_0[j] = verts[i + j];
				vert3f_1[j] = verts[i + j + 3];
				vert3f_2[j] = verts[i + j + 6];
			}

			rotateX(vert3f_0, vert3f_1, vert3f_2, angle);
			rotateY(vert3f_0, vert3f_1, vert3f_2, angle);
			rotateZ(vert3f_0, vert3f_1, vert3f_2, angle);

			//drawTriangle(vert3f_0, vert3f_1, vert3f_2, screenBuffer, 'x');
			fillTriangle(vert3f_0, vert3f_1, vert3f_2);
		}
		drawScreen();

		angle += ANGLE_INC;
	}
}

float* loadObj(char* fileName, int* objSize)
{
	// 125 000 faces & verts MAX ~ 1 MiB together
	int nVerts = 0;
	float* verts = malloc(125000 * sizeof(float));
	int nFaces = 0;
	int* faces = malloc(125000 * sizeof(float));

	FILE* file = fopen(fileName, "r");

	char line[128];
	while (fgets(line, 128, file) != NULL)
	{
		switch (line[0])
		{
		case 'v':
			sscanf(line, "v %f %f %f", &verts[nVerts], &verts[nVerts + 1], &verts[nVerts + 2]);
			nVerts += 3;
			break;
		case 'f':
			sscanf(line, "f %d %d %d", &faces[nFaces], &faces[nFaces + 1], &faces[nFaces + 2]);
			nFaces += 3;
			break;
		}
	}

	*objSize = sizeof(float) * nFaces * 3;
	float* obj = malloc(*objSize);

	for (int i = 0; i < nFaces; i++)
	{
		obj[i * 3] = verts[(faces[i] - 1) * 3];
		obj[i * 3 + 1] = verts[(faces[i] - 1) * 3 + 1];
		obj[i * 3 + 2] = verts[(faces[i] - 1) * 3 + 2];
	}

	fclose(file);
	free(verts);
	free(faces);

	return obj;
}

int main(int argc, char* argv[])
{
	if (argc != 5)
	{
		printf("usage: %s <width> <height> <angle increment per iteration> <model>\n", argv[0]);
		exit(0);
	}

	screenSize.X = atoi(argv[1]);
	screenSize.Y = atoi(argv[2]);
	ANGLE_INC = atof(argv[3]);
	initScreen();

	int objSize;
	float* obj = loadObj(argv[4], &objSize);

	render(obj, objSize);

	return 0;
}