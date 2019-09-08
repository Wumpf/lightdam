// Various config options which were easier to keep in the shader code than to expose to the UI ;-)

#define NUM_LIGHT_SAMPLES_AVAILABLE 32  // Number of light samples that we provide every frame
#define NUM_LIGHT_SAMPLES_PERHIT    4   // Number of light samples that we take on every hit

#define NUM_BOUNCES 5 // 1 bounce is direct lighting only
//#define DEBUG_VISUALIZE_NORMALS
//#define DEBUG_VISUALIZE_TEXCOORD
//#define DEBUG_VISUALIZE_DIFFUSETEXTURE

//#define RUSSIAN_ROULETTE