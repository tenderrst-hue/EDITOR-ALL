/*
 * EDITOR ALL (editor-all) v0.1 для Super Bear Adventure / Bearite.
 * Листаем объекты сцены (Renderer / UI Graphic), выбранный мигает. Действия: цвет, скрыть,
 * показать, заменить картинку, сбросить. Правки можно сохранить в файл и они применятся при запуске.
 * Всё через IL2CPP по именам, без смещений.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "bearite_api.h"
#include "bearite_il2cpp.h"

static const BeariteApi* g_api;
static const char* g_id = "editor-all";
static void* (*orig_get_main)(const void* m);
static int g_busy;

static void lg(const char* fmt, ...) {
    char b[300];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    g_api->log(g_id, b);
}
static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ---------- методы Unity ---------- */
static struct {
    void *obj_name, *comp_go, *go_active, *go_setactive, *go_scene;
    void *r_shared, *r_enabled, *mat_tex_get, *mat_tex_set, *mat_col_get, *mat_col_set;
    void *sr_spr_get, *sr_spr_set, *sr_col_get, *sr_col_set, *beh_enabled;
    void *g_col_get, *g_col_set, *img_spr_get, *img_spr_set, *raw_tex_get, *raw_tex_set, *spr_tex, *find_all;
    void *t_renderer, *t_graphic, *t_rect, *t_sprite, *t_texture;
    void* (*obj_class)(void*);
    const char* (*class_name)(void*);
} M;

static void* meth(const char* ns, const char* cls, const char* name, int argc) {
    void* m = bearite_find_method(ns, cls, name, argc);
    if (!m) lg("нет метода %s::%s", cls, name);
    return m;
}
static void* type_obj(const char* ns, const char* name) {
    void* (*get_type)(void*) = (void* (*)(void*))g_api->il2cpp_symbol("il2cpp_class_get_type");
    void* (*get_obj)(void*) = (void* (*)(void*))g_api->il2cpp_symbol("il2cpp_type_get_object");
    void* k = bearite_find_class(ns, name);
    if (!k || !get_type || !get_obj) { lg("нет типа %s", name); return NULL; }
    return get_obj(get_type(k));
}
static void init_methods(void) {
    M.obj_class = (void* (*)(void*))g_api->il2cpp_symbol("il2cpp_object_get_class");
    M.class_name = (const char* (*)(void*))g_api->il2cpp_symbol("il2cpp_class_get_name");
    M.obj_name = meth("UnityEngine", "Object", "get_name", 0);
    M.comp_go = meth("UnityEngine", "Component", "get_gameObject", 0);
    M.go_active = meth("UnityEngine", "GameObject", "get_activeSelf", 0);
    M.go_setactive = meth("UnityEngine", "GameObject", "SetActive", 1);
    M.go_scene = meth("UnityEngine", "GameObject", "get_scene", 0);
    M.r_shared = meth("UnityEngine", "Renderer", "get_sharedMaterial", 0);
    M.r_enabled = meth("UnityEngine", "Renderer", "set_enabled", 1);
    M.mat_tex_get = meth("UnityEngine", "Material", "get_mainTexture", 0);
    M.mat_tex_set = meth("UnityEngine", "Material", "set_mainTexture", 1);
    M.mat_col_get = meth("UnityEngine", "Material", "get_color", 0);
    M.mat_col_set = meth("UnityEngine", "Material", "set_color", 1);
    M.sr_spr_get = meth("UnityEngine", "SpriteRenderer", "get_sprite", 0);
    M.sr_spr_set = meth("UnityEngine", "SpriteRenderer", "set_sprite", 1);
    M.sr_col_get = meth("UnityEngine", "SpriteRenderer", "get_color", 0);
    M.sr_col_set = meth("UnityEngine", "SpriteRenderer", "set_color", 1);
    M.beh_enabled = meth("UnityEngine", "Behaviour", "set_enabled", 1);
    M.g_col_get = meth("UnityEngine.UI", "Graphic", "get_color", 0);
    M.g_col_set = meth("UnityEngine.UI", "Graphic", "set_color", 1);
    M.img_spr_get = meth("UnityEngine.UI", "Image", "get_sprite", 0);
    M.img_spr_set = meth("UnityEngine.UI", "Image", "set_sprite", 1);
    M.raw_tex_get = meth("UnityEngine.UI", "RawImage", "get_texture", 0);
    M.raw_tex_set = meth("UnityEngine.UI", "RawImage", "set_texture", 1);
    M.spr_tex = meth("UnityEngine", "Sprite", "get_texture", 0);
    M.find_all = meth("UnityEngine", "Resources", "FindObjectsOfTypeAll", 1);
    M.t_renderer = type_obj("UnityEngine", "Renderer");
    M.t_graphic = type_obj("UnityEngine.UI", "Graphic");
    M.t_rect = type_obj("UnityEngine", "RectTransform");
    M.t_sprite = type_obj("UnityEngine", "Sprite");
    M.t_texture = type_obj("UnityEngine", "Texture");
}

