struct SceneData {
    mat4 mat_viewproj;
    mat4 mat_cam;
    vec4 camera_world;
    vec4 sun_dir;
    vec4 sun_color;
    float spread;
    uint dimension;
};

#define mat_viewproj    scene.mat_viewproj
#define mat_cam         scene.mat_cam
#define camera_world    scene.camera_world
#define sun_dir         scene.sun_dir
#define sun_color       scene.sun_color
#define spread          scene.spread
#define dimension       scene.dimension
