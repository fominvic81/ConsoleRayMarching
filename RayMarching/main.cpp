#include <iostream>
#include <stdio.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <chrono>
#include <thread>
#include <windows.h>

#define PI 3.14159265358979323846
const int maxFPS = 60;

class Vec3 {
public:
	double x;
	double y;
	double z;

	Vec3() {
		x = 0; y = 0; z = 0;
	}

	Vec3(double x, double y, double z) {
		this->x = x;
		this->y = y;
		this->z = z;
	}

	Vec3 operator+(Vec3& a) {
		return { x + a.x, y + a.y, z + a.z };
	}
	Vec3 operator+(Vec3&& a) {
		return { x + a.x, y + a.y, z + a.z };
	}

	Vec3 operator-(Vec3& a) {
		return { x - a.x, y - a.y, z - a.z };
	}
	Vec3 operator-(Vec3&& a) {
		return { x - a.x, y - a.y, z - a.z };
	}

	Vec3 operator-() {
		return { -x, -y, -z };
	}

	Vec3 operator*(double s) {
		return { x * s, y * s, z * s };
	}

	double operator*(Vec3& a) {
		return x * a.x + y * a.y + z * a.z;
	}

	double len() {
		return sqrt(x * x + y * y + z * z);
	}

	Vec3& normalize() {
		double k = 1 / len();
		x *= k;
		y *= k;
		z *= k;
		return *this;
	}
};

double sign(double x) { return x < 0 ? -1 : 1; }

//const char* palette = " `.-':_,^=;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RD#$Bg0MNWQ%&@";
//const char* palette = " `.=*#%0$@";
const char* palette = " `.',:^\"-~=*+o]#8CUOD%0&$@";
const size_t paletteSize = strlen(palette);

char getChar(double light) {
	return palette[int(ceil(light * (paletteSize - 1)))];
}

bool isKeyPressed(int key) {
	return GetKeyState(key) & 0x8000;
}

enum ShapeType {
	Circle,
	Rect,
};

class Shape {
public:
	bool reversed = false;
	ShapeType type;
	Vec3 position;
	double radius;
	double w, h, d;

	static Shape circle(Vec3 position, double radius) {
		Shape shape;
		shape.type = ShapeType::Circle;
		shape.position = position;
		shape.radius = radius;
		return shape;
	}

	static Shape rect(Vec3 position, double w, double h, double d) {
		Shape shape;
		shape.type = ShapeType::Rect;
		shape.position = position;
		shape.w = w;
		shape.h = h;
		shape.d = d;
		return shape;
	}

	Shape& reverse() {
		reversed = !reversed;
		return *this;
	}

	double getDist(Vec3& point) {
		return reversed ? -getDistReal(point) : getDistReal(point);
	}

	Vec3 getNormal(Vec3& point) {
		return reversed ? -getNormalReal(point) : getNormalReal(point);
	}

	double getDistReal(Vec3& point) {
		switch (type) {
		case Circle:
			return (point - position).len() - radius;
		case Rect:
			return max(abs(position.x - point.x) - w * 0.5, max(abs(position.y - point.y) - h * 0.5, abs(position.z - point.z) - d * 0.5));
		}
	}

	Vec3 getNormalReal(Vec3& point) {
		Vec3 normal;
		double md = 1000000;

		switch (type) {
		case Circle:
			return (point - position).normalize();
		case Rect:
			if (abs(abs(position.x - point.x) - w * 0.5) < md) {
				normal = { -sign(position.x - point.x), 0, 0 };
				md = abs(abs(position.x - point.x) - w * 0.5);
			}
			if (abs(abs(position.y - point.y) - h * 0.5) < md) {
				normal = { 0, -sign(position.y - point.y), 0 };
				md = abs(abs(position.y - point.y) - h * 0.5);
			}
			if (abs(abs(position.z - point.z) - d * 0.5) < md) {
				normal = { 0, 0, -sign(position.z - point.z) };
				md = abs(abs(position.z - point.z) - d * 0.5);
			}
			return normal;
		}
	}
};

class Collection {
public:
	std::vector<Shape> shapes;