static void* inv(void* m, void* obj, void** args) { return m ? bearite_invoke(m, obj, args) : NULL; }

static void u16(void* s, char* out, int n) {
    out[0] = 0;
    if (!s) return;
    int len = *(int32_t*)((char*)s + 16);
    const uint16_t* c = (const uint16_t*)((char*)s + 20);
    int o = 0;
    for (int i = 0; i < len && o < n - 4; i++) {
        uint16_t ch = c[i];
        if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
        if (ch < 0x80) out[o++] = (char)ch;
        else if (ch < 0x800) { out[o++] = (char)(0xC0 | (ch >> 6)); out[o++] = (char)(0x80 | (ch & 63)); }
        else { out[o++] = (char)(0xE0 | (ch >> 12)); out[o++] = (char)(0x80 | ((ch >> 6) & 63)); out[o++] = (char)(0x80 | (ch & 63)); }
    }
    out[o] = 0;
}
static void oname(void* obj, char* out, int n) {
    out[0] = 0;
    if (obj) u16(inv(M.obj_name, obj, NULL), out, n);
}

/* ---------- элементы сцены ---------- */
typedef struct {
    void* obj;
    void* go;
    int type; /* 0 SpriteRenderer, 1 другой Renderer, 2 Image, 3 RawImage, 4 прочий UI */
    int ui;
    char go_name[64];
    char asset[64];
    char key[160];
    char label[140];
} Item;
#define MAXI 4000
static Item g_items[MAXI];
static int g_n;

static int in_scene(void* go) {
    void* r = inv(M.go_scene, go, NULL);
    return r && *(int32_t*)((char*)r + 16) != 0;
}

static void item_asset(Item* it) {
    void* a = NULL;
    it->asset[0] = 0;
    if (it->type == 0) a = inv(M.sr_spr_get, it->obj, NULL);
    else if (it->type == 1) { void* mat = inv(M.r_shared, it->obj, NULL); if (mat) a = inv(M.mat_tex_get, mat, NULL); }
    else if (it->type == 2) a = inv(M.img_spr_get, it->obj, NULL);
    else if (it->type == 3) a = inv(M.raw_tex_get, it->obj, NULL);
    if (a) oname(a, it->asset, sizeof it->asset);
}

static int fill_item(Item* it, void* obj, int ui) {
    void* go = inv(M.comp_go, obj, NULL);
    if (!go || !in_scene(go)) return 0;
    const char* cn = M.obj_class && M.class_name ? M.class_name(M.obj_class(obj)) : "";
    it->obj = obj;
    it->go = go;
    it->ui = ui;
    if (!ui) it->type = strcmp(cn, "SpriteRenderer") == 0 ? 0 : 1;
    else it->type = strcmp(cn, "Image") == 0 ? 2 : strcmp(cn, "RawImage") == 0 ? 3 : 4;
    oname(go, it->go_name, sizeof it->go_name);
    item_asset(it);
    snprintf(it->key, sizeof it->key, "%c:%s:%s", ui ? 'U' : 'R', it->go_name, it->asset);
    snprintf(it->label, sizeof it->label, "%s [%s]", it->go_name, it->asset);
    return 1;
}

