#include <stdio.h>
#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <linux/input.h>
#include <thread>

//may return 0 when not able to detect
const auto processor_count = std::thread::hardware_concurrency();

struct vec2{
	float x, y;

	vec2(float value) : x(value), y(value) {}
	vec2(float _x, float _y) : x(_x), y(_y) {}

	vec2 operator+(vec2 const& other) { return vec2(x + other.x, y + other.y); }
	vec2 operator-(vec2 const& other) { return vec2(x - other.x, y - other.y); }
	vec2 operator*(vec2 const& other) { return vec2(x * other.x, y * other.y); }
	vec2 operator/(vec2 const& other) { return vec2(x / other.x, y / other.y); }
};

struct vec3{
	float x, y, z;

	vec3(float _value) : x(_value), y(_value), z(_value) {};
	vec3(float _x, vec2 const& v) : x(_x), y(v.x), z(v.y) {};
	vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {};

	vec3 operator+(vec3 const& other) { return vec3(x + other.x, y + other.y, z + other.z); }
	vec3 operator-(vec3 const& other) { return vec3(x - other.x, y - other.y, z - other.z); }
	vec3 operator*(vec3 const& other) { return vec3(x * other.x, y * other.y, z * other.z); }
	vec3 operator/(vec3 const& other) { return vec3(x / other.x, y / other.y, z / other.z); }
	vec3 operator-() { return vec3(-x, -y, -z); }

};

float clamp(float value, float min, float max) { return fmax(fmin(value, max), min); }
double sign(double a) { return (0 < a) - (a < 0); }
double step(double edge, double x) { return x > edge; }

float length(vec2 const& v) { return sqrt(v.x * v.x + v.y * v.y); }