	Collection() {}
	Collection(std::vector<Shape> shapes) : shapes(shapes) {};

	void add(Shape shape) {
		shapes.push_back(shape);
	}

	double getDist(Vec3& point) {
		double dist = 1000000;
		for (Shape shape : shapes) {
			dist = min(dist, shape.getDist(point));
		}
		return dist;
	}

	Vec3 getNormal(Vec3& point) {
		Vec3 normal;
		double dist = 1000000;
		for (Shape shape : shapes) {
			double shapeDist = shape.getDist(point);
			if (shapeDist < dist) {
				dist = shapeDist;
				normal = shape.getNormal(point);
			}
		}
		return normal;
	}
};

class Camera {
public:
	const int width, height;
	char* const buffer;
	size_t bufferSize;
	Vec3 position;
	Vec3 angle;
	Vec3 light;

	double minDist = 3;
	double fogDist = 100;
	double maxViewDist = 500;

	Camera(int width, int height) : width(width), height(height), bufferSize((width + 1)* height), buffer(new char[(width + 1) * height]) {}

	void setPosition(Vec3 position) {
		this->position = position;
	}

	void lookAt(Vec3 point) {
		Vec3 delta = position - point;
		angle.x = -atan2(delta.x, -delta.z);
		double nz = delta.x * sin(angle.x) + delta.z * cos(angle.x);
		angle.y = atan2(-delta.y, -nz);
	}

	double getLight(Vec3& point, Collection& shapes) {
		Vec3 delta = light - point;
		Vec3 normal = shapes.getNormal(point);
		double dist = delta.len();
		double distReversed = 1 / dist;
		Vec3 dir = delta * distReversed;

		double dot = normal * dir;
		double val = (dot + 1) * 0.5;

		if (dist > fogDist) {
			val -= (dist - fogDist) / (maxViewDist - fogDist);
		}

		val *= (dot > 0 ? (1 - march(point + normal * 0.01, dir, shapes, dist, false)) : 0) * 0.6 + 0.4;

		return max(0, val);
	}

	double march(Vec3 pos, Vec3& dir, Collection& shapes, double maxDist, bool light = true) {
		double sumDist = 0;
		for (int i = 0; i < 100; ++i) {
			double dist = shapes.getDist(pos);
			sumDist += dist;
			if (sumDist > maxDist) return 0;
			if (dist > maxDist) return 0;
			if (dist < 0.01) return light ? getLight(pos, shapes) : 1;

			pos = pos + dir * dist;
		}

		return 0;
	}

	char* render(Collection& shapes, double fovX, double fovY) {

		double kx = tan(fovX / 2);
		double ky = tan(fovY / 2);

		double cx = cos(-angle.x);
		double sx = sin(-angle.x);

		double cy = cos(-angle.y);
		double sy = sin(-angle.y);

		double cz = cos(-angle.z);
		double sz = sin(-angle.z);

		int i = 0;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {

				if (x == 0 || x == width - 1) {
					buffer[i++] = '|';
					continue;
				}
				if (y == 0 || y == height - 1) {
					buffer[i++] = '-';
					continue;
				}
				if ((x == width / 2 && abs(y - height / 2) <= 2) ||
					(y == height / 2 && abs(x - width/ 2) <= 2)) {
					buffer[i++] = '#';
					continue;
				}

				double rx = (double(x) / (width - 1) - 0.5) * kx;
				double ry = (double(y) / (height - 1) - 0.5) * ky;

				Vec3 dir = { rx, ry, 1 };
				dir.normalize();

				double ox = dir.x, oy = dir.y, oz = dir.z;

				dir.x = ox * cz - oy * sz;
				dir.y = oy * cz + ox * sz;

				ox = dir.x;
				oy = dir.y;

				dir.y = oy * cy - oz * sy;
				dir.z = oz * cy + oy * sy;

				oz = dir.z;

				dir.x = ox * cx - oz * sx;
				dir.z = oz * cx + ox * sx;

				double val = march(position + dir * minDist, dir, shapes, maxViewDist);

				buffer[i++] = getChar(val);
			}
			buffer[i++] = '\n';
		}
		return buffer;
	}

	~Camera() {
		delete[] buffer;
	}
};

