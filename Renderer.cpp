#include "SDL.h"
#include <iostream>
#include <cstdlib>

using namespace std;
const int MaxAVars = 16;
const int MaxPVars = 16;
struct Vertex {
	float x; // The x component.
	float y; // The y component.
	float z; // The z component.
	float w; // The w component.

    float r;
    float g; 
    float b;

	/// Affine variables.
	float avar[MaxAVars];
	/// Perspective variables.
	float pvar[MaxPVars];
};

struct EdgeEquation {
    float a;
    float b; 
    float c;
    bool tie;
  
    EdgeEquation(const Vertex &v0, const Vertex &v1) {
        a = v0.y - v1.y;
        b = v1.x - v0.x;
        c = -(a * (v0.x + v1.x) + b * (v0.y + v1.y)) / 2;
        tie = a != 0 ? a > 0 : b > 0;
    }
  
    /// Evaluate the edge equation for the given point.
    float evaluate(float x, float y) {
        return a * x + b * y + c;
    }
  
    /// Test if the given point is inside the edge.
    bool test(float x, float y) {
        return test(evaluate(x, y));
    }
  
    /// Test for a given evaluated value.
    bool test(float v) {
        return (v > 0 || (v == 0 && tie));
    }
    //step the equations in the specified directions z value
    float stepX(float z) const {
        return z + a;
    }

    float stepX(float z, float size) const {
        return z + a * size;
    }

    float stepY(float z) const {
        return z + b;
    }

    float stepY(float z, float size) const {
        return z + b * size;
    }
};

struct ParameterEquation {
    float a;
    float b;
    float c;
  
    void init(
        float p0,
        float p1,
        float p2,
        const EdgeEquation &e0,
        const EdgeEquation &e1,
        const EdgeEquation &e2,
        float area) {
        float factor = 1.0f / (2.0f * area);
  
        a = factor * (p0 * e0.a + p1 * e1.a + p2 * e2.a);
        b = factor * (p0 * e0.b + p1 * e1.b + p2 * e2.b);
        c = factor * (p0 * e0.c + p1 * e1.c + p2 * e2.c);
    }
  
    /// Evaluate the parameter equation for the given point.
    float evaluate(float x, float y) {
        return a * x + b * y + c;
    }
};

struct TriangleEquation {
    float area;

    EdgeEquation x;
    EdgeEquation y;
    EdgeEquation z;
    
    ParameterEquation r;
    ParameterEquation g;
    ParameterEquation b;

    TriangleEquation(const Vertex &v0, const Vertex &v1, const Vertex &v2) {
        e0.init(v0, v1);
        e1.init(v1, v2);
        e2.init(v2, v0);
        area = 0.5f * (e0.c + e1.c + e2.c);

        //cull backfacing triangles
        if (area < 0){
            return;
        }
        r.init(v0.r, v1.r, v2.r, e0, e1, e2, area);
        g.init(v0.g, v1.g, v2.g, e0, e1, e2, area);
    }   b.init(v0.b, v1.b, v2.b, e0, e1, e2, area);
}

struct BaryCoords {
    float r;
    float g;
    float b;
    float wv0;
    float wv1;
    float wv2;
};

void putpixel(SDL_Surface*, int, int, Uint32);
void drawTriangle(const Vertex&, const Vertex&, const Vertex&, SDL_Surface*);
BaryCoords* getBarycentricCoordinates(const Vertex&, const Vertex&, const Vertex&, float, float);

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
      "SDL2Test",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      640,
      480,
      0
    );

    SDL_Surface *surface = SDL_GetWindowSurface(window);
    SDL_PixelFormat *pixelFormat = surface->format;
    SDL_FillRect(surface, 0, 0);

    const Vertex v0 = {500, 50, 0, 0, random() % 255, random() % 255, random() % 255}; 
    const Vertex v1 = {250, 300, 0, 0, random() % 255, random() % 255, random() % 255}; 
    const Vertex v2 = {10, 10, 0, 0, random() % 255, random() % 255, random() % 255}; 

    drawTriangle(v0, v1, v2, surface);
    SDL_UpdateWindowSurface(window);

    SDL_Delay(6000);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel) {
    int bpp = surface->format->BytesPerPixel;
    /* p = memory address of the pixel to set */
    Uint8 *p = (Uint8*) surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
        case 1:
            *p = pixel;
            break;

        case 2:
            *(Uint16*) p = pixel;
            break;

        case 3:
            if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                p[0] = (pixel >> 16) & 0xff;
                p[1] = (pixel >> 8) & 0xff;
                p[2] = pixel & 0xff;
            } else {
                p[0] = pixel & 0xff;
                p[1] = (pixel >> 8) & 0xff;
                p[2] = (pixel >> 16) & 0xff;
            }
            break;

        case 4:
            *(Uint32*) p = pixel;
            break;
    }
}
int blockSize = 4;

void drawTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, SDL_Surface* surface) {

    auto start = chrono::steady_clock::now();
    TriangleEquation e(v0, v1, v2);
  
    // Check if triangle is backfacing.
    if (e.area < 0) {
        return;
    }
    
    // Compute triangle bounding box.
    int minX = min(min(v0.x, v1.x), v2.x);
    int maxX = max(max(v0.x, v1.x), v2.x);
    int minY = min(min(v0.y, v1.y), v2.y);
    int maxY = max(max(v0.y, v1.y), v2.y); 

    float s = (float) blockSize - 1;
    minX = minX & ~s;
    maxX = maxX & ~s;
    maxY = maxY & ~s;
    minY = maxY & ~s;

    int stepsX = (maxX - minX) / blockSize + 1;
    int stepsY = (maxY - minY) / blockSize + 1;

    // Add 0.5 to sample at pixel centers.
    #pragma omp parallel for
    for (int i = 0; i < stepsX * stepsY; ++i) {
        int sx = i / stepsX;
        int sy = i % stepsY;

        float x = minX + sx * blockSize + 0.5f;
        float y = minY + sy * blockSize + 0.5f; 

        //test if block is within or outside the triangle
        EdgeData e00;
        e00.init(e, x, y);
        EdgeData e01 = e00;
        e01.stepY(e, s);
        EdgeData e10 = e00;
        e10.stepX(e, s);
        EdgeData e11 = e01;
        e11.stepX(e, s);

        int total = e00.test(e) + e01.test(e) + e10.test(e) + e11.test(e);
        // none within
        if (result == 0) {
            continue;
        }
        bool pcovered;
        //fully  covered
        else if (result == 4) {
            pcovered = false;
        }
        //partially covered
        else {
            pcovered = true;
        }
        rasterizeBlock<pcovered>(e, x, y, surface, v0, v1, v2);
       }
       auto end = chrono:steady_clock::now();
       cout << chrono::duration_cast <chrono::microsecondss>(end - start).count() << endl; 
}
template <bool TestEdges>
void rasterizeBlock(const TriangleEquations &e, float x, float y, SDL_Surface* surface) {
    Pixeldata p0;
   p0.init(e, x, y);

    EdgeData e0;
    if (TestEdges) {
        e0.init(e, x, y);
    }

    for (float yi = y; yi < y + blockSize; yi += 1.0f) {
        pixelData pi = p0;
        EdgeData ei;
        if (TestEdges)
				ei = eo;

			for (float xi = x; xi < x + blockSize; xi += 1.0f) {
				if (!TestEdges || (eqn.e0.test(ei.ev0) && eqn.e1.test(ei.ev1) && eqn.e2.test(ei.ev2))) {
					int rint = (int)(pi.r * 255);
					int gint = (int)(pi.g * 255);
					int bint = (int)(pi.b * 255);
					Uint32 color = SDL_MapRGB(surface->format, rint, gint, bint);
					putpixel(surface, (int)xi, (int)yi, color);
				}

				pi.stepX(eqn);
				if (TestEdges)
					ei.stepX(eqn);
			}

			po.stepY(eqn);
			if (TestEdges)
				eo.stepY(eqn);
		}
    }
}

