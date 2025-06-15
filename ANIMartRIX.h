/*
  ___        _            ___  ______ _____    _      
 / _ \      (_)          / _ \ | ___ \_   _|  (_)     
/ /_\ \_ __  _ _ __ ___ / /_\ \| |_/ / | |_ __ ___  __
|  _  | '_ \| | '_ ` _ \|  _  ||    /  | | '__| \ \/ /
| | | | | | | | | | | | | | | || |\ \  | | |  | |>  < 
\_| |_/_| |_|_|_| |_| |_\_| |_/\_| \_| \_/_|  |_/_/\_\

by Stefan Petrick 2023.

High quality LED animations for your next project.

This is a Shader and 5D Coordinate Mapper made for realtime 
rendering of generative animations & artistic dynamic visuals.

This is also a modular animation synthesizer with waveform 
generators, oscillators, filters, modulators, noise generators, 
compressors... and much more.

VO.42 beta version
 
This code is licenced under a Creative Commons Attribution 
License CC BY-NC 3.0

*/

#include <vector>
#include <FastLED.h>

#define num_oscillators 10


struct render_parameters {

  // TODO float center_x = (num_x / 2) - 0.5;   // center of the matrix
  // TODO float center_y = (num_y / 2) - 0.5;
  float center_x = (999 / 2) - 0.5;   // center of the matrix
  float center_y = (999 / 2) - 0.5;
  float dist, angle;                
  float scale_x = 0.1;                  // smaller values = zoom in
  float scale_y = 0.1;
  float scale_z = 0.1;       
  float offset_x, offset_y, offset_z;     
  float z;  
  float low_limit  = 0;                 // getting contrast by highering the black point
  float high_limit = 1;                                            
};

render_parameters animation;     // all animation parameters in one place
struct oscillators {

  float master_speed;            // global transition speed
  float offset[num_oscillators]; // oscillators can be shifted by a time offset
  float ratio[num_oscillators];  // speed ratios for the individual oscillators                                  
};

oscillators timings;             // all speed settings in one place

struct modulators {  

  float linear[num_oscillators];        // returns 0 to FLT_MAX
  float radial[num_oscillators];        // returns 0 to 2*PI
  float directional[num_oscillators];   // returns -1 to 1
  float noise_angle[num_oscillators];   // returns 0 to 2*PI        
};

modulators move;                 // all oscillator based movers and shifters at one place

struct rgb {

  float red, green, blue;
};

rgb pixel;

static const byte pNoise[] = {   151,160,137,91,90, 15,131, 13,201,95,96,
53,194,233, 7,225,140,36,103,30,69,142, 8,99,37,240,21,10,23,190, 6,
148,247,120,234,75, 0,26,197,62,94,252,219,203,117, 35,11,32,57,177,
33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,134,139,
48,27,166, 77,146,158,231,83,111,229,122, 60,211,133,230,220,105,92,
41,55,46,245,40,244,102,143,54,65,25,63,161, 1,216,80,73,209,76,132,
187,208, 89, 18,169,200,196,135,130,116,188,159, 86,164,100,109,198,
173,186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,
212,207,206, 59,227, 47,16,58,17,182,189, 28,42,223,183,170,213,119,
248,152,2,44,154,163,70,221,153,101,155,167,43,172, 9,129,22,39,253,
19,98,108,110,79,113,224,232,178,185,112,104,218,246, 97,228,251,34,
242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,
49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,
150,254,138,236,205, 93,222,114, 67,29,24, 72,243,141,128,195,78,66,
215,61,156,180
};