Camera camera = Camera(360, 100);

void updateControls(double dt) {
	if (GetConsoleWindow() != GetForegroundWindow()) return;
	double zoomMultiplier = isKeyPressed('C') ? 0.2 : 1;

	if (isKeyPressed(VK_LEFT)) camera.angle.x -= dt * 1.5 * zoomMultiplier;
	if (isKeyPressed(VK_RIGHT)) camera.angle.x += dt * 1.5 * zoomMultiplier;

	if (isKeyPressed(VK_UP)) camera.angle.y -= dt * 1.5 * zoomMultiplier;
	if (isKeyPressed(VK_DOWN)) camera.angle.y += dt * 1.5 * zoomMultiplier;

	if (isKeyPressed('L')) camera.angle.z -= dt * 1 * zoomMultiplier;
	if (isKeyPressed('K')) camera.angle.z += dt * 1 * zoomMultiplier;

	Vec3 movement;

	if (isKeyPressed(VK_PRIOR)) movement.y -= 20;
	if (isKeyPressed(VK_NEXT)) movement.y += 20;

	if (isKeyPressed('W') || isKeyPressed('S')) {
		movement = movement + Vec3{
			sin(camera.angle.x) * cos(camera.angle.y),
			sin(camera.angle.y),
			cos(camera.angle.x) * cos(camera.angle.y),
		} *10 * (isKeyPressed('W') ? 1 : -1);
	}
	if (isKeyPressed('D')) {
		movement.x += cos(-camera.angle.x) * 10;
		movement.z += sin(-camera.angle.x) * 10;
	}
	if (isKeyPressed('A')) {
		movement.x -= cos(-camera.angle.x) * 10;
		movement.z -= sin(-camera.angle.x) * 10;
	}
	if (isKeyPressed(VK_SPACE)) movement = movement * 10;

	camera.position = camera.position + movement * dt;
	camera.angle.y = min(PI / 2, max(-PI / 2, camera.angle.y));
}

int main() {

	Collection shapes = Collection({
		Shape::rect({ 0, 0, 0 }, 2, 3, 2),
		Shape::rect({ 0, -0.4, 1.3 }, 1, 1.5, 1),
		Shape::rect({ 0, -0.8, -1.1 }, 1.5, 0.75, 0.75),
		Shape::rect({ 0.5, 1.8, 0 }, 0.8, 1, 0.8),
		Shape::rect({ -0.5, 1.8, 0 }, 0.8, 1, 0.8),

		Shape::rect({ 0, 0, 10 }, 25, 25, 1),
	});
	camera.position.x = -5;
	camera.position.y = -4;
	camera.position.z = -5;
	camera.lookAt({ 0, 0, 0 });

	std::chrono::microseconds last = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

	double inc = 0;
	int fps = 0;
	while (true) {
		std::chrono::microseconds now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
		std::chrono::microseconds dtmcs = now - last;
		double dt = dtmcs.count() * 0.000001;
		double minDT = 1.0 / maxFPS;
		double nowSec = now.count() * 0.000001;

		if (dt < minDT) {
			dt = minDT;
			std::this_thread::sleep_for(std::chrono::microseconds(int(minDT * 1000000)) - dtmcs);
		}
		last = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

		inc += dt;
		if (inc > 0.2) {
			inc = 0;
			fps = dt == 0 ? 1 : int(1 / dt);
		}

		updateControls(dt);
		camera.light.x = cos(nowSec) * 5;
		camera.light.y = sin(nowSec) * 5;
		camera.light.z = -10;

		double zoomMultiplier = isKeyPressed('C') ? 0.2 : 1;

		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), { 0, 0 });
		char* pixels = camera.render(shapes, 100 * PI / 180 * zoomMultiplier, 55 * PI / 180 * zoomMultiplier);
		fwrite(pixels, camera.bufferSize, 1, stdout);

		std::cout << camera.position.x << " " << camera.position.y << " " << camera.position.z << std::endl;
		std::cout << fps << "fps    " << std::endl;
	}

	return 0;
}