BaryCoords* getBarycentricCoordinates(const Vertex& v0, const Vertex& v1, const Vertex& v2, 
        float px, float py) {

    float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y); 
    float wv0 = ((v1.y - v2.y) * (px - v2.x) + (v2.x - v1.x) * (py - v2.y)) / denom; 
    float wv1 = ((v2.y - v0.y) * (px - v2.x) + (v0.x - v2.x) * (py - v2.y)) / denom;
    float wv2 = 1 - wv0 - wv1;
    float r = wv0 * v0.r + wv1 * v1.r + wv2 * v2.r;
    float g = wv0 * v0.g + wv1 * v1.g + wv2 * v2.g;
    float b = wv0 * v0.b + wv1 * v1.b + wv2 * v2.b;
    BaryCoords* bc = (BaryCoords*) malloc(sizeof(BaryCoords));
    bc->r = r;
    bc->g = g;
    bc->b = b;
    bc->wv0 = wv0;
    bc->wv1 = wv1;
    bc->wv2 = wv2;
    return bc;
}
//Data classes

struct PixelData {
    float r;
    float g;
    float b;

    void init(const TriangleEquation &e, float x, float y) {
        r = e.r.evaluate(x,y);
        g = e.g.evaluate(x,y);
        b = e.b.evaluate(x,y);
    }
    // Step all pixel data in specified direction
    
    void stepX(const TriangleEquation &e) {
        r = e.r.stepX(r);
        g = e.r.stepX(g);
        b = e.r.stepX(b);
    }

    void stepY(const TriangleEquation &e) {
        r = e.r.stepY(r);
        g = e.r.stepY(g);
        b = e.r.stepY(b);
    }

    struct EdgeData {
        float ev0;
        float ev1;
        float ev2;

        //calculate edge data values
        void init(const TriangleEquation &e, float x, float y) {
            ev0 = e.e0.evaluate(x,y);
            ev1 = e.e1.evaluate(x,y);
            ev2 = e.e2.evaluate(x,y);
        }

        //Step edge values in specified direction
        void stepX(const TriangleEquation &e) {
            ev0 = e.e0.stepX(ev0);
            ev1 = e.e1.stepX(ev1);
            ev2 = e.e2.stepX(ev2);
        }
        void stepX(const TriangleEquation &e, float size) {
            ev0 = e.e0.stepX(ev0, size);
            ev1 = e.e1.stepX(ev1, size);
            ev2 = e.e2.stepX(ev2, size);
        }

        void stepY(const TriangleEquation &e) {
            ev0 = e.e0.stepY(ev0);
            ev1 = e.e1.stepY(ev1);
            ev2 = e.e2.stepY(ev2);
        }

        void stepY(const TriangleEquation &e, float size) {
            ev0 = e.e0.stepY(ev0, size);
            ev1 = e.e1.stepY(ev1, size);
            ev2 = e.e2.stepY(ev2, size);
        }

        bool test(const TriangleEquation &e) {
            return e.e0.test(ev0) && e.e1.test(ev1) &&  e.e2.test(ev2) 
        }

    }

}

//Shaders

class PixelShader {
    public:
        static const bool InterpolateZ = false;
        static const bool InterpolateW = false;
        static const int VarCount = 3;

        static SDL_Surface* surface;

        static void drawPixel(const PixelData &p) {
        int rint = (int)(p.r * 255);
        int gint = (int)(p.g * 255);
        int bint = (int)(p.b * 255);

        Uint32 color = rint << 16 | gint << 8 | bint;

        Uint32 *buffer = (Uint32*)((Uint8 *)surface->pixels + (int)p.y * surface->pitch + (int)p.x * 4);
        *buffer = color;
    }
};

class VertexShader {
public:
  static const int AttribCount = 2;

  static mat4f modelViewProjectionMatrix;

  static void processVertex(VertexShaderInput in, VertexShaderOutput *out)
  {
    const ObjData::VertexArrayData *data = static_cast<const ObjData::VertexArrayData*>(in[0]);

    vec4f position = modelViewProjectionMatrix * vec4f(data->vertex, 1.0f);

    out->x = position.x;
    out->y = position.y;
    out->z = position.y;
    out->w = position.w;
    out->pvar[0] = data->texcoord.x;
    out->pvar[1] = data->texcoord.y;
  }
};