/* ---------- правки ---------- */
typedef struct {
    char key[160];
    int has_color;
    float c[4];
    int hidden;
    char repl[64];
    char orig_repl[64];
    int has_oc;
    float oc[4];
    int restore;
} Edit;
#define MAXE 256
static Edit g_ed[MAXE];
static int g_ne;

static Edit* find_edit(const char* key) {
    for (int i = 0; i < g_ne; i++) if (!strcmp(g_ed[i].key, key)) return &g_ed[i];
    return NULL;
}
static Edit* get_edit(const char* key) {
    Edit* e = find_edit(key);
    if (e) return e;
    if (g_ne >= MAXE) return NULL;
    e = &g_ed[g_ne++];
    memset(e, 0, sizeof *e);
    snprintf(e->key, sizeof e->key, "%s", key);
    return e;
}
static void compact_edits(void) {
    int j = 0;
    for (int i = 0; i < g_ne; i++) if (!g_ed[i].restore) g_ed[j++] = g_ed[i];
    g_ne = j;
}

static void set_enabled(Item* it, int on) {
    unsigned char b = on ? 1 : 0;
    void* a[1] = { &b };
    inv(it->type <= 1 ? M.r_enabled : M.beh_enabled, it->obj, a);
}
static void set_color(Item* it, float* c) {
    void* a[1] = { c };
    if (it->type == 0) inv(M.sr_col_set, it->obj, a);
    else if (it->type == 1) { void* mat = inv(M.r_shared, it->obj, NULL); if (mat) inv(M.mat_col_set, mat, a); }
    else inv(M.g_col_set, it->obj, a);
}
static int get_color(Item* it, float* out) {
    void* r = NULL;
    if (it->type == 0) r = inv(M.sr_col_get, it->obj, NULL);
    else if (it->type == 1) { void* mat = inv(M.r_shared, it->obj, NULL); if (mat) r = inv(M.mat_col_get, mat, NULL); }
    else r = inv(M.g_col_get, it->obj, NULL);
    if (!r) return 0;
    memcpy(out, (char*)r + 16, 16);
    return 1;
}

static void* find_asset(void* type, const char* name) {
    void* arr = inv(M.find_all, NULL, (void*[]){ type });
    if (!arr) return NULL;
    uintptr_t len = *(uintptr_t*)((char*)arr + 0x18);
    void** d = (void**)((char*)arr + 0x20);
    char nm[96];
    for (uintptr_t i = 0; i < len; i++) {
        if (!d[i]) continue;
        oname(d[i], nm, sizeof nm);
        if (!strcmp(nm, name)) return d[i];
    }
    return NULL;
}

static void replace_asset(Item* it, const char* name) {
    if (!name[0]) return;
    if (it->type == 0 || it->type == 2) {
        void* s = find_asset(M.t_sprite, name);
        if (s) { void* a[1] = { s }; inv(it->type == 0 ? M.sr_spr_set : M.img_spr_set, it->obj, a); }
    } else if (it->type == 1 || it->type == 3) {
        void* tex = find_asset(M.t_texture, name);
        if (!tex) { void* s = find_asset(M.t_sprite, name); if (s) tex = inv(M.spr_tex, s, NULL); }
        if (!tex) return;
        void* a[1] = { tex };
        if (it->type == 3) inv(M.raw_tex_set, it->obj, a);
        else { void* mat = inv(M.r_shared, it->obj, NULL); if (mat) inv(M.mat_tex_set, mat, a); }
    }
}

static void apply_item(Item* it) {
    Edit* e = find_edit(it->key);
    if (!e) return;
    if (e->restore) {
        if (e->hidden) set_enabled(it, 1);
        if (e->has_oc) set_color(it, e->oc);
        if (e->repl[0] && e->orig_repl[0]) replace_asset(it, e->orig_repl);
        return;
    }
    if (e->hidden) set_enabled(it, 0);
    if (e->has_color) set_color(it, e->c);
    if (e->repl[0]) replace_asset(it, e->repl);
}