class ANIMartRIX {

public:

int num_x; // how many LEDs are in one row?
int num_y; // how many rows?

float speed_factor = 1; // 0.1 to 10

float radial_filter_radius = 23.0;      // on 32x32, use 11 for 16x16

bool  serpentine;

std::vector<std::vector<float>> polar_theta;        // look-up table for polar angles
std::vector<std::vector<float>> distance;           // look-up table for polar distances

unsigned long a, b, c;                  // for time measurements




float show1, show2, show3, show4, show5, show6, show7, show8, show9, show0;

ANIMartRIX() {}

ANIMartRIX(int w, int h,  bool serpentine) {
  this->init(w, h, serpentine);
}

void init(int w, int h,  bool serpentine) {
  this->num_x  = w;
  this->num_y = h;
  this->serpentine = serpentine;
  if(w <= 16) { 
    this->radial_filter_radius = 11; 
  }
  else {
    this->radial_filter_radius = 23; // on 32x32, use 11 for 16x16
  }
  render_polar_lookup_table((num_x / 2) - 0.5, (num_y / 2) - 0.5);          // precalculate all polar coordinates 
                                                                           // polar origin is set to matrix centre

  timings.master_speed = 0.01;    // set default speed ratio for the oscillators, not all effects set their own, so start from know state

}

/**
 * @brief Set the Speed Factor 0.1 to 10 - 1 for original speed
 * 
 * @param speed 
 */
void setSpeedFactor(float speed)  {
  this->speed_factor = speed;
}

// Dynamic darkening methods:

float subtract(float &a, float&b) {

  return a - b;
}


float multiply(float &a, float&b) {

  return a * b / 255.f;
}


// makes low brightness darker
// sets the black point high = more contrast 
// animation.low_limit should be 0 for best results
float colorburn(float &a, float&b) {  

  return (1-((1-a/255.f) / (b/255.f)))*255.f;
}


// Dynamic brightening methods

float add(float &a, float&b) {

  return a + b;
}


// makes bright even brighter
// reduces contrast
float screen(float &a, float&b) {

  return (1 - (1 - a/255.f) * (1 - b/255.f))*255.f;
}


float colordodge(float &a, float&b) {  

  return (a/(255.f-b)) * 255.f;
}
/*
 Ken Perlins improved noise   -  http://mrl.nyu.edu/~perlin/noise/
 C-port:  http://www.fundza.com/c4serious/noise/perlin/perlin.html
 by Malcolm Kesson;   arduino port by Peter Chiochetti, Sep 2007 :
 -  make permutation constant byte, obsoletes init(), lookup % 256
*/

float fade(float t){ return t * t * t * (t * (t * 6 - 15) + 10); }
float lerp(float t, float a, float b){ return a + t * (b - a); }
float grad(int hash, float x, float y, float z)
{
int    h = hash & 15;          /* CONVERT LO 4 BITS OF HASH CODE */
float  u = h < 8 ? x : y,      /* INTO 12 GRADIENT DIRECTIONS.   */
          v = h < 4 ? y : h==12||h==14 ? x : z;
return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}

#define P(x) pNoise[(x) & 255]

float pnoise(float x, float y, float z) {
  
int   X = (int)floorf(x) & 255,             /* FIND UNIT CUBE THAT */
      Y = (int)floorf(y) & 255,             /* CONTAINS POINT.     */
      Z = (int)floorf(z) & 255;
x -= floorf(x);                             /* FIND RELATIVE X,Y,Z */
y -= floorf(y);                             /* OF POINT IN CUBE.   */
z -= floorf(z);
float  u = fade(x),                         /* COMPUTE FADE CURVES */
       v = fade(y),                         /* FOR EACH OF X,Y,Z.  */
       w = fade(z);
int  A = P(X)+Y, 
     AA = P(A)+Z, 
     AB = P(A+1)+Z,                         /* HASH COORDINATES OF */
     B = P(X+1)+Y, 
     BA = P(B)+Z, 
     BB = P(B+1)+Z;                         /* THE 8 CUBE CORNERS, */

return lerp(w,lerp(v,lerp(u, grad(P(AA  ), x, y, z),    /* AND ADD */
                          grad(P(BA  ), x-1, y, z)),    /* BLENDED */
              lerp(u, grad(P(AB  ), x, y-1, z),         /* RESULTS */
                   grad(P(BB  ), x-1, y-1, z))),        /* FROM  8 */
            lerp(v, lerp(u, grad(P(AA+1), x, y, z-1),   /* CORNERS */
                 grad(P(BA+1), x-1, y, z-1)),           /* OF CUBE */
              lerp(u, grad(P(AB+1), x, y-1, z-1),
                   grad(P(BB+1), x-1, y-1, z-1))));
}


void calculate_oscillators(oscillators &timings) { 

  double runtime = millis() * timings.master_speed * speed_factor;  // global anaimation speed

  for (int i = 0; i < num_oscillators; i++) {
    
    move.linear[i]      = (runtime + timings.offset[i]) * timings.ratio[i];     // continously rising offsets, returns              0 to max_float
    
    move.radial[i]      = fmodf(move.linear[i], 2 * PI);                        // angle offsets for continous rotation, returns    0 to 2 * PI
    
    move.directional[i] = sinf(move.radial[i]);                                 // directional offsets or factors, returns         -1 to 1
    
    move.noise_angle[i] = PI * (1 + pnoise(move.linear[i], 0, 0));              // noise based angle offset, returns                0 to 2 * PI
    
  }
}


void run_default_oscillators(){

  timings.ratio[0] = 1;           // speed ratios for the oscillators, higher values = faster transitions
  timings.ratio[1] = 2;
  timings.ratio[2] = 3;
  timings.ratio[3] = 4;
  timings.ratio[4] = 5;
  timings.ratio[5] = 6;
  timings.ratio[6] = 7;
  timings.ratio[7] = 8;
  timings.ratio[8] = 9;
  timings.ratio[9] = 10;

  
  timings.offset[0] = 000;
  timings.offset[1] = 100;
  timings.offset[2] = 200;
  timings.offset[3] = 300;
  timings.offset[4] = 400;
  timings.offset[5] = 500;
  timings.offset[6] = 600;
  timings.offset[7] = 700;
  timings.offset[8] = 800;
  timings.offset[9] = 900;

  calculate_oscillators(timings);  
}


// Convert the 2 polar coordinates back to cartesian ones & also apply all 3d transitions.
// Calculate the noise value at this point based on the 5 dimensional manipulation of 
// the underlaying coordinates.

float render_value(render_parameters &animation) {

  // convert polar coordinates back to cartesian ones

  float newx = (animation.offset_x + animation.center_x - (cosf(animation.angle) * animation.dist)) * animation.scale_x;
  float newy = (animation.offset_y + animation.center_y - (sinf(animation.angle) * animation.dist)) * animation.scale_y;
  float newz = (animation.offset_z + animation.z) * animation.scale_z;

  // render noisevalue at this new cartesian point

  float raw_noise_field_value = pnoise(newx, newy, newz);
  
  // A) enhance histogram (improve contrast) by setting the black and white point (low & high_limit)
  // B) scale the result to a 0-255 range (assuming you want 8 bit color depth per rgb chanel)
  // Here happens the contrast boosting & the brightness mapping

  if (raw_noise_field_value < animation.low_limit)  raw_noise_field_value =  animation.low_limit;
  if (raw_noise_field_value > animation.high_limit) raw_noise_field_value = animation.high_limit;

  float scaled_noise_value = map_float(raw_noise_field_value, animation.low_limit, animation.high_limit, 0, 255);

  return scaled_noise_value;
}


// given a static polar origin we can precalculate 
// the polar coordinates

void render_polar_lookup_table(float cx, float cy) {

  polar_theta.resize(num_x, std::vector<float>(num_y, 0.0f));
  distance.resize(num_x, std::vector<float>(num_y, 0.0f));

  for (int xx = 0; xx < num_x; xx++) {
    for (int yy = 0; yy < num_y; yy++) {

      float dx = xx - cx;
      float dy = yy - cy;

      distance[xx][yy]    = hypotf(dx, dy);
      polar_theta[xx][yy] = atan2f(dy, dx); 
    }
  }
}



// float mapping maintaining 32 bit precision
// we keep values with high resolution for potential later usage

float map_float(float x, float in_min, float in_max, float out_min, float out_max) { 
  
  float result = (x-in_min) * (out_max-out_min) / (in_max-in_min) + out_min;
  if (result < out_min) result = out_min;
  if( result > out_max) result = out_max;

  return result; 
}


/* unnecessary bloat

// check result after colormapping and store the newly rendered rgb data

void write_pixel_to_framebuffer(int x, int y, rgb &pixel) {

      // assign the final color of this one pixel
      CRGB finalcolor = CRGB(pixel.red, pixel.green, pixel.blue);
     
      // write the rendered pixel into the framebutter
      leds[xy(x, y)] = finalcolor;
}
*/


// Avoid any possible color flicker by forcing the raw RGB values to be 0-255.
// This enables to play freely with random equations for the colormapping
// without causing flicker by accidentally missing the valid target range.

rgb rgb_sanity_check(rgb &pixel) {

      // rescue data if possible, return absolute value
      //if (pixel.red < 0)     pixel.red = fabsf(pixel.red);
      //if (pixel.green < 0) pixel.green = fabsf(pixel.green);
      //if (pixel.blue < 0)   pixel.blue = fabsf(pixel.blue);

      // Can never be negative colour
      if (pixel.red < 0)     pixel.red = 0;
      if (pixel.green < 0) pixel.green = 0;
      if (pixel.blue < 0)   pixel.blue = 0;

  
      // discard everything above the valid 8 bit colordepth 0-255 range
      if (pixel.red   > 255)   pixel.red = 255;
      if (pixel.green > 255) pixel.green = 255;
      if (pixel.blue  > 255)  pixel.blue = 255;

      return pixel;
}


// find the right led index according to you LED matrix wiring

uint16_t xy(uint8_t x, uint8_t y) {
  if (serpentine &&  y & 1)                             // check last bit
    return (y + 1) * num_x - 1 - x;      // reverse every second line for a serpentine lled layout
  else
    return y * num_x + x;                // use this equation only for a line by line layout
}                                        // remove the previous 3 lines of code in this case



void get_ready() {  // wait until new buffer is ready, measure time
  a = micros();
  logOutput();
}

virtual void setPixelColor(int x, int y, rgb pixel) = 0;

virtual void setPixelColor(int index, rgb pixel) = 0;

void logOutput() {
  b = micros();
}

void logFrame() {
  c = micros();
}

// Show the current framerate, rendered pixels per second,
// rendering time & time spend to push the data to the leds.
// in the serial monitor.

void report_performance() {
  
  float calc  = b - a;                         // waiting time
  float push  = c - b;                         // rendering time
  float total = c - a;                         // time per frame
  int fps  = 1000000 / total;                // frames per second
  int kpps = (fps * num_x * num_y) / 1000;   // kilopixel per second

  Serial.print(fps);                         Serial.print(" fps  ");
  Serial.print(kpps);                        Serial.print(" kpps @");
  Serial.print(num_x*num_y);                 Serial.print(" LEDs  ");  
  Serial.print(round(total));                Serial.print(" µs per frame  waiting: ");
  Serial.print(round((calc * 100) / total)); Serial.print("%  rendering: ");
  Serial.print(round((push * 100) / total)); Serial.print("%  (");
  Serial.print(round(calc));                 Serial.print(" + ");
  Serial.print(round(push));                 Serial.print(" µs)  Core-temp: ");
  // TODO Serial.print( tempmonGetTemp() );
            Serial.println(" °C");
 
}

// Effects


void Rotating_Blob() {

  get_ready(); 
  

  timings.master_speed = 0.01;    // speed ratios for the oscillators
  timings.ratio[0] = 0.1;         // higher values = faster transitions
  timings.ratio[1] = 0.03;
  timings.ratio[2] = 0.03;
  timings.ratio[3] = 0.03;
  
  
  timings.offset[1] = 10;
  timings.offset[2] = 20;
  timings.offset[3] = 30;
  
  calculate_oscillators(timings);     // get linear movers and oscillators going

  for (int x = 0; x < num_x; x++) {
    for (int y = 0; y < num_y; y++) {
  
      // describe and render animation layers
      animation.scale_x    = 0.05;
      animation.scale_y    = 0.05;
      animation.offset_x   = 0;
      animation.offset_y   = 0;
      animation.offset_z   = 100;
      animation.angle      = polar_theta[x][y] +  move.radial[0];
      animation.dist       = distance[x][y];
      animation.z          = move.linear[0];
      animation.low_limit  = -1;
      float show1          = render_value(animation);
      
      animation.angle      = polar_theta[x][y] - move.radial[1] + show1/512.0;
      animation.dist       = distance[x][y] * show1/255.0;
      animation.low_limit  = 0;
      animation.z          = move.linear[1];
      float show2          = render_value(animation);

      animation.angle      = polar_theta[x][y] - move.radial[2] + show1/512.0;
      animation.dist       = distance[x][y] * show1/220.0;
      animation.z          = move.linear[2];
      float show3          = render_value(animation);

      animation.angle      = polar_theta[x][y] - move.radial[3] + show1/512.0;
      animation.dist       = distance[x][y] * show1/200.0;
      animation.z          = move.linear[3];
      float show4          = render_value(animation);

      // colormapping
      pixel.red   = (show2+show4)/2;
      pixel.green = show3 / 6;
      pixel.blue  = 0;

      pixel = rgb_sanity_check(pixel);

     setPixelColor(x, y, pixel);
    }
  }
 
}




void Chasing_Spirals() {

  get_ready(); 
  

  timings.master_speed = 0.01;    // speed ratios for the oscillators
  timings.ratio[0] = 0.1;         // higher values = faster transitions
  timings.ratio[1] = 0.13;
  timings.ratio[2] = 0.16;
  
  timings.offset[1] = 10;
  timings.offset[2] = 20;
  timings.offset[3] = 30;
  
  calculate_oscillators(timings);     // get linear movers and oscillators going

  for (int x = 0; x < num_x; x++) {
    for (int y = 0; y < num_y; y++) {
  
      // describe and render animation layers
      animation.angle      = 3 * polar_theta[x][y] +  move.radial[0] - distance[x][y]/3;
      animation.dist       = distance[x][y];
      animation.scale_z    = 0.1;  
      animation.scale_y    = 0.1;
      animation.scale_x    = 0.1;
      animation.offset_x   = move.linear[0];
      animation.offset_y   = 0;
      animation.offset_z   = 0;
      animation.z          = 0;
      float show1          = render_value(animation);

      animation.angle      = 3 * polar_theta[x][y] +  move.radial[1] - distance[x][y]/3;
      animation.dist       = distance[x][y];
      animation.offset_x   = move.linear[1];
      float show2          = render_value(animation);

      animation.angle      = 3 * polar_theta[x][y] +  move.radial[2] - distance[x][y]/3;
      animation.dist       = distance[x][y];
      animation.offset_x   = move.linear[2];
      float show3          = render_value(animation);

      // colormapping
      float radius = radial_filter_radius;
      float radial_filter = (radius - distance[x][y]) / radius;

      pixel.red   = 3*show1 * radial_filter;
      pixel.green = show2 * radial_filter / 2;
      pixel.blue  = show3 * radial_filter / 4;

      pixel = rgb_sanity_check(pixel);

     setPixelColor(x, y, pixel);
    }
  }
 
}


// Shared helper for multi-layer patterns (Rings, Waves, Center_Field, Distance_Experiment)
void renderMultiLayerPattern(
    int layers,
    std::function<void(int layer, int x, int y)> setupAnimation,
    std::function<void(const float* shows)> combineShows
) {
    for (int x = 0; x < num_x; ++x) {
        for (int y = 0; y < num_y; ++y) {
            float shows[5] = {0}; // up to 5 layers for these effects
            for (int i = 0; i < layers; ++i) {
                setupAnimation(i, x, y);
                shows[i] = render_value(animation);
            }
            combineShows(shows);
            pixel = rgb_sanity_check(pixel);
            setPixelColor(x, y, pixel);
        }
    }
}

// Refactored Rings
void Rings() {
    get_ready();
    float ms = 0.01;
    std::vector<float> ratios = {1, 1.1, 1.2, 0, 0, 0, 0, 0, 0, 0};
    std::vector<float> offsets = {0, 100, 200, 300, 0, 0, 0, 0, 0, 0};
    get_ready_and_calculate_oscillators(ms, ratios, offsets);

    renderMultiLayerPattern(
        3,
        [this](int layer, int x, int y) {
            animation.dist       = distance[x][y];
            animation.angle      = polar_theta[x][y] + 5 * move.noise_angle[layer];
            animation.z          = 15 + 10 * layer;
            animation.scale_x    = 0.05 + 0.05 * layer;
            animation.scale_y    = 0.05 + 0.05 * layer;
            animation.offset_z   = 50 * move.linear[layer];
            animation.offset_x   = 150 * move.directional[layer];
            animation.offset_y   = 150 * move.directional[layer + 1];
        },
        [this](const float* shows) {
            pixel.red   = shows[0] + shows[1];
            pixel.green = shows[1] + shows[2];
            pixel.blue  = shows[2];
        }
    );
}

// Refactored Waves
void Waves() {
    get_ready();
    float ms = 0.01;
    std::vector<float> ratios = {2, 2.1, 1.2, 0, 0, 0, 0, 0, 0, 0};
    std::vector<float> offsets = {0, 100, 200, 300, 0, 0, 0, 0, 0, 0};
    get_ready_and_calculate_oscillators(ms, ratios, offsets);

    renderMultiLayerPattern(
        3,
        [this](int layer, int x, int y) {
            animation.dist       = distance[x][y];
            animation.angle      = polar_theta[x][y] + 4 * move.noise_angle[layer];
            animation.z          = 10 + 10 * layer;
            animation.scale_x    = 0.07 + 0.03 * layer;
            animation.scale_y    = 0.07 + 0.03 * layer;
            animation.offset_z   = 40 * move.linear[layer];
            animation.offset_x   = 120 * move.directional[layer];
            animation.offset_y   = 120 * move.directional[layer + 1];
        },
        [this](const float* shows) {
            pixel.red   = shows[0] + shows[1];
            pixel.green = shows[1] + shows[2];
            pixel.blue  = shows[2];
        }
    );
}

// Refactored Center_Field
void Center_Field() {
    get_ready();
    float ms = 0.01;
    std::vector<float> ratios = {1, 1.1, 1.2, 0, 0, 0, 0, 0, 0, 0};
    std::vector<float> offsets = {0, 100, 200, 300, 0, 0, 0, 0, 0, 0};
    get_ready_and_calculate_oscillators(ms, ratios, offsets);

    renderMultiLayerPattern(
        3,
        [this](int layer, int x, int y) {
            animation.dist       = distance[x][y];
            animation.angle      = polar_theta[x][y] + 3 * move.noise_angle[layer];
            animation.z          = 20 + 10 * layer;
            animation.scale_x    = 0.09 + 0.03 * layer;
            animation.scale_y    = 0.09 + 0.03 * layer;
            animation.offset_z   = 60 * move.linear[layer];
            animation.offset_x   = 100 * move.directional[layer];
            animation.offset_y   = 100 * move.directional[layer + 1];
        },
        [this](const float* shows) {
            pixel.red   = shows[0] + shows[1];
            pixel.green = shows[1] + shows[2];
            pixel.blue  = shows[2];
        }
    );
}

// Refactored Distance_Experiment
void Distance_Experiment() {
    get_ready();
    float ms = 0.01;
    std::vector<float> ratios = {0.2, 0.13, 0.012, 0, 0, 0, 0, 0, 0, 0};
    std::vector<float> offsets = {0, 100, 200, 300, 0, 0, 0, 0, 0, 0};
    get_ready_and_calculate_oscillators(ms, ratios, offsets);

    renderMultiLayerPattern(
        2,
        [this](int layer, int x, int y) {
            animation.dist       = distance[x][y];
            animation.angle      = polar_theta[x][y] + 2 * move.noise_angle[layer];
            animation.z          = 10 + 10 * layer;
            animation.scale_x    = 0.08 + 0.02 * layer;
            animation.scale_y    = 0.08 + 0.02 * layer;
            animation.offset_z   = 30 * move.linear[layer];
            animation.offset_x   = 80 * move.directional[layer];
            animation.offset_y   = 80 * move.directional[layer + 1];
        },
        [this](const float* shows) {
            pixel.red   = shows[0] + shows[1];
            pixel.green = shows[1];
            pixel.blue  = 0;
        }
    );
}

void Caleido1() {

  get_ready(); 
  float ms = 0.02;
  std::vector<float> ratios = {0.0025, 0.0027, 0.0031, 0.0033, 0.0036, 0.0039};
  std::vector<float> offsets = {0, 100, 200, 300, 400, 500};
  get_ready_and_calculate_oscillators(ms, ratios, offsets);

  renderCaleidoPattern(
    {5, 4, 5, 5, 5},          // angle factors
    {0.1, 0.15, 0.1, 0.15, 0.2}, // scales
    {5, 15, 25, 35, 45},         // z values
    {50, 50, 50, 50, 50},        // offsetZ factors
    {150, 150, 150, 150, 150},   // offsetX factors
    {150, 150, 150, 150, 150}    // offsetY factors
  );
}

void Caleido2() {

  get_ready(); 
  float ms = 0.03;
  std::vector<float> ratios = {0.003, 0.0032, 0.0034, 0.0036, 0.0038, 0.004};
  std::vector<float> offsets = {0, 120, 240, 360, 480, 600};
  get_ready_and_calculate_oscillators(ms, ratios, offsets);

  renderCaleidoPattern(
    {6, 5, 6, 6, 6},
    {0.12, 0.17, 0.12, 0.17, 0.22},
    {6, 16, 26, 36, 46},
    {55, 55, 55, 55, 55},
    {155, 155, 155, 155, 155},
    {155, 155, 155, 155, 155}
  );
}

void Caleido3() {

  get_ready(); 
  float ms = 0.025;
  std::vector<float> ratios = {0.0028, 0.0030, 0.0032, 0.0034, 0.0036, 0.0038};
  std::vector<float> offsets = {0, 110, 220, 330, 440, 550};
  get_ready_and_calculate_oscillators(ms, ratios, offsets);

  renderCaleidoPattern(
    {7, 6, 7, 7, 7},
    {0.13, 0.18, 0.13, 0.18, 0.23},
    {7, 17, 27, 37, 47},
    {60, 60, 60, 60, 60},
    {160, 160, 160, 160, 160},
    {160, 160, 160, 160, 160}
  );
}


// Shared helper for all Caleido* methods
void renderCaleidoPattern(
    const std::array<float, 5>& angle_factors,
    const std::array<float, 5>& scales,
    const std::array<float, 5>& zs,
    const std::array<float, 5>& offsetZ_factors,
    const std::array<float, 5>& offsetX_factors,
    const std::array<float, 5>& offsetY_factors
) {
    for (int x = 0; x < num_x / 2; x++) {
        for (int y = 0; y < num_y / 2; y++) {
            float shows[5];
            for (int i = 0; i < 5; ++i) {
                animation.dist       = distance[x][y];
                animation.angle      = polar_theta[x][y] + angle_factors[i] * move.noise_angle[i];
                animation.z          = zs[i];
                animation.scale_x    = scales[i];
                animation.scale_y    = scales[i];
                animation.offset_z   = offsetZ_factors[i] * move.linear[i];
                animation.offset_x   = offsetX_factors[i] * move.directional[i];
                animation.offset_y   = offsetY_factors[i] * move.directional[i+1];
                shows[i]             = render_value(animation);
            }

            pixel.red    = shows[0] + shows[1];
            pixel.green  = shows[2] + shows[3];
            pixel.blue   = shows[4];

            pixel = rgb_sanity_check(pixel);
            setPixelColor(x, y, pixel);
            setPixelColor(xy((num_x - 1) - x, y), pixel);
            setPixelColor(xy((num_x - 1) - x, (num_y - 1) - y), pixel);
            setPixelColor(xy(x, (num_y - 1) - y), pixel);
        }
    }
}