float length(vec3 const& v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
vec3 norm(vec3 v) { return v / length(v); }
float dot(vec3 const& a, vec3 const& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
vec3 abs(vec3 const& v) { return vec3(fabs(v.x), fabs(v.y), fabs(v.z)); }
vec3 sign(vec3 const& v) { return vec3(sign(v.x), sign(v.y), sign(v.z)); }
vec3 step(vec3 const& edge, vec3 v) { return vec3(step(edge.x, v.x), step(edge.y, v.y), step(edge.z, v.z)); }
vec3 reflect(vec3 rd, vec3 n) { return rd - n * (2 * dot(n, rd)); }

vec3 rotateX(vec3 a, double angle){
	vec3 b = a;
	b.z = a.z * cos(angle) - a.y * sin(angle);
	b.y = a.z * sin(angle) + a.y * cos(angle);
	return b;
}

vec3 rotateY(vec3 a, double angle){
	vec3 b = a;
	b.x = a.x * cos(angle) - a.z * sin(angle);
	b.z = a.x * sin(angle) + a.z * cos(angle);
	return b;
}

vec3 rotateZ(vec3 a, double angle){
	vec3 b = a;
	b.x = a.x * cos(angle) - a.y * sin(angle);
	b.y = a.x * sin(angle) + a.y * cos(angle);
	return b;
}

//ro - sphere position, rd - ray, r - radius
vec2 sphere(vec3 ro, vec3 rd, float r) {
	float b = dot(ro, rd);
	float c = dot(ro, ro) - r * r;
	float h = b * b - c;
	if (h < 0.0) return vec2(-1.0);
	h = sqrt(h);
	return vec2(-b - h, -b + h);
}

//ro - box pos, rd - ray
vec2 box(vec3 ro, vec3 rd, vec3 boxSize, vec3& outNormal) {
	vec3 m = vec3(1.0) / rd;
	vec3 n = m * ro;
	vec3 k = abs(m) * boxSize;
	vec3 t1 = -n - k;
	vec3 t2 = -n + k;
	float tN = fmax(fmax(t1.x, t1.y), t1.z);
	float tF = fmin(fmin(t2.x, t2.y), t2.z);
	if (tN > tF || tF < 0.0) return vec2(-1.0);
	vec3 yzx = vec3(t1.y, t1.z, t1.x);
	vec3 zxy = vec3(t1.z, t1.x, t1.y);
	outNormal = -sign(rd) * step(yzx, t1) * step(zxy, t1);
	return vec2(tN, tF);
}

//ro - camera, rd - ray, p - normal of plane, w - dist from (0,0,0)
float plane(vec3 ro, vec3 rd, vec3 p, float w) {
	return -(dot(ro, p) + w) / dot(rd, p);
}

class snake {
public:
	snake() : head(new vec3(0, 0, 0)) { body.push_back(*head); };
	snake(vec3* h) : head(h) { body.push_back(*head); };
	~snake() { 
		delete[] head;
	};
	float draw(vec3 ro,vec3 rd, vec3* n) {
		body[0] = *head;
		float minIt = INFINITY;
		for (int i = 0; i < body.size(); ++i) {
			vec2 intersection = sphere(ro - body[i], rd, radius);
			if (intersection.x > 0 && intersection.x < minIt) {
				vec3 itPoint = ro - body[i] + rd * intersection.x;
				minIt = intersection.x;
				*n = norm(itPoint);
			}
		}
		return minIt;
	};
	void swollow() {
		body[0] = *head;
		body.push_back(body[body.size() - 1]);
		body[body.size() - 1].x -= (radius * 2);
	};
	void move() {
		body[0] = *head;
		for (unsigned i = body.size() - 1; i > 0; --i)
			body[i] = body[i] + (body[i - 1] - body[i]) * (length(body[i - 1] - body[i]) - (radius * 2)) / (radius * 2);
	}
private:
	vec3* head;
	std::vector<vec3> body;
	double radius = 0.5f;
};


#define X_SIZE 1920
#define Y_SIZE 1080
#define SCREEN_SIZE X_SIZE * Y_SIZE
#define BUF_SIZE SCREEN_SIZE * sizeof(uint32_t)

bool key_map[255] = {0};

bool Pressed(int KeyCode){
    return key_map[KeyCode];
}

typedef struct __attribute((packed))__ {
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;
} Pixel;

typedef union {
	Pixel pixel;
	uint32_t raw;
} PixelUnion;

//event6 keyboard, event10 touchpad

int main(int argc, char** argv) {
    int timeout_ms = 1000/30;
	char input_kbd[] = "/dev/input/event6\0";
	int ret;
	struct pollfd fds[1];	    
	fds[0].fd = open(input_kbd, O_RDONLY|O_NONBLOCK);
	if(fds[0].fd<0){
		printf("error unable open for reading '%s'\n",input_kbd);
		return(0);
	}
	const int input_size = 72;
	unsigned char input_data[input_size];
    input_event* events = reinterpret_cast<input_event*>(input_data);
	memset(input_data,0,input_size);
	fds[0].events = POLLIN;
    
    
    float x = 0, y = 3, z = -2;
    float zv = -0.5f, xv = 0.0f, yv = 0.0f, jumph = 0.0f, deltjump = 0.01f;
	bool jump = 0;
	vec3 cur_pos(xv,yv,zv);    
	snake player(&cur_pos);
	int fd = open("/dev/fb0", O_RDWR);
	char* screen = (char*)(mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
	uint32_t *ptr = (uint32_t*)screen;
    uint32_t ptr1[BUF_SIZE / 4];
	PixelUnion p = {0};
	p.pixel.r = 0xFF;
    double aspect = X_SIZE / Y_SIZE;
    double t = 0, t1 = 0;
    while (true) {	
        ret = poll(fds, 1, timeout_ms);
		if(ret>0){
			if(fds[0].revents){
				ssize_t r = read(fds[0].fd,input_data,input_size);
				if(r<0){
					printf("error %d\n",(int)r);
					break;
				} else if(r == 72){
                    key_map[events[1].code] = events[1].value;
					memset(input_data,0,input_size);
				}
			}
		}
        vec3 light = norm(vec3(-0.5, 0.5, -1.0));
        vec3 spherePos = vec3(x, y, z);
        zv += jumph;

        vec3 forw(0, 0, 0);
        if (Pressed(KEY_O))
            player.swollow();
        if (Pressed(KEY_W))
            forw.x += 0.03;
        if (Pressed(KEY_S))
            forw.x -= 0.03;
        if (Pressed(KEY_A))
            forw.y -= 0.03;
        if (Pressed(KEY_D))
            forw.y += 0.03;
        if (Pressed(KEY_LEFTCTRL))
            zv = std::min(-0.05f, zv + 0.05f);
        else
            zv = std::max(-0.5f, zv - 0.05f);
        if (Pressed(KEY_SPACE) && jump == 0) {
            jump = 1;
            jumph += deltjump;
        }
        if (jump){
            if (jumph < 1.5f && jumph > 0.0f)
                jumph += deltjump;
            else {
                deltjump *= -1;
                if (jumph > 0.0f)
                    jumph += deltjump;
                else
                    jump = 0;
            }
        }
        if (forw.x != 0.0f && forw.y != 0.0f){
            forw.x *= 0.70710678118f;//sqrt(2) * 0.5
            forw.y *= 0.70710678118f;
        }
        zv -= jumph;
        forw = rotateZ(forw, t * 0.003);
        xv += forw.x;
        yv += forw.y;
        cur_pos = vec3(xv, yv, zv);
        player.move();
        
        std::thread parting[processor_count];
        for(int i = 0; i < processor_count; ++i){
            parting[i] = std::thread();
        }
        
        for (int i = 0; i < X_SIZE; ++i) {
            for(int j = 0; j < Y_SIZE; ++j){
                vec2 uv = vec2(i, j) / vec2(X_SIZE, Y_SIZE) * 2.0f - 1.0f;
                uv.x *= aspect;
                vec3 ro = vec3(xv, yv, zv);
                vec3 rd = norm(vec3(2, uv));
                rd = rotateY(rd, t1 * 0.003);
                rd = rotateY(rd, 0.25);
                rd = rotateZ(rd, t * 0.003);
                float diff = 1;
                for (int k = 0; k < 10; k++) {
                    float minIt = 1000;
                    vec3 n = 0;
                    float albedo = 0.7f;
                    vec2 intersection = sphere(ro - spherePos, rd, 1);
                    if (intersection.x > 0) {
                        vec3 itPoint = ro - spherePos + rd * intersection.x;
                        minIt = intersection.x;
                        n = norm(itPoint);
                        albedo = 1;
                    }
                    vec3 n1 = 0;
					float minIt1 = 0;
					minIt1 = player.draw(ro, rd, &n1);
					if (minIt1 < minIt) {
						minIt = minIt1;
						n = n1;
					}
                    vec3 boxN = 0;
                    intersection = box(ro + vec3(-2, 2, 1), rd, vec3(1, 2, 3), boxN);
                    if (intersection.x > 0 && intersection.x < minIt) {
                        minIt = intersection.x;
                        n = boxN;
                        albedo = 1;
                    }
                    intersection = plane(ro, rd, vec3(0, 0, -1), 1);
                    if (intersection.x > 0 && intersection.x < minIt) {
                        minIt = intersection.x;
                        n = vec3(0, 0, -1);
                        albedo = 0.2f;
                    }
                    intersection = sphere(ro - vec3(xv, yv, zv), rd, 0.5f);
                    if (intersection.x > 0 && intersection.x < minIt) {
                        vec3 itPoint = ro - vec3(xv, yv, zv) + rd * intersection.x;
                        minIt = intersection.x;
                        n = norm(itPoint);
                        albedo = 0.7f;
                    }
                    if (minIt < 1000) {
                        diff *= (dot(n, light) * 0.5 + 0.6) * albedo;
                        ro = ro + rd * (minIt - 0.01);
                        if (albedo > 0.4f)
                            rd = reflect(rd, n);
                        else
                            break;
                    }
                    else 
                        break;
                }
                ptr1[i + j * X_SIZE] = p.raw * diff;
            }
        }
        memcpy(ptr, ptr1, BUF_SIZE);
	}
	return 0;
}