static int match_filter(const char* label, const char* f) {
    if (!f[0]) return 1;
    size_t n = strlen(f);
    for (const char* p = label; *p; p++) {
        size_t i = 0;
        while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)f[i])) i++;
        if (i == n) return 1;
    }
    return 0;
}

/* обходит объекты: применяет правки; если collect - собирает список */
static void scan(int ui, int collect, const char* filter) {
    void* arr = inv(M.find_all, NULL, (void*[]){ ui ? M.t_graphic : M.t_renderer });
    if (!arr) return;
    uintptr_t len = *(uintptr_t*)((char*)arr + 0x18);
    void** d = (void**)((char*)arr + 0x20);
    Item tmp;
    for (uintptr_t i = 0; i < len; i++) {
        if (!d[i]) continue;
        if (!fill_item(&tmp, d[i], ui)) continue;
        apply_item(&tmp);
        if (collect && g_n < MAXI && match_filter(tmp.label, filter)) g_items[g_n++] = tmp;
    }
}

/* ---------- показать/скрыть UI ---------- */
static char g_rev[128][64];
static int g_nrev;
static int rev_has(const char* n) {
    for (int i = 0; i < g_nrev; i++) if (!strcmp(g_rev[i], n)) return 1;
    return 0;
}
static void reveal_ui(int reveal) {
    void* arr = inv(M.find_all, NULL, (void*[]){ M.t_rect });
    if (!arr) return;
    uintptr_t len = *(uintptr_t*)((char*)arr + 0x18);
    void** d = (void**)((char*)arr + 0x20);
    int cnt = 0;
    char nm[64];
    for (uintptr_t i = 0; i < len; i++) {
        if (!d[i]) continue;
        void* go = inv(M.comp_go, d[i], NULL);
        if (!go || !in_scene(go)) continue;
        void* act = inv(M.go_active, go, NULL);
        int is_active = act && *(unsigned char*)((char*)act + 16);
        oname(go, nm, sizeof nm);
        unsigned char b;
        void* a[1] = { &b };
        if (reveal && !is_active) {
            if (g_nrev < 128 && !rev_has(nm)) snprintf(g_rev[g_nrev++], 64, "%s", nm);
            b = 1; inv(M.go_setactive, go, a); cnt++;
        } else if (!reveal && is_active && rev_has(nm)) {
            b = 0; inv(M.go_setactive, go, a); cnt++;
        }
    }
    lg(reveal ? "показан скрытый UI: %d объектов" : "скрыт UI, который показывали: %d объектов", cnt);
    if (!reveal) g_nrev = 0;
}

/* ---------- сохранение ---------- */
static void edits_path(char* out, size_t n, int mk) {
    char dir[400];
    snprintf(dir, sizeof dir, "%s/editor_all", g_api->data_dir ? g_api->data_dir() : "/data/local/tmp");
    if (mk) mkdir(dir, 0755);
    snprintf(out, n, "%s/edits.txt", dir);
}
static void save_edits(void) {
    char p[460];
    edits_path(p, sizeof p, 1);
    FILE* f = fopen(p, "w");
    if (!f) { lg("не могу записать %s", p); return; }
    int cnt = 0;
    for (int i = 0; i < g_ne; i++) {
        Edit* e = &g_ed[i];
        fprintf(f, "%s\t%d\t%g\t%g\t%g\t%g\t%d\t%s\t%s\t%d\t%g\t%g\t%g\t%g\n", e->key, e->has_color, e->c[0], e->c[1], e->c[2], e->c[3],
                e->hidden, e->repl, e->orig_repl, e->has_oc, e->oc[0], e->oc[1], e->oc[2], e->oc[3]);
        cnt++;
    }
    fclose(f);
    lg("сохранено правок: %d", cnt);
}
static void load_edits(void) {
    char p[460];
    edits_path(p, sizeof p, 0);
    FILE* f = fopen(p, "r");
    if (!f) return;
    char line[800];
    while (fgets(line, sizeof line, f)) {
        char* t[14];
        int n = 0;
        char* s = line;
        while (n < 14) {
            t[n++] = s;
            char* tab = strchr(s, '\t');
            if (!tab) break;
            *tab = 0;
            s = tab + 1;
        }
        if (n < 14) continue;
        char* nl = strchr(t[13], '\n');
        if (nl) *nl = 0;
        Edit* e = get_edit(t[0]);
        if (!e) break;
        e->has_color = atoi(t[1]);
        for (int k = 0; k < 4; k++) e->c[k] = (float)atof(t[2 + k]);
        e->hidden = atoi(t[6]);
        snprintf(e->repl, sizeof e->repl, "%s", t[7]);
        snprintf(e->orig_repl, sizeof e->orig_repl, "%s", t[8]);
        e->has_oc = atoi(t[9]);
        for (int k = 0; k < 4; k++) e->oc[k] = (float)atof(t[10 + k]);
    }
    fclose(f);
    lg("загружено правок: %d", g_ne);
}

