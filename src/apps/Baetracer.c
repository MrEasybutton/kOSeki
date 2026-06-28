#include "gui.h"
#include "graphics.h"
#include "isr.h"
#include "kmath.h"
#include "kheap.h"
#include "serial.h"
#include "procsys.h"
#include "timer.h"
#include "string.h"
#include "bae.h"
#include "vesa.h"
#include "keyboard.h"
#include "fat32.h"

#define OBJPAR_IMPLEMENTATION
#include "objpar.h"

#define STBI_MALLOC(sz) kmalloc(sz)
#define STBI_REALLOC(p,sz) krealloc(p,sz)
#define STBI_FREE(p) kfree(p)
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

typedef struct { float x, y, z; } Vec3f;

static inline Vec3f V3_ADD (Vec3f a, Vec3f b) { return (Vec3f){a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline Vec3f V3_SUB (Vec3f a, Vec3f b) { return (Vec3f){a.x-b.x, a.y-b.y, a.z-b.z}; }
static inline Vec3f V3_SCALE(Vec3f a, float t) { return (Vec3f){a.x*t, a.y*t, a.z*t }; }
static inline Vec3f V3_MUL (Vec3f a, Vec3f b) { return (Vec3f){a.x*b.x, a.y*b.y, a.z*b.z}; }
static inline float V3_DOT (Vec3f a, Vec3f b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline Vec3f V3_CROSS(Vec3f a, Vec3f b) { return (Vec3f){a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
static inline Vec3f V3_NORM (Vec3f a) { float l=sqrt(V3_DOT(a,a)); if(l<1e-5f)l=1e-5f; return V3_SCALE(a,1.0f/l); }
static inline Vec3f V3_REFL (Vec3f d, Vec3f n) { return V3_SUB(d, V3_SCALE(n, 2.0f*V3_DOT(d,n))); }
static inline float fminf(float a, float b) { return a < b ? a : b; }
static inline float fmaxf(float a, float b) { return a > b ? a : b; }
static inline float clampf(float x, float a, float b) { return x<a?a:(x>b?b:x); }
static inline float smoothstepf(float a, float b, float x) {
    x = clampf((x-a)/(b-a), 0.0f, 1.0f); return x*x*(3.0f-2.0f*x);
}

#define FLOOR_Y -0.6f
#define CHECKER_SCALE 1.0f
#define REFL_THRESHOLD 0.25f
#define RENDER_W 128
#define RENDER_H 128
#define SHADOW_BIAS 0.004f
#define KUW_R 1
#define ROWS_PER_TICK 60
#define MAX_SPHERES 6
#define MAX_LIGHTS 2
#define MAX_BOUNCES 2

typedef struct {
    Vec3f v0, edge1, edge2;
    Vec3f n[3];
    float u[3], v_uv[3];
} Triangle;

typedef struct BVHNode {
    Vec3f bb_min, bb_max;
    struct BVHNode *left, *right;
    int tri_idx; //-1 = internal
} BVHNode;

typedef struct {
    Triangle *triangles;
    int count;
    BVHNode *bvh_root;
    uint32 *texture;
    int tex_w, tex_h;
} Mesh;

typedef struct {
    Vec3f center;
    float radius;
    Vec3f color;
    float specular;
    float reflectivity;
} Sphere;

typedef struct { Vec3f pos, color; } Light;

#define DICE_HALF 0.6f
#define DICE_SPAWN_Y 2.5f
#define DICE_SPAWN_XR 0.1f
#define DICE_SPAWN_Z -3.0f
#define DICE_SPAWN_ZR 0.1f
#define DICE_VX_MAX 0.08f
#define DICE_VZ_MAX 0.08f
#define DICE_GRAVITY -6.0f
#define DICE_BOUNCE 0.52f
#define DICE_FRICTION 0.88f
#define DICE_ROT_SPEED 0.09f
#define DICE_SPECULAR 48.0f
#define DICE_REFL 0.08f
#define DICE_REST_VEL 0.04f
#define DICE_TRI_COUNT 12
#define DICE_HIT_BASE 2000
#define DT 0.016f

typedef struct {
    Vec3f pos, vel;
    float rot[9];
    Vec3f rot_axis;
    float rot_angle, rot_speed;
    int at_rest;
    Triangle tris[DICE_TRI_COUNT];
    BVHNode *bvh;
    Vec3f color;
    float specular, reflectivity;
} Dice;

typedef struct {
    Sphere spheres[MAX_SPHERES];
    int sphere_count;
    Light lights[MAX_LIGHTS];
    int light_count;
    Mesh *mesh;
    Dice *dice;
    Vec3f cam_pos, ambient;
    float angle, yaw, pitch;
    BOOL mouse_locked, m_pressed, b_pressed;
    int last_mouse_x, last_mouse_y;
    Window *win;
    uint32 *framebuf;
    Vec3f *ray_dirs;
    int rays_valid, next_row, cb_parity;
    uint32 last_render_tick;
} RaytracerState;

void SCENE(RaytracerState *s);
extern int g_mouse_x_pos, g_mouse_y_pos, sneaky_bae;

static float ray_triangle(Vec3f ro, Vec3f rd, Triangle *tri, float *uo, float *vo) {
    Vec3f h = V3_CROSS(rd, tri->edge2);
    float a = V3_DOT(tri->edge1, h);
    if (a > -1e-5f && a < 1e-5f) return -1.0f;
    float f = 1.0f / a;
    Vec3f s = V3_SUB(ro, tri->v0);
    float u = f * V3_DOT(s, h);
    if (u < 0.0f || u > 1.0f) return -1.0f;
    Vec3f q = V3_CROSS(s, tri->edge1);
    float v = f * V3_DOT(rd, q);
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    float t = f * V3_DOT(tri->edge2, q);
    if (t > SHADOW_BIAS) { *uo = u; *vo = v; return t; }
    return -1.0f;
}

static inline BOOL ray_aabb(Vec3f ro, Vec3f inv, Vec3f mn, Vec3f mx, float *tn) {
    float t1=(mn.x-ro.x)*inv.x, t2=(mx.x-ro.x)*inv.x;
    float tmin=fminf(t1,t2), tmax=fmaxf(t1,t2);
    float t3=(mn.y-ro.y)*inv.y, t4=(mx.y-ro.y)*inv.y;
    tmin=fmaxf(tmin,fminf(t3,t4)); tmax=fminf(tmax,fmaxf(t3,t4));
    float t5=(mn.z-ro.z)*inv.z, t6=(mx.z-ro.z)*inv.z;
    tmin=fmaxf(tmin,fminf(t5,t6)); tmax=fminf(tmax,fmaxf(t5,t6));
    if (tmax < 0.0f || tmin > tmax) return FALSE;
    *tn = tmin; return TRUE;
}

static BVHNode* build_bvh(Triangle *tris, int *idx, int count) {
    BVHNode *node = (BVHNode*)kmalloc(sizeof(BVHNode));
    node->bb_min = (Vec3f){ 1e30f, 1e30f, 1e30f};
    node->bb_max = (Vec3f){-1e30f, -1e30f, -1e30f};
    node->left = node->right = NULL;
    node->tri_idx = -1;

    for (int i = 0; i < count; i++) {
        Triangle *t = &tris[idx[i]];
        Vec3f v1 = V3_ADD(t->v0, t->edge1), v2 = V3_ADD(t->v0, t->edge2);
        node->bb_min.x = fminf(node->bb_min.x, fminf(t->v0.x, fminf(v1.x, v2.x)));
        node->bb_min.y = fminf(node->bb_min.y, fminf(t->v0.y, fminf(v1.y, v2.y)));
        node->bb_min.z = fminf(node->bb_min.z, fminf(t->v0.z, fminf(v1.z, v2.z)));
        node->bb_max.x = fmaxf(node->bb_max.x, fmaxf(t->v0.x, fmaxf(v1.x, v2.x)));
        node->bb_max.y = fmaxf(node->bb_max.y, fmaxf(t->v0.y, fmaxf(v1.y, v2.y)));
        node->bb_max.z = fmaxf(node->bb_max.z, fmaxf(t->v0.z, fmaxf(v1.z, v2.z)));
    }

    if (count <= 2) {
        if (count == 1) {
            node->tri_idx = idx[0];
        } else {
            node->left = (BVHNode*)kmalloc(sizeof(BVHNode)); memset(node->left, 0, sizeof(BVHNode));
            node->right = (BVHNode*)kmalloc(sizeof(BVHNode)); memset(node->right, 0, sizeof(BVHNode));
            node->left->tri_idx = idx[0]; node->left->bb_min = node->bb_min; node->left->bb_max = node->bb_max;
            node->right->tri_idx = idx[1]; node->right->bb_min = node->bb_min; node->right->bb_max = node->bb_max;
        }
        return node;
    }

    Vec3f size = V3_SUB(node->bb_max, node->bb_min);
    int axis = (size.y > size.x && size.y > size.z) ? 1 : (size.z > size.x && size.z > size.y) ? 2 : 0;
    float mid = (axis==0 ? node->bb_min.x+size.x*0.5f : axis==1 ? node->bb_min.y+size.y*0.5f : node->bb_min.z+size.z*0.5f);

    int i = 0, j = count - 1;
    while (i <= j) {
        Triangle *t = &tris[idx[i]];
        float c = (axis==0 ? t->v0.x+(t->edge1.x+t->edge2.x)*0.33f :
                   axis==1 ? t->v0.y+(t->edge1.y+t->edge2.y)*0.33f :
                              t->v0.z+(t->edge1.z+t->edge2.z)*0.33f);
        if (c < mid) { i++; } else { int tmp=idx[i]; idx[i]=idx[j]; idx[j]=tmp; j--; }
    }
    if (i == 0 || i == count) i = count / 2;
    node->left = build_bvh(tris, idx, i);
    node->right = build_bvh(tris, idx + i, count - i);
    return node;
}

static void intersect_bvh(Vec3f ro, Vec3f rd, Vec3f inv, Mesh *mesh, BVHNode *node, float *tm, int *hi, float *uo, float *vo) {
    float tb;
    if (!ray_aabb(ro, inv, node->bb_min, node->bb_max, &tb) || tb > *tm) return;
    if (node->tri_idx != -1) {
        float u, v, t = ray_triangle(ro, rd, &mesh->triangles[node->tri_idx], &u, &v);
        if (t > SHADOW_BIAS && t < *tm) { *tm=t; *hi=1000+node->tri_idx; *uo=u; *vo=v; }
        return;
    }
    intersect_bvh(ro, rd, inv, mesh, node->left, tm, hi, uo, vo);
    intersect_bvh(ro, rd, inv, mesh, node->right, tm, hi, uo, vo);
}

static int shadow_bvh(Vec3f ro, Vec3f rd, Vec3f inv, float ld, Mesh *mesh, BVHNode *node) {
    float tb;
    if (!ray_aabb(ro, inv, node->bb_min, node->bb_max, &tb) || tb > ld) return 0;
    if (node->tri_idx != -1) {
        float u, v, t = ray_triangle(ro, rd, &mesh->triangles[node->tri_idx], &u, &v);
        return (t > SHADOW_BIAS && t < ld);
    }
    return shadow_bvh(ro,rd,inv,ld,mesh,node->left) || shadow_bvh(ro,rd,inv,ld,mesh,node->right);
}

static float ray_sphere(Vec3f ro, Vec3f rd, Sphere *s) {
    Vec3f oc = V3_SUB(ro, s->center);
    float b = V3_DOT(oc, rd), c = V3_DOT(oc,oc) - s->radius*s->radius;
    float disc = b*b - c;
    if (disc < 0.0f) return -1.0f;
    float sq = sqrt(disc), t0 = -b-sq, t1 = -b+sq;
    if (t0 > SHADOW_BIAS) return t0;
    if (t1 > SHADOW_BIAS) return t1;
    return -1.0f;
}

static void dice_make_rotation(Vec3f ax, float ang, float *rot) {
    float c=cosf(ang), s=sinf(ang), t=1.0f-c, x=ax.x, y=ax.y, z=ax.z;
    rot[0]=t*x*x+c; rot[1]=t*x*y+s*z; rot[2]=t*x*z-s*y;
    rot[3]=t*x*y-s*z; rot[4]=t*y*y+c; rot[5]=t*y*z+s*x;
    rot[6]=t*x*z+s*y; rot[7]=t*y*z-s*x; rot[8]=t*z*z+c;
}

static Vec3f dice_rot_vec(const float *rot, Vec3f v) {
    return (Vec3f){
        rot[0]*v.x + rot[3]*v.y + rot[6]*v.z,
        rot[1]*v.x + rot[4]*v.y + rot[7]*v.z,
        rot[2]*v.x + rot[5]*v.y + rot[8]*v.z,
    };
}

static void dice_rebuild_geometry(Dice *d) {
    float h = DICE_HALF;
    Vec3f lc[8] = {
        {-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
        {-h,-h, h},{h,-h, h},{h,h, h},{-h,h, h},
    };
    Vec3f wc[8];
    for (int i = 0; i < 8; i++) wc[i] = V3_ADD(d->pos, dice_rot_vec(d->rot, lc[i]));

#define DTRI(dst,a,b,c,fid) do { \
    (dst)->v0=wc[a]; (dst)->edge1=V3_SUB(wc[b],wc[a]); (dst)->edge2=V3_SUB(wc[c],wc[a]); \
    Vec3f _fn=V3_NORM(V3_CROSS((dst)->edge1,(dst)->edge2)); \
    (dst)->n[0]=(dst)->n[1]=(dst)->n[2]=_fn; \
    (dst)->u[0]=(dst)->u[1]=(dst)->u[2]=(float)(fid); \
    (dst)->v_uv[0]=(dst)->v_uv[1]=(dst)->v_uv[2]=0.0f; \
} while(0)
    // 1 opp 6, 2 opp 5, 3 opp 4
    DTRI(&d->tris[0], 0,1,2, 0); DTRI(&d->tris[1], 0,2,3, 0);
    DTRI(&d->tris[2], 5,4,7, 1); DTRI(&d->tris[3], 5,7,6, 1);
    DTRI(&d->tris[4], 4,0,3, 2); DTRI(&d->tris[5], 4,3,7, 2);
    DTRI(&d->tris[6], 1,5,6, 3); DTRI(&d->tris[7], 1,6,2, 3);
    DTRI(&d->tris[8], 4,5,1, 4); DTRI(&d->tris[9], 4,1,0, 4);
    DTRI(&d->tris[10], 3,2,6, 5); DTRI(&d->tris[11], 3,6,7, 5);
#undef DTRI
}

static void dice_rebuild_bvh(Dice *d) {
    int idx[DICE_TRI_COUNT];
    for (int i = 0; i < DICE_TRI_COUNT; i++) idx[i] = i;
    Mesh tmp = { d->tris, DICE_TRI_COUNT, NULL, NULL, 0, 0 };
    d->bvh = build_bvh(tmp.triangles, idx, DICE_TRI_COUNT);
}

static unsigned int dice_rng = 12345u;
static float dice_randf(void) {
    dice_rng = dice_rng * 1664525u + 1013904223u;
    return (float)((dice_rng >> 8) & 0xFFFFFF) / 16777216.0f;
}
static float dice_randf_sym(void) { return dice_randf() * 2.0f - 1.0f; }

static void snap_dice(Dice *d) {
    static const Vec3f fn[6] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}};
    static const Vec3f fr[6] = {{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{1,0,0},{1,0,0}};

    int best = 4; float bd = 1e30f;
    for (int i = 0; i < 6; i++) { Vec3f w=dice_rot_vec(d->rot,fn[i]); if(w.y<bd){bd=w.y;best=i;} }
    Vec3f ld=fn[best], lr=fr[best];
    Vec3f lf = { ld.y*lr.z-ld.z*lr.y, ld.z*lr.x-ld.x*lr.z, ld.x*lr.y-ld.y*lr.x };

    d->rot[0]=lr.x; d->rot[1]=-ld.x; d->rot[2]=lf.x;
    d->rot[3]=lr.y; d->rot[4]=-ld.y; d->rot[5]=lf.y;
    d->rot[6]=lr.z; d->rot[7]=-ld.z; d->rot[8]=lf.z;
}

static int shadow_hit(Vec3f ro, Vec3f rd, float ld, RaytracerState *scene);
static Vec3f trace(Vec3f ro, Vec3f rd, RaytracerState *scene, int depth);

static void dice_tick(Dice *d) {
    if (d->at_rest) return;
    d->vel.y += DICE_GRAVITY * DT;
    d->pos.x += d->vel.x; d->pos.y += d->vel.y; d->pos.z += d->vel.z;
    d->rot_angle += d->rot_speed;
    dice_make_rotation(d->rot_axis, d->rot_angle, d->rot);

    float h = DICE_HALF;
    Vec3f lc[8] = {{-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},{-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}};
    float low = 1e30f;
    for (int i = 0; i < 8; i++) { float y=V3_ADD(d->pos,dice_rot_vec(d->rot,lc[i])).y; if(y<low)low=y; }

    if (low <= FLOOR_Y) {
        d->pos.y += FLOOR_Y - low; // lift so lowest corner is at floor
        float spd = d->vel.y < 0.0f ? -d->vel.y : d->vel.y;
        if (spd < DICE_REST_VEL) {
            d->vel = (Vec3f){0,0,0}; d->rot_speed = 0.0f;
            snap_dice(d);
            //recomp
            float sn = 1e30f;
            for (int i = 0; i < 8; i++) {
                float y = V3_ADD(d->pos, dice_rot_vec(d->rot, lc[i])).y;
                if (y < sn) sn = y;
            }
            if (sn < FLOOR_Y) d->pos.y += FLOOR_Y - sn;
            d->at_rest = 1;
        } else {
            d->vel.y = -d->vel.y * DICE_BOUNCE;
            d->vel.x *= DICE_FRICTION; d->vel.z *= DICE_FRICTION;
            d->rot_speed *= DICE_BOUNCE;
        }
    }
    dice_rebuild_geometry(d);
    dice_rebuild_bvh(d);
}

static Dice* dice_spawn_seeded(unsigned int seed) {
    dice_rng ^= seed;
    dice_randf(); dice_randf(); dice_randf();
    Dice *d = (Dice*)kmalloc(sizeof(Dice));
    memset(d, 0, sizeof(Dice));
    d->pos = (Vec3f){ dice_randf_sym()*DICE_SPAWN_XR, DICE_SPAWN_Y, DICE_SPAWN_Z + dice_randf_sym()*DICE_SPAWN_ZR };
    d->vel = (Vec3f){ dice_randf_sym()*DICE_VX_MAX, 0.0f, dice_randf_sym()*DICE_VZ_MAX };
    Vec3f ax = { dice_randf_sym(), dice_randf_sym()*0.4f, dice_randf_sym() };
    if (V3_DOT(ax,ax) < 0.01f) ax = (Vec3f){1,0,0};
    d->rot_axis = V3_NORM(ax);
    d->rot_angle = dice_randf() * 6.2832f;
    d->rot_speed = DICE_ROT_SPEED * (0.7f + dice_randf() * 0.6f);
    d->color = (Vec3f){0.80f, 0.08f, 0.08f};
    d->specular = DICE_SPECULAR;
    d->reflectivity = DICE_REFL;
    dice_make_rotation(d->rot_axis, d->rot_angle, d->rot);
    dice_rebuild_geometry(d);
    dice_rebuild_bvh(d);
    return d;
}

#define PIP_R 0.072f
#define MAX_PIPS 6

static const float pip_cx[6][MAX_PIPS] = {
    {0.50f,0,0,0,0,0}, {0.30f,0.70f,0,0,0,0}, {0.25f,0.50f,0.75f,0,0,0},
    {0.30f,0.70f,0.30f,0.70f,0,0}, {0.30f,0.70f,0.50f,0.30f,0.70f,0},
    {0.30f,0.70f,0.30f,0.70f,0.30f,0.70f},
};
static const float pip_cy[6][MAX_PIPS] = {
    {0.50f,0,0,0,0,0}, {0.30f,0.70f,0,0,0,0}, {0.25f,0.50f,0.75f,0,0,0},
    {0.30f,0.30f,0.70f,0.70f,0,0}, {0.25f,0.25f,0.50f,0.75f,0.75f,0},
    {0.20f,0.20f,0.50f,0.50f,0.80f,0.80f},
};
static const int pip_count[6] = {1,2,3,4,5,6};

static int dice_in_pip(int face, float pu, float pv) {
    for (int i = 0; i < pip_count[face]; i++) {
        float dx=pu-pip_cx[face][i], dy=pv-pip_cy[face][i];
        if (dx*dx + dy*dy < PIP_R*PIP_R) return 1;
    }
    return 0;
}

static void dice_face_uv(Dice *d, int tri_idx, Vec3f hit, float *pu, float *pv) {
    int face = tri_idx / 2;
    static const Vec3f fr[6] = {{1,0,0},{-1,0,0},{0,0,-1},{0,0,1},{1,0,0},{1,0,0}};
    static const Vec3f fu[6] = {{0,1,0},{0,1,0},{0,1,0},{0,1,0},{0,0,1},{0,0,1}};
    static const Vec3f fn[6] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}};
    Vec3f right = dice_rot_vec(d->rot, fr[face]);
    Vec3f up = dice_rot_vec(d->rot, fu[face]);
    Vec3f fnw = dice_rot_vec(d->rot, fn[face]);
    Vec3f centre = V3_ADD(d->pos, V3_SCALE(fnw, DICE_HALF));
    Vec3f delta = V3_SUB(hit, centre);
    float inv2h = 1.0f / (2.0f * DICE_HALF);
    *pu = V3_DOT(delta, right) * inv2h + 0.5f;
    *pv = V3_DOT(delta, up) * inv2h + 0.5f;
}

static Vec3f shade_dice(Vec3f hit, Vec3f rd, int tri_idx, float u, float v, RaytracerState *scene, int depth) {
    Dice *d = scene->dice;
    int face = (int)(d->tris[tri_idx].u[0] + 0.5f);
    Vec3f normal = d->tris[tri_idx].n[0];
    if (V3_DOT(normal, rd) > 0.0f) normal = V3_SCALE(normal, -1.0f);

    float pu, pv;
    dice_face_uv(d, tri_idx, hit, &pu, &pv);
    int pip = dice_in_pip(face, pu, pv);
    
    Vec3f base = pip ? (Vec3f){1.0f, 1.0f, 1.0f} : d->color;
    Vec3f color = V3_MUL(base, scene->ambient);

    for (int li = 0; li < scene->light_count; li++) {
        Light *l = &scene->lights[li];
        Vec3f tol = V3_SUB(l->pos, hit);
        float ld = sqrt(V3_DOT(tol, tol));
        Vec3f ldir = V3_SCALE(tol, 1.0f/ld);
        if (shadow_hit(hit, ldir, ld, scene)) continue;
        float diff = V3_DOT(normal, ldir);
        diff = diff > 0.6f ? 1.0f : diff > 0.2f ? 0.55f : 0.0f;
        color = V3_ADD(color, V3_MUL(V3_SCALE(base, diff), l->color));
        if (!pip) {
            Vec3f hv = V3_NORM(V3_ADD(ldir, V3_SCALE(rd, -1.0f)));
            color = V3_ADD(color, V3_SCALE(l->color, V3_DOT(normal,hv) > 0.94f ? 0.6f : 0.0f));
        }
    }
    float rim = V3_DOT(normal, V3_SCALE(rd, -1.0f));
    rim = 1.0f - (rim < 0.0f ? 0.0f : rim); rim = rim*rim*rim;
    color = V3_ADD(color, V3_SCALE(base, rim * 0.15f));
    if (!pip && depth < MAX_BOUNCES && d->reflectivity > REFL_THRESHOLD)
        color = V3_ADD(color, V3_SCALE(trace(hit, V3_NORM(V3_REFL(rd,normal)), scene, depth+1), d->reflectivity));
    return color;
}

static int nearest_hit(Vec3f ro, Vec3f rd, RaytracerState *scene, float *to, float *uo, float *vo) {
    float tm = 1e30f; int hit = -1;
    for (int i = 0; i < scene->sphere_count; i++) {
        float t = ray_sphere(ro, rd, &scene->spheres[i]);
        if (t > SHADOW_BIAS && t < tm) { tm = t; hit = i; }
    }
    Vec3f inv = {1.0f/rd.x, 1.0f/rd.y, 1.0f/rd.z};
    if (scene->mesh && scene->mesh->bvh_root)
        intersect_bvh(ro, rd, inv, scene->mesh, scene->mesh->bvh_root, &tm, &hit, uo, vo);
    if (scene->dice && scene->dice->bvh) {
        Dice *d = scene->dice;
        Mesh dm = { d->tris, DICE_TRI_COUNT, d->bvh, NULL, 0, 0 };
        int dh = -1; float du=0, dv=0;
        intersect_bvh(ro, rd, inv, &dm, d->bvh, &tm, &dh, &du, &dv);
        if (dh >= 1000) { hit = DICE_HIT_BASE + (dh - 1000); *uo = du; *vo = dv; }
    }
    *to = tm; return hit;
}

static int shadow_hit(Vec3f ro, Vec3f rd, float ld, RaytracerState *scene) {
    for (int i = 0; i < scene->sphere_count; i++) {
        Sphere *sp = &scene->spheres[i];
        Vec3f oc = V3_SUB(sp->center, ro);
        float dot = V3_DOT(oc, rd);
        if (dot < -sp->radius || dot > ld + sp->radius) continue;
        float p2 = V3_DOT(oc,oc) - dot*dot;
        if (p2 > sp->radius*sp->radius) continue;
        float t = ray_sphere(ro, rd, sp);
        if (t > SHADOW_BIAS && t < ld) return 1;
    }
    Vec3f inv = {1.0f/rd.x, 1.0f/rd.y, 1.0f/rd.z};
    if (scene->mesh && scene->mesh->bvh_root &&
        shadow_bvh(ro,rd,inv,ld,scene->mesh,scene->mesh->bvh_root)) return 1;
    if (scene->dice && scene->dice->bvh) {
        Dice *d = scene->dice;
        Mesh dm = { d->tris, DICE_TRI_COUNT, d->bvh, NULL, 0, 0 };
        if (shadow_bvh(ro, rd, inv, ld, &dm, d->bvh)) return 1;
    }
    return 0;
}

static inline float ray_floor(Vec3f ro, Vec3f rd) {
    if (rd.y >= -0.0001f) return -1.0f;
    float t = (FLOOR_Y - ro.y) / rd.y;
    return t > SHADOW_BIAS ? t : -1.0f;
}

static Vec3f shade_floor(Vec3f hit, Vec3f rd, RaytracerState *scene, int depth) {
    int cx = (int)floor(hit.x/CHECKER_SCALE), cz = (int)floor(hit.z/CHECKER_SCALE);
    Vec3f base = ((cx+cz)&1) ? (Vec3f){0.88f,0.84f,0.78f} : (Vec3f){0.18f,0.17f,0.16f};
    Vec3f normal = {0,1,0};
    Vec3f color = V3_MUL(base, scene->ambient);
    for (int li = 0; li < scene->light_count; li++) {
        Light *l = &scene->lights[li];
        Vec3f tol = V3_SUB(l->pos, hit);
        if (tol.y <= 0.0f) continue;
        float ld = sqrt(V3_DOT(tol,tol));
        Vec3f ldir = V3_SCALE(tol, 1.0f/ld);
        if (shadow_hit(hit, ldir, ld, scene)) continue;
        float diff = V3_DOT(normal, ldir); if (diff < 0) diff = 0;
        color = V3_ADD(color, V3_MUL(V3_SCALE(base, diff), l->color));
    }
    if (depth < MAX_BOUNCES)
        color = V3_ADD(color, V3_SCALE(trace(hit, (Vec3f){rd.x,-rd.y,rd.z}, scene, depth+1), 0.12f));
    return color;
}

static Vec3f shade_sphere(Vec3f hit, Vec3f normal, Vec3f rd, int si, RaytracerState *scene, int depth) {
    Sphere *s = &scene->spheres[si];
    Vec3f color = V3_MUL(s->color, scene->ambient);
    for (int li = 0; li < scene->light_count; li++) {
        Light *l = &scene->lights[li];
        Vec3f tol = V3_SUB(l->pos, hit);
        float ld = sqrt(V3_DOT(tol,tol));
        Vec3f ldir = V3_SCALE(tol, 1.0f/ld);
        if (shadow_hit(hit, ldir, ld, scene)) continue;
        float diff = V3_DOT(normal, ldir);
        diff = diff > 0.6f ? 1.0f : diff > 0.2f ? 0.5f : 0.0f;
        color = V3_ADD(color, V3_MUL(V3_SCALE(s->color, diff), l->color));
        Vec3f hv = V3_NORM(V3_ADD(ldir, V3_SCALE(rd,-1.0f)));
        color = V3_ADD(color, V3_SCALE(l->color, V3_DOT(normal,hv) > 0.96f ? 0.72f : 0.0f));
    }
    float rim = 1.0f - V3_DOT(normal, V3_SCALE(rd,-1.0f)); rim=rim*rim*rim;
    color = V3_ADD(color, V3_SCALE((Vec3f){0.8f,0.2f,0.6f}, rim));
    if (depth < MAX_BOUNCES && s->reflectivity > REFL_THRESHOLD)
        color = V3_ADD(color, V3_SCALE(trace(hit, V3_NORM(V3_REFL(rd,normal)), scene, depth+1), s->reflectivity));
    return color;
}

static Vec3f shade_mesh(Vec3f hit, Vec3f rd, int tri_idx, float u, float v,
                        RaytracerState *scene, int depth) {
    Mesh *m = scene->mesh;
    Triangle *tri = &m->triangles[tri_idx];
    float w = 1.0f - u - v;

    Vec3f normal = V3_NORM(V3_ADD(V3_ADD(V3_SCALE(tri->n[0],w), V3_SCALE(tri->n[1],u)), V3_SCALE(tri->n[2],v)));
    if (V3_DOT(normal,rd) > 0.0f) normal = V3_SCALE(normal, -1.0f);
    float uu = tri->u[0]*w+tri->u[1]*u+tri->u[2]*v;
    float vv = tri->v_uv[0]*w+tri->v_uv[1]*u+tri->v_uv[2]*v;
    Vec3f base = {0.8f, 0.8f, 0.8f};
    if (m->texture) {
        int tx=(int)(uu*m->tex_w)%m->tex_w, ty=(int)((1.0f-vv)*m->tex_h)%m->tex_h;
        if(tx<0)tx+=m->tex_w; if(ty<0)ty+=m->tex_h;
        uint32 c = m->texture[ty*m->tex_w+tx];
        base = (Vec3f){(c&0xFF)/255.0f,((c>>8)&0xFF)/255.0f,((c>>16)&0xFF)/255.0f};
    }
    Vec3f color = V3_MUL(base, scene->ambient);
    for (int li = 0; li < scene->light_count; li++) {
        Light *l = &scene->lights[li];
        Vec3f tol = V3_SUB(l->pos, hit);
        float ld = sqrt(V3_DOT(tol,tol));
        Vec3f ldir = V3_SCALE(tol, 1.0f/ld);
        if (shadow_hit(hit, ldir, ld, scene)) continue;
        float diff = V3_DOT(normal, ldir); if(diff<0)diff=0;
        diff = diff>0.5f?1.0f:diff>0.1f?0.6f:0.2f;
        color = V3_ADD(color, V3_MUL(V3_SCALE(base,diff), l->color));
    }
    float rim = 1.0f - V3_DOT(normal, V3_SCALE(rd,-1.0f)); if(rim<0)rim=0; rim=rim*rim*rim;
    color = V3_ADD(color, V3_SCALE(base, rim*0.3f));
    if (depth < MAX_BOUNCES)
        color = V3_ADD(color, V3_SCALE(trace(hit, V3_NORM(V3_REFL(rd,normal)), scene, depth+1), 0.02f));
    return color;
}

static Vec3f trace(Vec3f ro, Vec3f rd, RaytracerState *scene, int depth) {
    float t, u, v;
    int hit = nearest_hit(ro, rd, scene, &t, &u, &v);
    float tf = ray_floor(ro, rd);
    if (tf > SHADOW_BIAS && (hit < 0 || tf < t))
        return shade_floor(V3_ADD(ro, V3_SCALE(rd,tf)), rd, scene, depth);
    if (hit < 0) {
        if (rd.y <= 0.0f) return (Vec3f){0,0,0};
        float h = clampf(rd.y, 0.0f, 1.0f);
        Vec3f top={0.16f,0.24f,0.42f}, mid={0.34f,0.30f,0.52f};
        Vec3f horizon={0.82f,0.52f,0.76f}, glow={0.48f,0.84f,1.0f};
        float t2=smoothstepf(0.12f,0.45f,h), t3=smoothstepf(0.35f,1.0f,h);

        Vec3f col = V3_ADD(V3_SCALE(horizon,1.0f-t2), V3_SCALE(mid,t2));
        float band = clampf(1.0f - fabs(h-0.22f)*4.0f, 0.0f, 1.0f);
        col = V3_ADD(col, V3_SCALE(glow, band*0.10f));
        col = V3_ADD(V3_SCALE(col,1.0f-t3), V3_SCALE(top,t3));
        col = V3_ADD(col, V3_SCALE((Vec3f){0.55f,0.35f,0.65f}, expf(-power((h-0.12f)*8.0f,2.0f))*0.12f));
        return col;
    }
    Vec3f pos = V3_ADD(ro, V3_SCALE(rd, t));
    if (hit >= DICE_HIT_BASE) return shade_dice(pos, rd, hit-DICE_HIT_BASE, u, v, scene, depth);
    if (hit >= 1000) return shade_mesh(pos, rd, hit-1000, u, v, scene, depth);
    return shade_sphere(pos, V3_NORM(V3_SUB(pos, scene->spheres[hit].center)), rd, hit, scene, depth);
}

static uint32 color_to_u32(Vec3f c) {
    int r=clampf(c.x*255,0,255), g=clampf(c.y*255,0,255), b=clampf(c.z*255,0,255);
    return 0xFF000000|(r<<16)|(g<<8)|b;
}

static void precompute_rays(RaytracerState *s) {
    float aspect = (float)RENDER_W/RENDER_H, fov = 0.7f;
    float cy=cosf(s->yaw), sy=sinf(s->yaw), cp=cosf(s->pitch), sp=sinf(s->pitch);
    
    Vec3f fwd = { -sy*cp, sp, -cy*cp };
    Vec3f right = { cy, 0, -sy };
    Vec3f up = { sy*sp, cp, cy*sp };
    for (int py = 0; py < RENDER_H; py++) {
        float fv = (1.0f - 2.0f*(py+0.5f)/RENDER_H) * fov;
        for (int px = 0; px < RENDER_W; px++) {
            float fu = (2.0f*(px+0.5f)/RENDER_W - 1.0f) * aspect * fov;
            Vec3f rd = V3_ADD(V3_ADD(fwd, V3_SCALE(right, fu)), V3_SCALE(up, fv));
            s->ray_dirs[py*RENDER_W+px] = V3_NORM(rd);
        }
    }
    s->rays_valid = 1;
}

static void kuw_quad(const uint32 *fb, int x0, int y0, int x1, int y1,
                     int *or, int *og, int *ob, int *ov) {
    int sr=0,sg=0,sb=0,sr2=0,sg2=0,sb2=0,n=0;
    for (int y=y0; y<=y1; y++) for (int x=x0; x<=x1; x++) {
        uint32 c=fb[y*RENDER_W+x];
        int r=(c>>16)&0xFF, g=(c>>8)&0xFF, b=c&0xFF;
        sr+=r; sg+=g; sb+=b; sr2+=r*r; sg2+=g*g; sb2+=b*b; n++;
    }
    *or=sr/n; *og=sg/n; *ob=sb/n;
    *ov=(sr2-sr*sr/n)+(sg2-sg*sg/n)+(sb2-sb*sb/n);
}

static void PP_ani(RaytracerState *scene, int cb_parity) {
    (void)cb_parity;
    uint32 *fb = scene->framebuf;
    static uint32 tmp[RENDER_W*RENDER_H];
    int r = KUW_R;
    for (int py=0; py<RENDER_H; py++) for (int px=0; px<RENDER_W; px++) {
        int x0=px-r<0?0:px-r, y0=py-r<0?0:py-r;
        int x1=px+r>=RENDER_W?RENDER_W-1:px+r, y1=py+r>=RENDER_H?RENDER_H-1:py+r;
        int qr[4],qg[4],qb[4],qv[4];
        kuw_quad(fb, x0, y0, px, py, &qr[0], &qg[0], &qb[0], &qv[0]);
        kuw_quad(fb, px, y0, x1, py, &qr[1], &qg[1], &qb[1], &qv[1]);
        kuw_quad(fb, x0, py, px, y1, &qr[2], &qg[2], &qb[2], &qv[2]);
        kuw_quad(fb, px, py, x1, y1, &qr[3], &qg[3], &qb[3], &qv[3]);
        int best=0; for(int q=1;q<4;q++) if(qv[q]<qv[best])best=q;
        tmp[py*RENDER_W+px] = 0xFF000000|(qr[best]<<16)|(qg[best]<<8)|qb[best];
    }
    for (int i=0; i<RENDER_W*RENDER_H; i++) fb[i]=tmp[i];
}

static void render_rows(RaytracerState *s) {
    if (!s->rays_valid) precompute_rays(s);
    if (s->next_row == 0) { s->cb_parity ^= 1; SCENE(s); }
    int end = s->next_row + ROWS_PER_TICK; if (end > RENDER_H) end = RENDER_H;
    for (int py=s->next_row; py<end; py++) {
        int rp = (py&1) ^ s->cb_parity;
        for (int px=0; px<RENDER_W; px++) {
            if ((px&1) == rp)
                s->framebuf[py*RENDER_W+px] = color_to_u32(trace(s->cam_pos, s->ray_dirs[py*RENDER_W+px], s, 0));
            else {
                int nx = px > 0 ? px-1 : px+1;
                s->framebuf[py*RENDER_W+px] = s->framebuf[py*RENDER_W+nx];
            }
        }
    }
    s->next_row = end;
    if (s->next_row >= RENDER_H) { s->next_row=0; PP_ani(s, s->cb_parity); }
}

static void render_bae(Window *win) {
    RaytracerState *s = (RaytracerState*)get_process(win->pid)->data;
    if (!s) return;
    int cx=win->x+2, cy=win->y+22, cw=win->width-4, ch=win->height-24;
    render_rows(s);
    static int x_map[1024], x_map_cw=0;
    if (x_map_cw != cw) {
        int map_w = cw < 1024 ? cw : 1024;
        for(int sx=0;sx<map_w;sx++) x_map[sx]=sx*RENDER_W/cw;
        x_map_cw=cw;
    }
    int draw_w = cw < 1024 ? cw : 1024;
    uint32 sw = vbe_get_width();
    for (int sy=0; sy<ch; sy++) {
        uint32 *src=&s->framebuf[(sy*RENDER_H/ch)*RENDER_W], *dst=&g_back_buffer[(cy+sy)*sw+cx];
        for (int sx=0; sx<draw_w; sx++) dst[sx]=src[x_map[sx]];
    }
    text("WASD to move", win->x+4, win->y+win->height-60, RGB(250, 245, 20), FONT_KALNIA, true);
    text("press [M] to look around", win->x+4, win->y+win->height-40, RGB(250, 245, 20), FONT_KALNIA, true);
    text("press [B] to reroll", win->x+4, win->y+win->height-20, RGB(250, 245, 20), FONT_KALNIA, true);
}

extern char* fat_read_file(char* fpath);

// i don't add this to the scene cause it's damn laggy but maybe next time
static Mesh* load_mesh_obj(const char* path) {
    FAT_dirent *de = fat_find_file(path);
    if (!de) return NULL;
    uint32 file_size = de->Size; kfree(de);
    char *data = fat_read_file((char*)path);
    if (!data) return NULL;

    unsigned int bufsz = objpar_get_size(data, file_size);
    void *buffer = kmalloc(bufsz);
    objpar_data_t obj; objpar(data, file_size, buffer, &obj);
    kprint("OBJ: %d faces, fw=%d, tw=%d, pw=%d\n",
                  obj.face_count, obj.face_width, obj.texcoord_width, obj.position_width);
    float ca=cosf(2.7925f), sa=sinf(2.7925f);
    Mesh *mesh = (Mesh*)kmalloc(sizeof(Mesh));
    mesh->count = obj.face_count;
    mesh->triangles = (Triangle*)kmalloc(sizeof(Triangle)*mesh->count);
    mesh->texture = NULL; mesh->bvh_root = NULL;

    unsigned int fw=obj.face_width, pw=obj.position_width, tw=obj.texcoord_width, nw=obj.normal_width;
    for (unsigned int i = 0; i < obj.face_count; i++) {
        Triangle *tri = &mesh->triangles[i];
        unsigned int fb = i * fw * 3;
        for (int j = 0; j < 3; j++) {
            unsigned int vi=obj.p_faces[fb+j*3+0], ti=obj.p_faces[fb+j*3+1], ni=obj.p_faces[fb+j*3+2];
            unsigned int pi = (vi-1)*pw;
            Vec3f vr={obj.p_positions[pi],obj.p_positions[pi+1],obj.p_positions[pi+2]};
            Vec3f vrot={vr.x*ca+vr.z*sa, vr.y, -vr.x*sa+vr.z*ca};
            if (j==0) tri->v0 = vrot;
            else if (j==1) tri->edge1 = V3_SUB(vrot,tri->v0);
            else tri->edge2 = V3_SUB(vrot,tri->v0);
            if (obj.texcoord_count>0 && ti>0) {
                unsigned int tidx=(ti-1)*tw;
                tri->u[j]=obj.p_texcoords[tidx]; tri->v_uv[j]=obj.p_texcoords[tidx+1];
            } else { tri->u[j]=0; tri->v_uv[j]=0; }
            if (obj.normal_count>0 && ni>0) {
                unsigned int nidx=(ni-1)*nw;
                Vec3f nr={obj.p_normals[nidx],obj.p_normals[nidx+1],obj.p_normals[nidx+2]};
                tri->n[j]=(Vec3f){nr.x*ca+nr.z*sa, nr.y, -nr.x*sa+nr.z*ca};
            } else if (j==2) {
                Vec3f fn=V3_NORM(V3_CROSS(tri->edge1,tri->edge2));
                tri->n[0]=tri->n[1]=tri->n[2]=fn;
            }
        }
    }
    int *idx=(int*)kmalloc(sizeof(int)*mesh->count);
    for(int i=0;i<mesh->count;i++) idx[i]=i;
    mesh->bvh_root=build_bvh(mesh->triangles,idx,mesh->count);
    kfree(idx); kfree(buffer); kfree(data);
    return mesh;
}

static void load_texture_png(Mesh *mesh, const char *path) {
    FAT_dirent *de = fat_find_file(path); if(!de) return;
    uint32 sz=de->Size; kfree(de);
    char *data=fat_read_file((char*)path); if(!data) return;
    int w,h,n;
    unsigned char *px=stbi_load_from_memory((unsigned char*)data,sz,&w,&h,&n,4);
    if(px){mesh->texture=(uint32*)px; mesh->tex_w=w; mesh->tex_h=h;}
    kfree(data);
}

void launch_baetracer() {
    Process *p = create_process("Baetracer");
    RaytracerState *s = (RaytracerState*)kmalloc(sizeof(RaytracerState));
    memset(s, 0, sizeof(RaytracerState));
    s->framebuf = (uint32*)kmalloc(sizeof(uint32)*RENDER_W*RENDER_H);
    s->ray_dirs = (Vec3f*) kmalloc(sizeof(Vec3f) *RENDER_W*RENDER_H);
    s->cam_pos = (Vec3f){0.0f, 1.5f, 1.6f};
    s->ambient = (Vec3f){0.25f, 0.25f, 0.25f};

    s->sphere_count = 6;
    
    s->spheres[0] = (Sphere){{0,2.8f,-3}, {0.75f}, {1.00f,0.85f,0.10f}, 48, 0.15f};
    s->spheres[1] = (Sphere){{0,2.8f,-3}, {0.50f}, {0.15f,0.35f,1.00f}, 40, 0.12f};
    s->spheres[2] = (Sphere){{0,2.8f,-3}, {0.46f}, {0.25f,0.90f,0.35f}, 36, 0.10f};
    s->spheres[3] = (Sphere){{0,2.8f,-3}, {0.44f}, {0.95f,0.10f,0.10f}, 36, 0.10f};
    s->spheres[4] = (Sphere){{0,2.8f,-3}, {0.40f}, {0.65f,0.40f,0.15f}, 24, 0.08f};
    s->spheres[5] = (Sphere){{0,2.8f,-3}, {0.38f}, {1.00f,0.07f,0.57f}, 52, 0.18f};

    s->light_count = 1;
    s->lights[0] = (Light){{3.0f,2.0f,1.6f},{0.9f,0.72f,0.88f}};

    p->data = s;
    Window *win = window(p->pid, "Baetracer", -1,-1, 320,320);
    s->win = win;
    win->content_renderer = render_bae;
    s->dice = dice_spawn_seeded(0xBAE5EEDu);
}

static float normalize_angle(float a) {
    while (a > 3.14159f) a -= 6.28318f;
    while (a < -3.14159f) a += 6.28318f;
    return a;
}

void SCENE(RaytracerState *s) {
    BOOL mk = kb_is_key_pressed(SCAN_CODE_KEY_M);
    if (mk && !s->m_pressed) s->mouse_locked = !s->mouse_locked;
    s->m_pressed = mk;

    BOOL bk = kb_is_key_pressed(SCAN_CODE_KEY_B);
    if (bk && !s->b_pressed) {
        if (s->dice) kfree(s->dice);
        s->dice = dice_spawn_seeded((unsigned int)(mouse_getx()*7919 + mouse_gety()*6271));
    }
    s->b_pressed = bk;

    if (s->mouse_locked) {
        int cx=s->win->x+s->win->width/2, cy=s->win->y+s->win->height/2;
        int dx=mouse_getx()-cx, dy=mouse_gety()-cy;
        s->yaw = normalize_angle(s->yaw - dx*0.005f);
        s->pitch = normalize_angle(s->pitch - dy*0.005f);
        if (s->pitch > 1.5f) s->pitch = 1.5f;
        if (s->pitch < -1.5f) s->pitch = -1.5f;
        extern int g_mouse_x_pos, g_mouse_y_pos;
        g_mouse_x_pos=cx; g_mouse_y_pos=cy; sneaky_bae=TRUE;
    } else { sneaky_bae=FALSE; }

    float speed = 0.12f;
    float cz = cosf(s->yaw), sz = sinf(s->yaw);
    if (kb_is_key_pressed(SCAN_CODE_KEY_W)) { s->cam_pos.z -= cz*speed; s->cam_pos.x -= sz*speed; s->rays_valid=0; }
    if (kb_is_key_pressed(SCAN_CODE_KEY_S)) { s->cam_pos.z += cz*speed; s->cam_pos.x += sz*speed; s->rays_valid=0; }
    if (kb_is_key_pressed(SCAN_CODE_KEY_A)) { s->cam_pos.z += sz*speed; s->cam_pos.x -= cz*speed; s->rays_valid=0; }
    if (kb_is_key_pressed(SCAN_CODE_KEY_D)) { s->cam_pos.z -= sz*speed; s->cam_pos.x += cz*speed; s->rays_valid=0; }

    s->angle += 0.06f;
    static const float orbit_r[6] = {3.20f,2.20f,2.45f,1.20f,2.80f,2.00f};
    static const float phase[6] = {0.0f,1.047f,2.094f,3.142f,4.189f,5.236f};
    static const float yw[6] = {0.20f,0.15f,0.18f,0.12f,0.10f,0.14f};
    for (int i = 0; i < 6; i++) {
        float a = s->angle + phase[i];
        s->spheres[i].center = (Vec3f){cosf(a)*orbit_r[i], 2.8f+sinf(a*0.7f)*yw[i], -3.0f+sinf(a)*orbit_r[i]};
    }
    s->rays_valid = 0;

    if (s->dice) dice_tick(s->dice);
}