struct SceneData {
    mat4 mat_viewproj;
    mat4 mat_cam;
    vec4 camera_world;
    vec4 light_dir;
    vec4 light_color;
    float spread;
    uint dimension;
};

#define mat_viewproj    scene.mat_viewproj
#define mat_cam         scene.mat_cam
#define camera_world    scene.camera_world.xyz
#define light_dir       scene.light_dir
#define light_color     scene.light_color
#define spread          scene.spread
#define dimension       scene.dimension