/* ---------- главный цикл ---------- */
static double g_next_tick, g_next_scan;
static int g_flip_init, g_last_run, g_last_save, g_last_reset, g_last_reveal;
static int g_last_sel = -1, g_blink;
static void* g_blink_obj;
static int g_blink_type;
static char g_last_list[24], g_last_filter[64];

static void rescan_all_edits_now(void) {
    scan(0, 0, "");
    scan(1, 0, "");
}

static void do_action(Item* it) {
    char act[40];
    snprintf(act, sizeof act, "%s", g_api->get_string(g_id, "act", "Подсветка"));
    if (!strcmp(act, "Список в лог")) {
        lg("--- список (%d), номер = значение «Номер объекта» ---", g_n);
        for (int i = 0; i < g_n && i < 150; i++) lg("#%d %s", i, g_items[i].label);
        return;
    }
    if (!it) { lg("список пуст: нечего выбирать"); return; }
    if (!strcmp(act, "Подсветка")) return;
    Edit* e = get_edit(it->key);
    if (!e) { lg("слишком много правок"); return; }
    e->restore = 0;
    if (!strcmp(act, "Цвет")) {
        if (!e->has_oc) e->has_oc = get_color(it, e->oc);
        e->has_color = 1;
        e->c[0] = (float)g_api->get_float(g_id, "tint_r", 1);
        e->c[1] = (float)g_api->get_float(g_id, "tint_g", 1);
        e->c[2] = (float)g_api->get_float(g_id, "tint_b", 1);
        e->c[3] = (float)g_api->get_float(g_id, "tint_a", 1);
        lg("цвет применён: %s", it->label);
    } else if (!strcmp(act, "Скрыть")) {
        e->hidden = 1;
        lg("скрыто: %s", it->label);
    } else if (!strcmp(act, "Показать")) {
        e->hidden = 0;
        set_enabled(it, 1);
        unsigned char b = 1;
        void* a[1] = { &b };
        inv(M.go_setactive, it->go, a);
        lg("показано: %s", it->label);
    } else if (!strcmp(act, "Заменить")) {
        char nm[64];
        snprintf(nm, sizeof nm, "%s", g_api->get_string(g_id, "src_name", ""));
        if (!nm[0]) { lg("впишите имя картинки в «Заменить на»"); return; }
        if (!e->orig_repl[0]) snprintf(e->orig_repl, sizeof e->orig_repl, "%s", it->asset);
        snprintf(e->repl, sizeof e->repl, "%s", nm);
        lg("замена: %s -> %s", it->label, nm);
    } else if (!strcmp(act, "Сбросить этот")) {
        e->restore = 1;
        rescan_all_edits_now();
        compact_edits();
        lg("сброшено: %s", it->label);
        g_next_scan = 0;
        return;
    }
    rescan_all_edits_now();
    g_next_scan = 0;
}

static void tick(void) {
    double t = now_s();
    if (t < g_next_tick) return;
    g_next_tick = t + 0.3;

    int on = g_api->get_bool(g_id, "ed_on", 0);
    int run = g_api->get_bool(g_id, "run", 0);
    int sv = g_api->get_bool(g_id, "save", 0);
    int rs = g_api->get_bool(g_id, "reset_all", 0);
    int rv = g_api->get_bool(g_id, "reveal_ui", 0);
    char list[24], filter[64];
    snprintf(list, sizeof list, "%s", g_api->get_string(g_id, "list", ""));
    snprintf(filter, sizeof filter, "%s", g_api->get_string(g_id, "filter", ""));
    int ui = !strcmp(list, "UI");
    int sel = (int)g_api->get_int(g_id, "sel", 0);

    int f_run = 0, f_sv = 0, f_rs = 0, f_rv = 0;
    if (g_flip_init) {
        f_run = run != g_last_run; f_sv = sv != g_last_save; f_rs = rs != g_last_reset; f_rv = rv != g_last_reveal;
    }
    g_flip_init = 1;
    g_last_run = run; g_last_save = sv; g_last_reset = rs; g_last_reveal = rv;

    int need = on || g_ne > 0;
    int changed = strcmp(list, g_last_list) || strcmp(filter, g_last_filter);
    if (need && (t >= g_next_scan || changed)) {
        g_next_scan = t + 4.0;
        snprintf(g_last_list, sizeof g_last_list, "%s", list);
        snprintf(g_last_filter, sizeof g_last_filter, "%s", filter);
        g_n = 0;
        if (on) {
            scan(ui, 1, filter);
            if (g_ne > 0) scan(!ui, 0, "");
            if (sel >= g_n) lg("в списке %d объектов (номер %d вне списка)", g_n, sel);
        } else {
            rescan_all_edits_now();
        }
    }
    if (!on) return;

    int idx = g_n ? (sel < g_n ? (sel < 0 ? 0 : sel) : g_n - 1) : -1;
    Item* it = idx >= 0 ? &g_items[idx] : NULL;

    if (idx != g_last_sel) {
        if (g_blink_obj) {
            Item old = { 0 };
            old.obj = g_blink_obj; old.type = g_blink_type;
            set_enabled(&old, 1);
            g_blink_obj = NULL;
        }
        g_last_sel = idx;
        if (it) lg("выбрано #%d из %d: %s", idx, g_n, it->label);
    }
    if (f_run) do_action(it);
    if (f_sv) save_edits();
    if (f_rv) reveal_ui(1);
    if (f_rs) {
        for (int i = 0; i < g_ne; i++) g_ed[i].restore = 1;
        rescan_all_edits_now();
        g_ne = 0;
        reveal_ui(0);
        char p[460];
        edits_path(p, sizeof p, 0);
        remove(p);
        lg("всё сброшено, сохранённые правки удалены");
    }

    Edit* ce = it ? find_edit(it->key) : NULL;
    if (it && !(ce && ce->hidden)) {
        g_blink = !g_blink;
        g_blink_obj = it->obj;
        g_blink_type = it->type;
        set_enabled(it, g_blink);
    }
}

static void* hook_get_main(const void* m) {
    void* cam = orig_get_main(m);
    if (cam && !g_busy) {
        g_busy = 1;
        tick();
        g_busy = 0;
    }
    return cam;
}

__attribute__((visibility("default")))
void bearite_mod_init(const BeariteApi* api, const char* mod_id) {
    if (api->version != BEARITE_API_VERSION) return;
    g_api = api;
    if (mod_id) g_id = mod_id;
    lg("EDITOR ALL: запуск");
    if (!bearite_il2cpp_init(api)) { lg("IL2CPP не найден, выхожу"); return; }
    init_methods();
    load_edits();
    if (bearite_hook_method(api, g_id, "UnityEngine", "Camera", "get_main", 0, (void*)hook_get_main, (void**)&orig_get_main) != 0) {
        lg("не удалось подключиться к камере, мод не работает");
        return;
    }
    lg("EDITOR ALL: готов");
}
