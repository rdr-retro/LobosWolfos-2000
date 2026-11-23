/* ==========================================================================\n+ * Vista Inicio: arbol de escenas, lienzo de nodos e inspector del ProjectModel.\n+ *\n+ * Esta vista no crea piezas paralelas ni transforma el modelo en tarjetas.\n+ * Todo lo que se dibuja se busca por ID en ProjectModel, de modo que el mismo\n+ * .log puede alimentar despues al compilador, al motor y a las utilidades.\n+ * ========================================================================== */
#include "app.h"
#include "inicio.h"

#define INICIO_PANEL       0x00212C3Au
#define INICIO_PANEL_2     0x002A3948u
#define INICIO_LINEA       0x00465A6Eu
#define INICIO_TEXTO       0x00E8EEF4u
#define INICIO_MUTED       0x0099AABDu
#define INICIO_ACCENTO     0x0049B6FFu
#define INICIO_SELECCION   0x003278E0u
#define INICIO_NODO        0x003A536Bu
#define INICIO_NODO_ACTOR  0x004F805Fu
#define INICIO_NODO_CAMARA 0x006C5B8Au
#define INICIO_FILA_ALTO   22
#define INICIO_ARBOL_ANCHO 280
#define INICIO_INSPECTOR   310
#define INICIO_MARGEN      14
#define INICIO_BOTON_ANCHO 58
#define INICIO_BOTON_ALTO  20
#define INICIO_BOTON_HUECO 4
#define INICIO_BOTONES     7

/* La vista usa el mismo Inicio del servicio CLI, asi que cada comando visual
 * comparte snapshots con --inicio. Estas dos funciones se definen mas abajo. */
static void ultimo_fijar(Inicio *in, const char *texto);
static void guardar_pasado(Inicio *in);

typedef struct InicioFila {
    int id;
    int kind;
    int nivel;
} InicioFila;

static ProjectModel *g_inicio_modelo;
static char g_inicio_ruta[MAX_PATH];
static int g_inicio_seleccion = -1;
static int g_inicio_scroll;
static InicioFila g_inicio_filas[PM_ENTIDADES_MAX + 16];
static int g_inicio_num_filas;
static int g_inicio_editando;
static int g_inicio_edit_prop = -1; /* -2 = nombre, >= 0 = propiedad */
static int g_inicio_edit_primer_caracter;
static char g_inicio_borrador[PM_TEXTO_MAX];
static char g_inicio_edit_error[PM_ERROR_MAX];
static int g_inicio_reparentar_origen = -1;

/* Orden de los botones de la barra del lienzo. El texto es corto para que
 * los siete quepan en el ancho minimo del panel central. */
static const char *const g_inicio_botones[INICIO_BOTONES] = {
    "ESCENA", "NODO", "BORRAR", "PADRE", "RAIZ", "DESH", "REH"
};
enum {
    INICIO_B_ESCENA = 0, INICIO_B_NODO, INICIO_B_BORRAR,
    INICIO_B_PADRE, INICIO_B_RAIZ, INICIO_B_DESH, INICIO_B_REH
};

static Zona inicio_cuerpo(void) {
    int alto = g.alto - AREA_SUP_ALTO - BARRA_ABAJO_ALTO;
    if (alto < 80) alto = 80;
    return zona_hacer(0, AREA_SUP_ALTO, g.ancho, alto);
}

static Zona inicio_arbol(void) {
    Zona c = inicio_cuerpo();
    return zona_hacer(0, c.y, INICIO_ARBOL_ANCHO, c.alto);
}

static Zona inicio_lienzo(void) {
    Zona c = inicio_cuerpo();
    int x = INICIO_ARBOL_ANCHO;
    int ancho = g.ancho - INICIO_ARBOL_ANCHO - INICIO_INSPECTOR;
    if (ancho < 120) ancho = 120;
    return zona_hacer(x, c.y, ancho, c.alto);
}

static Zona inicio_inspector(void) {
    Zona c = inicio_cuerpo();
    int x = g.ancho - INICIO_INSPECTOR;
    if (x < INICIO_ARBOL_ANCHO + 120) x = INICIO_ARBOL_ANCHO + 120;
    return zona_hacer(x, c.y, g.ancho - x, c.alto);
}

static const EntidadModelo *inicio_entidad(int id) {
    if (!g_inicio_modelo || id <= 0) return NULL;
    return pm_entidad_const(g_inicio_modelo, id);
}

static const char *inicio_prop(const EntidadModelo *e, const char *clave) {
    int i;
    if (!e || !clave) return "";
    for (i = 0; i < e->num_propiedades; i++)
        if (strcmp(e->propiedades[i].clave, clave) == 0)
            return e->propiedades[i].valor;
    return "";
}

static int inicio_prop_int(const EntidadModelo *e, const char *clave, int por_defecto) {
    const char *valor = inicio_prop(e, clave);
    char *fin;
    long n;
    if (!valor[0]) return por_defecto;
    n = strtol(valor, &fin, 10);
    if (fin == valor) return por_defecto;
    return (int)n;
}

static int inicio_ref(const EntidadModelo *e, const char *nombre) {
    int i;
    if (!e || !nombre) return 0;
    for (i = 0; i < e->num_referencias; i++)
        if (strcmp(e->referencias[i].nombre, nombre) == 0)
            return e->referencias[i].id;
    return 0;
}

static int inicio_es_nodo_de(const EntidadModelo *e, int escena) {
    return e && e->kind == PM_KIND_NODO &&
           inicio_prop_int(e, "escena", inicio_ref(e, "escena")) == escena;
}

static int inicio_escena_inicial(void) {
    int i;
    if (!g_inicio_modelo) return 0;
    for (i = 0; i < g_inicio_modelo->num_entidades; i++)
        if (g_inicio_modelo->entidades[i].kind == PM_KIND_ESCENA &&
            inicio_prop_int(&g_inicio_modelo->entidades[i], "inicial", 0))
            return g_inicio_modelo->entidades[i].id;
    for (i = 0; i < g_inicio_modelo->num_entidades; i++)
        if (g_inicio_modelo->entidades[i].kind == PM_KIND_ESCENA)
            return g_inicio_modelo->entidades[i].id;
    return 0;
}

static int inicio_escena_actual(void) {
    const EntidadModelo *e = inicio_entidad(g_inicio_seleccion);
    int escena;
    if (e) {
        if (e->kind == PM_KIND_ESCENA) return e->id;
        escena = inicio_prop_int(e, "escena", inicio_ref(e, "escena"));
        if (escena > 0) return escena;
    }
    return inicio_escena_inicial();
}

static int inicio_nodo_padre(const EntidadModelo *e) {
    return inicio_prop_int(e, "padre", inicio_ref(e, "padre"));
}

static int inicio_nodo_nivel(const EntidadModelo *e) {
    int id = inicio_nodo_padre(e);
    int nivel = 0;
    int guardia = 0;
    while (id > 0 && guardia++ < PM_ENTIDADES_MAX) {
        const EntidadModelo *p = inicio_entidad(id);
        if (!p || p->kind != PM_KIND_NODO) break;
        nivel++;
        id = inicio_nodo_padre(p);
    }
    if (nivel > 8) nivel = 8;
    return nivel;
}

static void inicio_fila_anadir(int id, int kind, int nivel) {
    if (g_inicio_num_filas >= (int)(sizeof(g_inicio_filas) / sizeof(g_inicio_filas[0])))
        return;
    g_inicio_filas[g_inicio_num_filas].id = id;
    g_inicio_filas[g_inicio_num_filas].kind = kind;
    g_inicio_filas[g_inicio_num_filas].nivel = nivel;
    g_inicio_num_filas++;
}

static void inicio_filas_construir(void) {
    int i, j;
    g_inicio_num_filas = 0;
    if (!g_inicio_modelo) return;
    for (i = 0; i < g_inicio_modelo->num_entidades; i++) {
        const EntidadModelo *escena = &g_inicio_modelo->entidades[i];
        if (escena->kind != PM_KIND_ESCENA) continue;
        inicio_fila_anadir(escena->id, escena->kind, 0);
        for (j = 0; j < g_inicio_modelo->num_entidades; j++) {
            const EntidadModelo *nodo = &g_inicio_modelo->entidades[j];
            if (inicio_es_nodo_de(nodo, escena->id))
                inicio_fila_anadir(nodo->id, nodo->kind, inicio_nodo_nivel(nodo) + 1);
        }
    }
    for (i = 0; i < g_inicio_modelo->num_entidades; i++)
        if (g_inicio_modelo->entidades[i].kind == PM_KIND_RECURSO)
            inicio_fila_anadir(g_inicio_modelo->entidades[i].id,
                               PM_KIND_RECURSO, 0);
    for (i = 0; i < g_inicio_modelo->num_entidades; i++)
        if (g_inicio_modelo->entidades[i].kind == PM_KIND_SCRIPT)
            inicio_fila_anadir(g_inicio_modelo->entidades[i].id,
                               PM_KIND_SCRIPT, 0);
    if (g_inicio_scroll < 0) g_inicio_scroll = 0;
    if (g_inicio_scroll >= g_inicio_num_filas && g_inicio_num_filas > 0)
        g_inicio_scroll = g_inicio_num_filas - 1;
}

static void inicio_diagnostico(char *diagnostico, size_t longitud,
                               const char *texto) {
    if (diagnostico && longitud > 0) {
        strncpy(diagnostico, texto ? texto : "", longitud - 1);
        diagnostico[longitud - 1] = 0;
    }
    strncpy(g_inicio_edit_error, texto ? texto : "", sizeof(g_inicio_edit_error) - 1);
    g_inicio_edit_error[sizeof(g_inicio_edit_error) - 1] = 0;
}

static int inicio_entero_exacto(const char *texto, int *salida) {
    char *fin;
    long valor;
    if (!texto || !texto[0] || !salida) return 0;
    valor = strtol(texto, &fin, 10);
    if (*fin || valor < -2147483647L || valor > 2147483647L) return 0;
    *salida = (int)valor;
    return 1;
}

static int inicio_propiedad_conocida(const EntidadModelo *e, const char *campo) {
    if (!e || !campo || !campo[0]) return 0;
    if (strcmp(campo, "nombre") == 0) return 0;
    if (e->kind == PM_KIND_NODO) {
        return strcmp(campo, "x") == 0 || strcmp(campo, "y") == 0 ||
               strcmp(campo, "w") == 0 || strcmp(campo, "h") == 0 ||
               strcmp(campo, "visible") == 0 || strcmp(campo, "capa") == 0 ||
               strcmp(campo, "zoom") == 0 || strcmp(campo, "tipo") == 0;
    }
    if (e->kind == PM_KIND_ESCENA)
        return strcmp(campo, "inicial") == 0 || strcmp(campo, "ancho") == 0 ||
               strcmp(campo, "alto") == 0 || strcmp(campo, "fps") == 0 ||
               strcmp(campo, "fondo") == 0;
    if (e->kind == PM_KIND_PROYECTO)
        return strcmp(campo, "ancho") == 0 || strcmp(campo, "alto") == 0 ||
               strcmp(campo, "fps") == 0 || strcmp(campo, "fondo") == 0;
    return 0;
}

static int inicio_valor_propiedad_valido(const EntidadModelo *e,
                                         const char *campo, const char *valor) {
    int n;
    int entero = 0;
    if (!e || !campo || !valor) return 0;
    if (strcmp(campo, "x") == 0 || strcmp(campo, "y") == 0 ||
        strcmp(campo, "w") == 0 || strcmp(campo, "h") == 0 ||
        strcmp(campo, "visible") == 0 || strcmp(campo, "capa") == 0 ||
        strcmp(campo, "zoom") == 0 || strcmp(campo, "ancho") == 0 ||
        strcmp(campo, "alto") == 0 || strcmp(campo, "fps") == 0)
        entero = 1;
    if (!entero) return 1;
    if (!inicio_entero_exacto(valor, &n)) return 0;
    if ((strcmp(campo, "w") == 0 || strcmp(campo, "h") == 0 ||
         strcmp(campo, "ancho") == 0 || strcmp(campo, "alto") == 0 ||
         strcmp(campo, "fps") == 0) && n <= 0) return 0;
    if (strcmp(campo, "visible") == 0 && n != 0 && n != 1) return 0;
    if (strcmp(campo, "zoom") == 0 && n != 1 && n != 2 && n != 4) return 0;
    return 1;
}

static int inicio_padre_valido(ProjectModel *modelo, EntidadModelo *nodo,
                               int padre) {
    int escena, id, guardia = 0;
    if (padre == 0) return 1;
    if (!modelo || !nodo || padre <= 0 || padre == nodo->id) return 0;
    if (!pm_entidad(modelo, padre) || pm_entidad(modelo, padre)->kind != PM_KIND_NODO)
        return 0;
    escena = inicio_prop_int(nodo, "escena", inicio_ref(nodo, "escena"));
    id = padre;
    while (id > 0 && guardia++ < PM_ENTIDADES_MAX) {
        EntidadModelo *actual = pm_entidad(modelo, id);
        int actual_escena;
        if (!actual || actual->kind != PM_KIND_NODO) return 0;
        actual_escena = inicio_prop_int(actual, "escena", inicio_ref(actual, "escena"));
        if (actual_escena != escena) return 0;
        if (actual->id == nodo->id) return 0;
        id = inicio_nodo_padre(actual);
    }
    return id == 0;
}

int inicio_modelo_editar(int id, const char *campo, const char *valor,
                         char *diagnostico, size_t longitud) {
    Inicio *in = inicio_servicio();
    ProjectModel nuevo;
    EntidadModelo *e;
    int n, i, inicial;
    char validacion[PM_ERROR_MAX];

    if (!g_inicio_modelo) {
        inicio_diagnostico(diagnostico, longitud, "no hay modelo cargado");
        return 0;
    }
    if (!pm_entidad(g_inicio_modelo, id) || !campo || !campo[0] || !valor) {
        inicio_diagnostico(diagnostico, longitud, "entidad o campo inexistente");
        return 0;
    }
    if (!inicio_valor_propiedad_valido(pm_entidad_const(g_inicio_modelo, id),
                                       campo, valor)) {
        inicio_diagnostico(diagnostico, longitud, "valor no valido para el campo");
        return 0;
    }
    /* Se edita una copia: solo se publica si el grafo sigue siendo valido.
     * El snapshot del pasado se guarda en ese momento, nunca antes. */
    pm_copiar(g_inicio_modelo, &nuevo);
    e = pm_entidad(&nuevo, id);

    if (strcmp(campo, "nombre") == 0) {
        if (!valor[0] || !pm_renombrar(e, valor)) {
            inicio_diagnostico(diagnostico, longitud, "el nombre no puede estar vacio");
            return 0;
        }
    } else if (e->kind == PM_KIND_NODO && strcmp(campo, "padre") == 0) {
        if (!inicio_entero_exacto(valor, &n) || n < 0 ||
            !inicio_padre_valido(&nuevo, e, n)) {
            inicio_diagnostico(diagnostico, longitud, "padre invalido o ciclo jerarquico");
            return 0;
        }
        if (!pm_propiedad(e, "padre", valor, e->origen.linea) ||
            (n > 0 ? pm_referencia(e, "padre", n, NULL, e->origen.linea)
                   : (pm_quitar_referencia(e, "padre"), 1))) {
            inicio_diagnostico(diagnostico, longitud, "no se pudo actualizar el padre");
            return 0;
        }
    } else if (strcmp(campo, "escena") == 0 && e->kind == PM_KIND_NODO) {
        inicio_diagnostico(diagnostico, longitud,
                           "la escena de un nodo se cambia desde su montaje");
        return 0;
    } else if (strcmp(campo, "inicial") == 0 && e->kind == PM_KIND_ESCENA) {
        if (!inicio_entero_exacto(valor, &inicial) || (inicial != 0 && inicial != 1)) {
            inicio_diagnostico(diagnostico, longitud, "inicial debe ser 0 o 1");
            return 0;
        }
        for (i = 0; i < nuevo.num_entidades; i++) {
            EntidadModelo *otra = &nuevo.entidades[i];
            if (otra->kind == PM_KIND_ESCENA && inicial)
                pm_propiedad(otra, "inicial", otra->id == e->id ? "1" : "0",
                             otra->origen.linea);
        }
        if (!inicial && !pm_propiedad(e, "inicial", "0", e->origen.linea)) {
            inicio_diagnostico(diagnostico, longitud, "no se pudo actualizar inicial");
            return 0;
        }
    } else if (inicio_propiedad_conocida(e, campo) || inicio_prop(e, campo)[0]) {
        if (!pm_propiedad(e, campo, valor, e->origen.linea)) {
            inicio_diagnostico(diagnostico, longitud, "no caben mas propiedades");
            return 0;
        }
    } else {
        inicio_diagnostico(diagnostico, longitud, "campo no editable en esta entidad");
        return 0;
    }

    if (pm_validar(&nuevo, validacion, sizeof(validacion))) {
        inicio_diagnostico(diagnostico, longitud, validacion);
        return 0;
    }
    guardar_pasado(in);
    pm_copiar(&nuevo, &in->modelo);
    ultimo_fijar(in, "editado");
    inicio_diagnostico(diagnostico, longitud, "sin errores");
    inicio_filas_construir();
    app_invalidar();
    return 1;
}static void inicio_escribir_valor(FILE *f, const char *clave, const char *valor) {
    const char *p = valor ? valor : "";
    if (clave && clave[0]) fprintf(f, " %s \"", clave);
    else fputs(" \"", f);
    while (*p) {
        fputc(*p == '"' ? '\'' : *p, f);
        p++;
    }
    fputc('"', f);
}

static int inicio_prop_saltar(const char *clave, const char *a, const char *b,
                              const char *c, const char *d, const char *e,
                              const char *f) {
    return (a && strcmp(clave, a) == 0) || (b && strcmp(clave, b) == 0) ||
           (c && strcmp(clave, c) == 0) || (d && strcmp(clave, d) == 0) ||
           (e && strcmp(clave, e) == 0) || (f && strcmp(clave, f) == 0);
}

static void inicio_escribir_props(FILE *f, const EntidadModelo *e,
                                  const char *a, const char *b,
                                  const char *c, const char *d,
                                  const char *extra, const char *extra2) {
    int i;
    if (!f || !e) return;
    for (i = 0; i < e->num_propiedades; i++) {
        const PropiedadModelo *p = &e->propiedades[i];
        if (inicio_prop_saltar(p->clave, a, b, c, d, extra, extra2)) continue;
        inicio_escribir_valor(f, p->clave, p->valor);
    }
}

int inicio_modelo_guardar(const char *ruta, char *diagnostico, size_t longitud) {
    FILE *f;
    const EntidadModelo *e;
    const ReferenciaModelo *objetivo;
    int i, j, padre, recurso;
    char validacion[PM_ERROR_MAX];
    if (!g_inicio_modelo || !ruta || !ruta[0]) {
        inicio_diagnostico(diagnostico, longitud, "no hay modelo o ruta de salida");
        return 0;
    }
    if (pm_validar(g_inicio_modelo, validacion, sizeof(validacion))) {
        inicio_diagnostico(diagnostico, longitud, validacion);
        return 0;
    }
    f = fopen(ruta, "w");
    if (!f) {
        inicio_diagnostico(diagnostico, longitud, "no se pudo abrir la salida");
        return 0;
    }
    fprintf(f, "LOG 2\n\n");
    e = NULL;
    for (i = 0; i < g_inicio_modelo->num_entidades; i++)
        if (g_inicio_modelo->entidades[i].kind == PM_KIND_PROYECTO) {
            e = &g_inicio_modelo->entidades[i];
            break;
        }
    if (e) {
        fprintf(f, "proyecto");
        inicio_escribir_valor(f, "nombre", e->nombre);
        inicio_escribir_props(f, e, "nombre", NULL, NULL, NULL, NULL, NULL);
        fprintf(f, "\n\n");
    }
    for (j = 0; j < 4; j++) {
        int kind = j == 0 ? PM_KIND_RECURSO : j == 1 ? PM_KIND_SCRIPT :
                   j == 2 ? PM_KIND_ESCENA : PM_KIND_NODO;
        for (i = 0; i < g_inicio_modelo->num_entidades; i++) {
            e = &g_inicio_modelo->entidades[i];
            if (e->kind != kind) continue;
            if (kind == PM_KIND_RECURSO) {
                fprintf(f, "recurso id %d", e->id);
                inicio_escribir_valor(f, "tipo", inicio_prop(e, "tipo"));
                inicio_escribir_valor(f, "nombre", e->nombre);
                inicio_escribir_valor(f, "ruta", inicio_prop(e, "ruta"));
                inicio_escribir_props(f, e, "tipo", "nombre", "ruta", "id", NULL, NULL);
            } else if (kind == PM_KIND_SCRIPT) {
                fprintf(f, "script id %d", e->id);
                inicio_escribir_valor(f, "nombre", e->nombre);
                inicio_escribir_valor(f, "ruta", inicio_prop(e, "ruta"));
                inicio_escribir_props(f, e, "nombre", "ruta", "id", NULL, NULL, NULL);
            } else if (kind == PM_KIND_ESCENA) {
                fprintf(f, "escena id %d", e->id);
                inicio_escribir_valor(f, "nombre", e->nombre);
                inicio_escribir_props(f, e, "nombre", "id", NULL, NULL, NULL, NULL);
            } else {
                padre = inicio_nodo_padre(e);
                recurso = inicio_ref(e, "recurso");
                fprintf(f, "nodo id %d escena %d padre %d", e->id,
                        inicio_prop_int(e, "escena", inicio_ref(e, "escena")), padre);
                inicio_escribir_valor(f, "tipo", inicio_prop(e, "tipo"));
                inicio_escribir_valor(f, "nombre", e->nombre);
                inicio_escribir_props(f, e, "id", "escena", "padre", "tipo", "nombre", "recurso");
                if (recurso > 0) fprintf(f, " recurso %d", recurso);
            }
            fprintf(f, "\n");
        }
        if (kind == PM_KIND_ESCENA) fprintf(f, "\n");
    }
    for (i = 0; i < g_inicio_modelo->num_entidades; i++) {
        e = &g_inicio_modelo->entidades[i];
        if (e->kind != PM_KIND_BINDING) continue;
        objetivo = NULL;
        for (j = 0; j < e->num_referencias; j++)
            if (strcmp(e->referencias[j].nombre, "nodo") == 0 ||
                strcmp(e->referencias[j].nombre, "escena") == 0) {
                objetivo = &e->referencias[j];
                break;
            }
        fprintf(f, "asociar script %d %s %d evento", inicio_ref(e, "script"),
                objetivo && strcmp(objetivo->nombre, "escena") == 0 ? "escena" : "nodo",
                objetivo ? objetivo->id : 0);
        inicio_escribir_valor(f, "", inicio_prop(e, "evento"));
        fprintf(f, "\n");
    }
    fprintf(f, "FIN\n");
    fclose(f);
    recortar_texto(g_inicio_ruta, ruta, sizeof(g_inicio_ruta));
    inicio_diagnostico(diagnostico, longitud, "sin errores");
    return 1;
}

static void inicio_texto_recortado(int x, int y, const char *texto,
                                   u32 color, int ancho) {
    char corto[PM_TEXTO_MAX + PM_RUTA_MAX];
    int maximo = (ancho - 4) / 7;
    if (maximo < 1) return;
    recortar_texto(corto, texto ? texto : "", sizeof(corto));
    if ((int)strlen(corto) > maximo) {
        corto[maximo] = 0;
        if (maximo > 3) {
            corto[maximo - 1] = '.';
            corto[maximo - 2] = '.';
            corto[maximo - 3] = '.';
        }
    }
    fuente_dibujar(x, y, corto, color);
}

static void inicio_etiqueta_entidad(char *dst, int tam,
                                    const EntidadModelo *e) {
    if (!e) { dst[0] = 0; return; }
    if (e->kind == PM_KIND_ESCENA)
        _snprintf(dst, tam, "ESCENA  %s", e->nombre);
    else if (e->kind == PM_KIND_NODO)
        _snprintf(dst, tam, "%s  %s", inicio_prop(e, "tipo"), e->nombre);
    else
        _snprintf(dst, tam, "%s  %s", pm_kind_nombre(e->kind), e->nombre);
    dst[tam - 1] = 0;
}

static void inicio_dibujar_fila(Zona panel, int indice, int y) {
    const InicioFila *fila;
    const EntidadModelo *e;
    char texto[PM_NOMBRE_MAX + 32];
    int x;
    Zona z;
    if (indice < 0 || indice >= g_inicio_num_filas) return;
    fila = &g_inicio_filas[indice];
    e = inicio_entidad(fila->id);
    if (!e) return;
    z = zona_hacer(panel.x + 6, y, panel.ancho - 12, INICIO_FILA_ALTO);
    if (fila->id == g_inicio_seleccion) dibujar_rect_z(z, INICIO_SELECCION);
    x = z.x + 8 + fila->nivel * 14;
    if (fila->kind == PM_KIND_ESCENA) {
        dibujar_rect(x, y + 7, x + 8, y + 15, INICIO_ACCENTO);
        x += 14;
    } else if (fila->kind == PM_KIND_NODO) {
        dibujar_rect(x, y + 8, x + 7, y + 15, INICIO_NODO_ACTOR);
        x += 13;
    }
    inicio_etiqueta_entidad(texto, sizeof(texto), e);
    inicio_texto_recortado(x, y + 6, texto,
                           fila->id == g_inicio_seleccion ? TEXTO_CLARO : INICIO_TEXTO,
                           zona_derecha(z) - x - 4);
}

static void inicio_dibujar_arbol(void) {
    Zona p = inicio_arbol();
    int i, y;
    dibujar_rect_z(p, INICIO_PANEL);
    dibujar_rect(p.x, p.y, zona_derecha(p), p.y + 34, INICIO_PANEL_2);
    fuente_dibujar(p.x + INICIO_MARGEN, p.y + 11, "ESCENAS Y RECURSOS", INICIO_TEXTO);
    dibujar_linea(p.x, p.y + 34, zona_derecha(p), p.y + 34, INICIO_LINEA);
    for (i = g_inicio_scroll; i < g_inicio_num_filas; i++) {
        y = p.y + 40 + (i - g_inicio_scroll) * INICIO_FILA_ALTO;
        if (y + INICIO_FILA_ALTO > zona_abajo(p) - 10) break;
        inicio_dibujar_fila(p, i, y);
    }
    if (g_inicio_num_filas == 0)
        fuente_dibujar(p.x + INICIO_MARGEN, p.y + 54, "Sin escenas en el modelo", INICIO_MUTED);
}

static void inicio_nodo_posicion(const EntidadModelo *e, Zona lienzo,
                                 int *x, int *y) {
    int px = inicio_prop_int(e, "x", e ? e->ui.x : 0);
    int py = inicio_prop_int(e, "y", e ? e->ui.y : 0);
    *x = lienzo.x + 24 + px;
    *y = lienzo.y + 55 + py;
}

static Zona inicio_nodo_zona(const EntidadModelo *e, Zona lienzo) {
    int x, y, ancho = 156, alto = 48;
    const char *tipo = inicio_prop(e, "tipo");
    inicio_nodo_posicion(e, lienzo, &x, &y);
    if (strcmp(tipo, "camera2d") == 0) { ancho = 142; alto = 36; }
    if (strcmp(tipo, "root") == 0) { ancho = 180; alto = 42; }
    return zona_hacer(x, y, ancho, alto);
}

/* ------------------------------------------------------------------ */
/* Barra de comandos del lienzo Inicio                                 */
/* ------------------------------------------------------------------ */

static Zona inicio_boton_zona(Zona lienzo, int indice) {
    int bloque = INICIO_BOTONES * INICIO_BOTON_ANCHO +
                 (INICIO_BOTONES - 1) * INICIO_BOTON_HUECO;
    int x = zona_derecha(lienzo) - bloque - 8;
    int y = lienzo.y + 6;
    return zona_hacer(x + indice * (INICIO_BOTON_ANCHO + INICIO_BOTON_HUECO),
                      y, INICIO_BOTON_ANCHO, INICIO_BOTON_ALTO);
}

static void inicio_dibujar_botones(Zona lienzo) {
    Inicio *in = inicio_servicio();
    int i, ancho;
    ancho = INICIO_BOTONES * INICIO_BOTON_ANCHO +
            (INICIO_BOTONES - 1) * INICIO_BOTON_HUECO;
    if (zona_derecha(lienzo) - ancho - 8 < lienzo.x + 150) return;
    for (i = 0; i < INICIO_BOTONES; i++) {
        Zona b = inicio_boton_zona(lienzo, i);
        int activo = (i == INICIO_B_PADRE && g_inicio_reparentar_origen > 0) ||
                     (i == INICIO_B_DESH && in->num_pasado > 0) ||
                     (i == INICIO_B_REH && in->num_futuro > 0);
        int sobre = zona_contiene(b, g.raton_x, g.raton_y);
        dibujar_rect_z(b, activo ? INICIO_SELECCION :
                           sobre ? INICIO_NODO : INICIO_PANEL_2);
        dibujar_marco_z(b, activo ? INICIO_ACCENTO : INICIO_LINEA);
        fuente_dibujar(b.x + 5, b.y + 5, g_inicio_botones[i],
                       activo || sobre ? TEXTO_CLARO : INICIO_TEXTO);
    }
}

static int inicio_boton_en(int x, int y) {
    Zona lienzo = inicio_lienzo();
    int i;
    for (i = 0; i < INICIO_BOTONES; i++)
        if (zona_contiene(inicio_boton_zona(lienzo, i), x, y)) return i;
    return -1;
}

static void inicio_nombre_auto(char *dst, size_t tam, int kind) {
    const char *base = kind == PM_KIND_ESCENA ? "Escena" : "Nodo";
    int n;
    for (n = 1; n < 999; n++) {
        int i, libre = 1;
        _snprintf(dst, tam, "%s %d", base, n);
        dst[tam - 1] = 0;
        if (!g_inicio_modelo) return;
        for (i = 0; i < g_inicio_modelo->num_entidades; i++)
            if (strcmp(g_inicio_modelo->entidades[i].nombre, dst) == 0)
                libre = 0;
        if (libre) return;
    }
    dst[0] = 0;
}

static void inicio_accion_fallo(const char *texto) {
    inicio_diagnostico(NULL, 0, texto);
    app_estado("%s", texto);
    app_invalidar();
}

static int inicio_accion_crear(int kind) {
    Inicio *in = inicio_servicio();
    char nombre[PM_NOMBRE_MAX];
    const EntidadModelo *sel;
    int padre = 0, id;

    if (!g_inicio_modelo) {
        inicio_accion_fallo("carga un .log antes de crear nodos");
        return 0;
    }
    sel = inicio_entidad(g_inicio_seleccion);
    if (kind == PM_KIND_NODO && sel && sel->kind == PM_KIND_NODO)
        padre = sel->id;
    inicio_nombre_auto(nombre, sizeof(nombre), kind);
    if (!nombre[0]) {
        inicio_accion_fallo("no queda un nombre libre");
        return 0;
    }
    if (!inicio_crear(in, kind, nombre, padre)) {
        inicio_accion_fallo(in->ultimo);
        return 0;
    }
    id = in->modelo.siguiente_id - 1;
    g_inicio_seleccion = id;
    g_inicio_reparentar_origen = -1;
    inicio_filas_construir();
    app_estado("creado %s #%d", pm_kind_nombre(kind), id);
    app_invalidar();
    return 1;
}

static int inicio_accion_borrar(void) {
    Inicio *in = inicio_servicio();
    const EntidadModelo *e = inicio_entidad(g_inicio_seleccion);
    int id = g_inicio_seleccion;

    if (!e) {
        inicio_accion_fallo("selecciona una entidad para borrar");
        return 0;
    }
    if (!inicio_borrar(in, id)) {
        inicio_accion_fallo(in->ultimo);
        return 0;
    }
    g_inicio_seleccion = -1;
    g_inicio_reparentar_origen = -1;
    inicio_filas_construir();
    app_estado("borrada entidad #%d", id);
    app_invalidar();
    return 1;
}

static int inicio_accion_reparentar(int id, int padre) {
    Inicio *in = inicio_servicio();
    if (!inicio_reparentar(in, id, padre)) {
        inicio_accion_fallo(in->ultimo);
        return 0;
    }
    g_inicio_seleccion = id;
    g_inicio_reparentar_origen = -1;
    inicio_filas_construir();
    app_estado("nodo #%d reparentado a %d", id, padre);
    app_invalidar();
    return 1;
}

static void inicio_accion_marcar_padre(void) {
    const EntidadModelo *e = inicio_entidad(g_inicio_seleccion);
    if (!e || e->kind != PM_KIND_NODO) {
        inicio_accion_fallo("selecciona un nodo antes de elegir padre");
        return;
    }
    g_inicio_reparentar_origen = e->id;
    app_estado("elige el padre de #%d (clic en el lienzo o RAIZ)", e->id);
    app_invalidar();
}

static int inicio_accion_historial(int deshacer) {
    Inicio *in = inicio_servicio();
    int ok = deshacer ? inicio_deshacer(in) : inicio_rehacer(in);
    if (!ok) {
        inicio_accion_fallo(in->ultimo);
        return 0;
    }
    if (g_inicio_seleccion > 0 &&
        !inicio_entidad(g_inicio_seleccion))
        g_inicio_seleccion = -1;
    g_inicio_reparentar_origen = -1;
    inicio_filas_construir();
    app_estado("%s", in->ultimo);
    app_invalidar();
    return 1;
}

static void inicio_dibujar_rejilla(Zona z) {
    int x, y;
    dibujar_rect_z(z, 0x00182430u);
    for (x = z.x + 16; x < zona_derecha(z); x += 32)
        dibujar_linea(x, z.y, x, zona_abajo(z), 0x00263848u);
    for (y = z.y + 16; y < zona_abajo(z); y += 32)
        dibujar_linea(z.x, y, zona_derecha(z), y, 0x00263848u);
    dibujar_linea(z.x + z.ancho / 2, z.y, z.x + z.ancho / 2,
                  zona_abajo(z), 0x003B6475u);
    dibujar_linea(z.x, z.y + z.alto / 2, zona_derecha(z),
                  z.y + z.alto / 2, 0x003B6475u);
}

static void inicio_dibujar_conexion_nodo(const EntidadModelo *e, Zona lienzo) {
    int padre = inicio_nodo_padre(e);
    const EntidadModelo *p = inicio_entidad(padre);
    Zona a, b;
    if (!p || p->kind != PM_KIND_NODO) return;
    a = inicio_nodo_zona(p, lienzo);
    b = inicio_nodo_zona(e, lienzo);
    dibujar_linea(a.x + a.ancho / 2, a.y + a.alto / 2,
                  b.x + b.ancho / 2, b.y + b.alto / 2, INICIO_ACCENTO);
}

static void inicio_dibujar_nodo(const EntidadModelo *e, Zona lienzo) {
    Zona z = inicio_nodo_zona(e, lienzo);
    const char *tipo = inicio_prop(e, "tipo");
    char linea[PM_NOMBRE_MAX + 24];
    u32 color = INICIO_NODO;
    if (strcmp(tipo, "actor") == 0) color = INICIO_NODO_ACTOR;
    if (strcmp(tipo, "camera2d") == 0) color = INICIO_NODO_CAMARA;
    dibujar_rect_z(z, color);
    if (e->id == g_inicio_seleccion) dibujar_marco_z(z, INICIO_ACCENTO);
    else dibujar_marco_z(z, INICIO_LINEA);
    inicio_texto_recortado(z.x + 10, z.y + 8, e->nombre, INICIO_TEXTO,
                           z.ancho - 20);
    _snprintf(linea, sizeof(linea), "%s  #%d", tipo[0] ? tipo : "nodo", e->id);
    linea[sizeof(linea) - 1] = 0;
    inicio_texto_recortado(z.x + 10, z.y + 26, linea, INICIO_MUTED,
                           z.ancho - 20);
}

static void inicio_dibujar_lienzo(void) {
    Zona z = inicio_lienzo();
    int escena = inicio_escena_actual();
    int i;
    const EntidadModelo *e;
    const EntidadModelo *esc = inicio_entidad(escena);
    char titulo[PM_NOMBRE_MAX + 32];
    inicio_dibujar_rejilla(z);
    dibujar_rect(z.x, z.y, zona_derecha(z), z.y + 34, INICIO_PANEL_2);
    _snprintf(titulo, sizeof(titulo), "LIENZO  %s",
              esc ? esc->nombre : "sin escena");
    titulo[sizeof(titulo) - 1] = 0;
    fuente_dibujar(z.x + INICIO_MARGEN, z.y + 11, titulo, INICIO_TEXTO);
    inicio_dibujar_botones(z);
    if (!g_inicio_modelo) {
        fuente_dibujar(z.x + INICIO_MARGEN, z.y + 58,
                       "Carga un .log LOG 2 para comenzar", INICIO_MUTED);
        return;
    }
    for (i = 0; i < g_inicio_modelo->num_entidades; i++) {
        e = &g_inicio_modelo->entidades[i];
        if (inicio_es_nodo_de(e, escena)) inicio_dibujar_conexion_nodo(e, z);
    }
    for (i = 0; i < g_inicio_modelo->num_entidades; i++) {
        e = &g_inicio_modelo->entidades[i];
        if (inicio_es_nodo_de(e, escena)) inicio_dibujar_nodo(e, z);
    }
}

static void inicio_dibujar_inspector(void) {
    Zona p = inicio_inspector();
    const EntidadModelo *e = inicio_entidad(g_inicio_seleccion);
    int y = p.y + 52;
    int i;
    char linea[PM_RUTA_MAX + PM_NOMBRE_MAX + 32];
    dibujar_rect_z(p, INICIO_PANEL);
    dibujar_rect(p.x, p.y, zona_derecha(p), p.y + 34, INICIO_PANEL_2);
    fuente_dibujar(p.x + INICIO_MARGEN, p.y + 11, "INSPECTOR DEL MODELO", INICIO_TEXTO);
    if (!e) {
        fuente_dibujar(p.x + INICIO_MARGEN, y, "Selecciona una entidad", INICIO_MUTED);
        y += 28;
        if (g_inicio_modelo) {
            _snprintf(linea, sizeof(linea), "entidades: %d", g_inicio_modelo->num_entidades);
            fuente_dibujar(p.x + INICIO_MARGEN, y, linea, INICIO_TEXTO);
            y += 20;
            _snprintf(linea, sizeof(linea), "escena activa: #%d", inicio_escena_actual());
            fuente_dibujar(p.x + INICIO_MARGEN, y, linea, INICIO_TEXTO);
        }
        return;
    }
    _snprintf(linea, sizeof(linea), "%s  #%d", pm_kind_nombre(e->kind), e->id);
    linea[sizeof(linea) - 1] = 0;
    fuente_dibujar(p.x + INICIO_MARGEN, y, linea, INICIO_ACCENTO);
    y += 22;
    inicio_texto_recortado(p.x + INICIO_MARGEN, y,
                           g_inicio_editando && g_inicio_edit_prop == -2 ?
                               g_inicio_borrador : e->nombre,
                           g_inicio_editando && g_inicio_edit_prop == -2 ?
                               INICIO_ACCENTO : INICIO_TEXTO,
                           p.ancho - INICIO_MARGEN * 2);
    y += 26;
    if (e->origen.archivo[0]) {
        _snprintf(linea, sizeof(linea), "origen: %s:%d", e->origen.archivo,
                  e->origen.linea);
        linea[sizeof(linea) - 1] = 0;
        inicio_texto_recortado(p.x + INICIO_MARGEN, y, linea, INICIO_MUTED,
                               p.ancho - INICIO_MARGEN * 2);
        y += 24;
    }
    dibujar_linea(p.x + INICIO_MARGEN, y, zona_derecha(p) - INICIO_MARGEN,
                  y, INICIO_LINEA);
    y += 12;
    for (i = 0; i < e->num_propiedades && y < zona_abajo(p) - 28; i++) {
        _snprintf(linea, sizeof(linea), "%s = %s%s", e->propiedades[i].clave,
                  g_inicio_editando && g_inicio_edit_prop == i ?
                      g_inicio_borrador : e->propiedades[i].valor,
                  g_inicio_editando && g_inicio_edit_prop == i ? "_" : "");
        linea[sizeof(linea) - 1] = 0;
        inicio_texto_recortado(p.x + INICIO_MARGEN, y, linea, INICIO_TEXTO,
                               p.ancho - INICIO_MARGEN * 2);
        y += 19;
    }
    if (e->num_referencias > 0 && y < zona_abajo(p) - 28) {
        y += 5;
        dibujar_linea(p.x + INICIO_MARGEN, y, zona_derecha(p) - INICIO_MARGEN,
                      y, INICIO_LINEA);
        y += 12;
        for (i = 0; i < e->num_referencias && y < zona_abajo(p) - 28; i++) {
            _snprintf(linea, sizeof(linea), "%s -> #%d", e->referencias[i].nombre,
                      e->referencias[i].id);
            linea[sizeof(linea) - 1] = 0;
            inicio_texto_recortado(p.x + INICIO_MARGEN, y, linea, INICIO_MUTED,
                                   p.ancho - INICIO_MARGEN * 2);
            y += 19;
        }
    }
    if (g_inicio_editando && y < zona_abajo(p) - 38) {
        fuente_dibujar(p.x + INICIO_MARGEN, zona_abajo(p) - 34,
                       "Enter guarda  Esc cancela", INICIO_MUTED);
    }
    if (g_inicio_edit_error[0] && y < zona_abajo(p) - 18)
        inicio_texto_recortado(p.x + INICIO_MARGEN, zona_abajo(p) - 18,
                               g_inicio_edit_error, 0x00FF9A8Au,
                               p.ancho - INICIO_MARGEN * 2);
}

void inicio_modelo_cargar(ProjectModel *modelo, const char *ruta) {
    Inicio *in = inicio_servicio();
    /* El modelo de la vista es el mismo del servicio: asi los botones y
     * --inicio comparten snapshots, seleccion y diagnostico. */
    inicio_iniciar(in);
    if (modelo) {
        pm_copiar(modelo, &in->modelo);
        pm_destruir(modelo);
        g_inicio_modelo = &in->modelo;
        recortar_texto(in->origen, ruta ? ruta : "", sizeof(in->origen));
        ultimo_fijar(in, "cargado");
    } else {
        g_inicio_modelo = NULL;
    }
    recortar_texto(g_inicio_ruta, ruta ? ruta : "", sizeof(g_inicio_ruta));
    g_inicio_seleccion = -1;
    g_inicio_scroll = 0;
    g_inicio_editando = 0;
    g_inicio_edit_prop = -1;
    g_inicio_edit_primer_caracter = 0;
    g_inicio_borrador[0] = 0;
    g_inicio_edit_error[0] = 0;
    g_inicio_reparentar_origen = -1;
    inicio_filas_construir();
}

void inicio_modelo_limpiar(void) {
    Inicio *in = inicio_servicio();
    /* No se libera g_inicio_modelo: apunta al modelo embebido del servicio. */
    if (g_inicio_modelo == &in->modelo) inicio_iniciar(in);
    g_inicio_modelo = NULL;
    g_inicio_ruta[0] = 0;
    g_inicio_seleccion = -1;
    g_inicio_scroll = 0;
    g_inicio_num_filas = 0;
    g_inicio_editando = 0;
    g_inicio_edit_prop = -1;
    g_inicio_edit_primer_caracter = 0;
    g_inicio_borrador[0] = 0;
    g_inicio_edit_error[0] = 0;
    g_inicio_reparentar_origen = -1;
}

void inicio_dibujar(void) {
    Zona c = inicio_cuerpo();
    char resumen[96];
    dibujar_rect_z(c, FONDO);
    inicio_filas_construir();
    inicio_dibujar_arbol();
    inicio_dibujar_lienzo();
    inicio_dibujar_inspector();
    _snprintf(resumen, sizeof(resumen), "Inicio  |  .log -> ProjectModel  |  %d entidades",
              g_inicio_modelo ? g_inicio_modelo->num_entidades : 0);
    resumen[sizeof(resumen) - 1] = 0;
    dibujar_rect(0, c.y, g.ancho, c.y + 1, INICIO_ACCENTO);
    dibujar_texto(12, g.alto - BARRA_ABAJO_ALTO + 9, resumen, INICIO_TEXTO);
}

static int inicio_fila_en(int x, int y) {
    Zona p = inicio_arbol();
    int i;
    if (!zona_contiene(p, x, y)) return -1;
    i = g_inicio_scroll + (y - (p.y + 40)) / INICIO_FILA_ALTO;
    if (i < 0 || i >= g_inicio_num_filas) return -1;
    if (y < p.y + 40 || y >= p.y + 40 +
        (g_inicio_num_filas - g_inicio_scroll) * INICIO_FILA_ALTO)
        return -1;
    return i;
}

static int inicio_nodo_en(int x, int y) {
    Zona z = inicio_lienzo();
    int escena = inicio_escena_actual();
    int i;
    if (!zona_contiene(z, x, y) || !g_inicio_modelo) return -1;
    for (i = g_inicio_modelo->num_entidades - 1; i >= 0; i--) {
        const EntidadModelo *e = &g_inicio_modelo->entidades[i];
        if (inicio_es_nodo_de(e, escena) && zona_contiene(inicio_nodo_zona(e, z), x, y))
            return e->id;
    }
    return -1;
}

static int inicio_inspector_campo_en(int x, int y) {
    Zona p = inicio_inspector();
    const EntidadModelo *e = inicio_entidad(g_inicio_seleccion);
    int cursor, i;
    if (!e || !zona_contiene(p, x, y)) return -1;
    cursor = p.y + 52;
    cursor += 22;
    if (y >= cursor - 6 && y < cursor + 23) return -2;
    cursor += 26;
    if (e->origen.archivo[0]) cursor += 24;
    cursor += 24; /* separador y margen */
    for (i = 0; i < e->num_propiedades; i++) {
        if (cursor >= zona_abajo(p) - 28) break;
        if (y >= cursor - 4 && y < cursor + 17) return i;
        cursor += 19;
    }
    return -1;
}

static void inicio_editar_iniciar(int campo) {
    const EntidadModelo *e = inicio_entidad(g_inicio_seleccion);
    if (!e || (campo < 0 && campo != -2)) return;
    g_inicio_editando = 1;
    g_inicio_edit_prop = campo;
    g_inicio_edit_primer_caracter = 1;
    if (campo == -2)
        recortar_texto(g_inicio_borrador, e->nombre, sizeof(g_inicio_borrador));
    else if (campo < e->num_propiedades)
        recortar_texto(g_inicio_borrador, e->propiedades[campo].valor,
                       sizeof(g_inicio_borrador));
    else {
        g_inicio_editando = 0;
        g_inicio_edit_prop = -1;
        g_inicio_edit_primer_caracter = 0;
        return;
    }
    g_inicio_edit_error[0] = 0;
    app_estado("editando %s", campo == -2 ? "nombre" : e->propiedades[campo].clave);
    app_invalidar();
}

void inicio_pulsar(int x, int y, int boton) {
    int fila, id, campo, boton_barra;
    if (boton != 0) return;
    inicio_filas_construir();
    boton_barra = inicio_boton_en(x, y);
    if (boton_barra >= 0) {
        switch (boton_barra) {
        case INICIO_B_ESCENA: inicio_accion_crear(PM_KIND_ESCENA); break;
        case INICIO_B_NODO:   inicio_accion_crear(PM_KIND_NODO); break;
        case INICIO_B_BORRAR: inicio_accion_borrar(); break;
        case INICIO_B_PADRE:  inicio_accion_marcar_padre(); break;
        case INICIO_B_RAIZ:
            if (g_inicio_reparentar_origen > 0)
                inicio_accion_reparentar(g_inicio_reparentar_origen, 0);
            else if (inicio_entidad(g_inicio_seleccion))
                inicio_accion_reparentar(g_inicio_seleccion, 0);
            else
                inicio_accion_fallo("selecciona un nodo antes de quitar el padre");
            break;
        case INICIO_B_DESH: inicio_accion_historial(1); break;
        case INICIO_B_REH:  inicio_accion_historial(0); break;
        }
        return;
    }
    campo = inicio_inspector_campo_en(x, y);
    if (campo >= 0 || campo == -2) {
        inicio_editar_iniciar(campo);
        return;
    }
    fila = inicio_fila_en(x, y);
    id = fila >= 0 ? g_inicio_filas[fila].id : inicio_nodo_en(x, y);
    if (g_inicio_reparentar_origen > 0) {
        if (id > 0)
            inicio_accion_reparentar(g_inicio_reparentar_origen, id);
        else if (zona_contiene(inicio_lienzo(), x, y))
            inicio_accion_reparentar(g_inicio_reparentar_origen, 0);
        else {
            g_inicio_reparentar_origen = -1;
            app_estado("reparentado cancelado");
            app_invalidar();
        }
        return;
    }
    if (id > 0) {
        g_inicio_seleccion = id;
        app_estado("entidad #%d seleccionada", id);
        app_invalidar();
    }
}

void inicio_soltar(int x, int y, int boton) { (void)x; (void)y; (void)boton; }
void inicio_mover(int x, int y, int botones) { (void)x; (void)y; (void)botones; }

void inicio_rueda(int x, int y, int delta, int ctrl) {
    Zona p = inicio_arbol();
    if (ctrl || !zona_contiene(p, x, y)) return;
    if (delta > 0 && g_inicio_scroll > 0) g_inicio_scroll--;
    if (delta < 0 && g_inicio_scroll + 1 < g_inicio_num_filas) g_inicio_scroll++;
    app_invalidar();
}

void inicio_tecla(int vk, int ctrl, int shift) {
    char diagnostico[PM_ERROR_MAX];
    if (g_inicio_editando) {
        if (vk == VK_ESCAPE) {
            g_inicio_editando = 0;
            g_inicio_edit_prop = -1;
            g_inicio_edit_primer_caracter = 0;
            g_inicio_edit_error[0] = 0;
            app_estado("edicion cancelada");
            app_invalidar();
            return;
        }
        if (vk == VK_BACK) {
            size_t n = strlen(g_inicio_borrador);
            g_inicio_edit_primer_caracter = 0;
            if (n > 0) g_inicio_borrador[n - 1] = 0;
            app_invalidar();
            return;
        }
        if (vk == VK_RETURN) {
            diagnostico[0] = 0;
            if (inicio_modelo_editar(g_inicio_seleccion,
                                     g_inicio_edit_prop == -2 ? "nombre" :
                                         pm_entidad_const(g_inicio_modelo,
                                                          g_inicio_seleccion)->propiedades[g_inicio_edit_prop].clave,
                                     g_inicio_borrador, diagnostico,
                                     sizeof(diagnostico))) {
                g_inicio_editando = 0;
                g_inicio_edit_prop = -1;
                g_inicio_edit_primer_caracter = 0;
                g_inicio_edit_error[0] = 0;
                app_estado("cambio guardado en el modelo");
            } else {
                strncpy(g_inicio_edit_error, diagnostico,
                        sizeof(g_inicio_edit_error) - 1);
                g_inicio_edit_error[sizeof(g_inicio_edit_error) - 1] = 0;
                app_estado("no se pudo guardar: %s", diagnostico);
            }
            app_invalidar();
            return;
        }
        return;
    }
    if (vk == VK_ESCAPE) {
        if (g_inicio_reparentar_origen > 0) {
            g_inicio_reparentar_origen = -1;
            app_estado("reparentado cancelado");
        } else {
            g_inicio_seleccion = -1;
        }
        app_invalidar();
        return;
    }
    if (ctrl && (vk == 'Z' || vk == 'z')) {
        inicio_accion_historial(!shift);
        return;
    }
    if (ctrl && (vk == 'Y' || vk == 'y')) {
        inicio_accion_historial(0);
        return;
    }
    if (vk == VK_DELETE) {
        inicio_accion_borrar();
        return;
    }
    if (vk == VK_INSERT)
        inicio_accion_crear(shift ? PM_KIND_ESCENA : PM_KIND_NODO);
}

void inicio_caracter(int ch) {
    size_t n;
    if (!g_inicio_editando || ch < 32 || ch > 126) return;
    if (g_inicio_edit_primer_caracter) {
        g_inicio_borrador[0] = 0;
        g_inicio_edit_primer_caracter = 0;
    }
    n = strlen(g_inicio_borrador);
    if (n + 1 >= sizeof(g_inicio_borrador)) return;
    g_inicio_borrador[n] = (char)ch;
    g_inicio_borrador[n + 1] = 0;
    g_inicio_edit_error[0] = 0;
    app_invalidar();
}

void inicio_vista_volcar(const char *ruta) {
    Inicio *in = inicio_servicio();
    FILE *f;
    int i, escenas = 0, nodos = 0, scripts = 0, asociaciones = 0;
    const EntidadModelo *e;
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "w");
    if (!f) return;
    if (g_inicio_modelo) {
        for (i = 0; i < g_inicio_modelo->num_entidades; i++) {
            e = &g_inicio_modelo->entidades[i];
            if (e->kind == PM_KIND_ESCENA) escenas++;
            if (e->kind == PM_KIND_NODO) nodos++;
            if (e->kind == PM_KIND_SCRIPT) scripts++;
            if (e->kind == PM_KIND_BINDING) asociaciones++;
        }
    }
    fprintf(f, "inicio_vista=1\n");
    fprintf(f, "inicio_ruta=%s\n", g_inicio_ruta);
    fprintf(f, "inicio_sel=%d\n", g_inicio_seleccion);
    fprintf(f, "inicio_undo=%d redo=%d ultimo=%s reparentar=%d\n",
            in->num_pasado, in->num_futuro, in->ultimo,
            g_inicio_reparentar_origen);
    fprintf(f, "inicio_filas=%d\n", g_inicio_num_filas);
    fprintf(f, "inicio_entidades=%d escenas=%d nodos=%d scripts=%d asociaciones=%d\n",
            g_inicio_modelo ? g_inicio_modelo->num_entidades : 0,
            escenas, nodos, scripts, asociaciones);
    if (g_inicio_modelo) {
        e = inicio_entidad(g_inicio_seleccion);
        if (e) fprintf(f, "inicio_seleccion=%s #%d\n", pm_kind_nombre(e->kind), e->id);
        for (i = 0; i < g_inicio_modelo->num_entidades; i++) {
            e = &g_inicio_modelo->entidades[i];
            if (e->kind == PM_KIND_NODO)
                fprintf(f, "inicio_nodo=%d escena=%d padre=%d nombre=%s\n",
                        e->id, inicio_prop_int(e, "escena", inicio_ref(e, "escena")),
                        inicio_nodo_padre(e), e->nombre);
        }
    }
    fclose(f);
}

/* --- Servicio CLI de Inicio restaurado desde 455acbb --------------------- */
/* inicio.c - servicio de nodos con historial compartido (plan 17.1).
 *
 * Capa delgada sobre ProjectModel: guarda un snapshot antes de cada cambio y
 * lo intercambia con el futuro al deshacer o rehacer. No depende de Win32 ni
 * de la interfaz, asi que se puede probar con la flag --inicio sin abrir la
 * aplicacion. Los snapshots viven en la instancia estatica de inicio.h.
 */
#include "inicio.h"
#include "modelo_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copiar_seguro(char *dst, size_t tam, const char *src) {
    size_t n;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= tam) n = tam - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void diag_fijar(char *dst, size_t tam, const char *texto) {
    if (dst && tam) copiar_seguro(dst, tam, texto);
}

static void ultimo_fijar(Inicio *in, const char *texto) {
    if (in) copiar_seguro(in->ultimo, sizeof(in->ultimo), texto);
}

static void limpiar_espacios(char *texto) {
    size_t i;
    for (i = 0; texto && texto[i]; i++)
        if (texto[i] == ' ' || texto[i] == '\t') texto[i] = '_';
}

static void recortar(char *texto) {
    size_t n, primero, ultimo;
    if (!texto) return;
    n = strlen(texto);
    primero = 0;
    while (primero < n && (texto[primero] == ' ' || texto[primero] == '\t'))
        primero++;
    ultimo = n;
    while (ultimo > primero &&
           (texto[ultimo - 1] == ' ' || texto[ultimo - 1] == '\t'))
        ultimo--;
    if (primero) memmove(texto, texto + primero, ultimo - primero);
    texto[ultimo - primero] = '\0';
}

/* Parte "campo|campo;campo" sin tocar la cadena original. */
static void trozo(const char **p, char *dst, size_t tam) {
    size_t n = 0;
    if (!dst || tam == 0) return;
    if (!p || !*p) {
        dst[0] = '\0';
        return;
    }
    while (**p && **p != '|' && **p != ';' && n + 1 < tam) {
        dst[n++] = **p;
        (*p)++;
    }
    dst[n] = '\0';
    if (**p == '|') (*p)++;
}

static int numero(const char *texto) {
    if (!texto || !texto[0]) return 0;
    return atoi(texto);
}

static void empujar(ProjectModel *pila, int *num, const ProjectModel *modelo) {
    int i;
    if (*num >= INICIO_HISTORIAL_MAX) {
        for (i = 1; i < INICIO_HISTORIAL_MAX; i++)
            pila[i - 1] = pila[i];
        *num = INICIO_HISTORIAL_MAX - 1;
    }
    pm_copiar(modelo, &pila[*num]);
    (*num)++;
}

/* Todo cambio valido descarta el futuro, como en cualquier editor. */
static void guardar_pasado(Inicio *in) {
    if (!in) return;
    empujar(in->pasado, &in->num_pasado, &in->modelo);
    in->num_futuro = 0;
}

Inicio *inicio_servicio(void) {
    static Inicio servicio;
    static int listo = 0;
    if (!listo) {
        inicio_iniciar(&servicio);
        listo = 1;
    }
    return &servicio;
}

void inicio_iniciar(Inicio *in) {
    if (!in) return;
    memset(in, 0, sizeof(*in));
    in->modelo.siguiente_id = 1;
}

int inicio_cargar(Inicio *in, const char *ruta, char *diagnostico,
                  size_t longitud) {
    ProjectModel *m;
    diag_fijar(diagnostico, longitud, "");
    if (!in || !ruta || !ruta[0]) {
        diag_fijar(diagnostico, longitud, "falta la ruta del .log");
        return 0;
    }
    m = ml_leer(ruta, diagnostico, longitud);
    if (!m) {
        if (diagnostico && longitud && !diagnostico[0])
            diag_fijar(diagnostico, longitud, "no se pudo leer el .log");
        return 0;
    }
    /* ml_leer escribe "sin errores" cuando todo va bien; aqui solo interesa
     * el diagnostico de fallo, asi que se limpia para no confundirlo con el
     * detalle de una orden posterior. */
    diag_fijar(diagnostico, longitud, "");
    /* Cargar empieza una nueva sesion: el historial anterior no aplica. */
    inicio_iniciar(in);
    pm_copiar(m, &in->modelo);
    copiar_seguro(in->origen, sizeof(in->origen), ruta);
    ultimo_fijar(in, "cargado");
    pm_destruir(m);
    return 1;
}

int inicio_crear(Inicio *in, int kind, const char *nombre, int id_padre) {
    int id;
    char error[PM_ERROR_MAX];

    if (!in) return 0;
    guardar_pasado(in);
    id = pm_anadir(&in->modelo, kind, nombre);
    if (id && id_padre &&
        !pm_reparentar(&in->modelo, id, id_padre)) id = 0;
    if (!id) {
        copiar_seguro(error, sizeof(error), pm_error(&in->modelo));
        pm_copiar(&in->pasado[in->num_pasado - 1], &in->modelo);
        in->num_pasado--;
        in->num_futuro = 0;
        ultimo_fijar(in, error);
        return 0;
    }
    ultimo_fijar(in, "creado");
    return 1;
}

int inicio_borrar(Inicio *in, int id) {
    int borrados;
    char error[PM_ERROR_MAX];

    if (!in) return 0;
    guardar_pasado(in);
    borrados = pm_borrar(&in->modelo, id);
    if (!borrados) {
        copiar_seguro(error, sizeof(error), pm_error(&in->modelo));
        pm_copiar(&in->pasado[in->num_pasado - 1], &in->modelo);
        in->num_pasado--;
        in->num_futuro = 0;
        ultimo_fijar(in, error);
        return 0;
    }
    ultimo_fijar(in, "borrado");
    return 1;
}

int inicio_reparentar(Inicio *in, int id, int id_padre) {
    char error[PM_ERROR_MAX];

    if (!in) return 0;
    guardar_pasado(in);
    if (!pm_reparentar(&in->modelo, id, id_padre)) {
        copiar_seguro(error, sizeof(error), pm_error(&in->modelo));
        pm_copiar(&in->pasado[in->num_pasado - 1], &in->modelo);
        in->num_pasado--;
        in->num_futuro = 0;
        ultimo_fijar(in, error);
        return 0;
    }
    ultimo_fijar(in, "reparentado");
    return 1;
}

int inicio_deshacer(Inicio *in) {
    if (!in || in->num_pasado <= 0) {
        ultimo_fijar(in, "no hay cambios que deshacer");
        return 0;
    }
    empujar(in->futuro, &in->num_futuro, &in->modelo);
    in->num_pasado--;
    pm_copiar(&in->pasado[in->num_pasado], &in->modelo);
    ultimo_fijar(in, "deshecho");
    return 1;
}

int inicio_rehacer(Inicio *in) {
    if (!in || in->num_futuro <= 0) {
        ultimo_fijar(in, "no hay cambios que rehacer");
        return 0;
    }
    empujar(in->pasado, &in->num_pasado, &in->modelo);
    in->num_futuro--;
    pm_copiar(&in->futuro[in->num_futuro], &in->modelo);
    ultimo_fijar(in, "rehecho");
    return 1;
}

void inicio_volcar(const Inicio *in, const char *ruta) {
    char ultimo[PM_ERROR_MAX];
    FILE *f;

    if (!in || !ruta || !ruta[0]) return;
    pm_volcar(&in->modelo, ruta);
    copiar_seguro(ultimo, sizeof(ultimo), in->ultimo);
    limpiar_espacios(ultimo);
    f = fopen(ruta, "a");
    if (!f) return;
    fprintf(f, "inicio undo=%d redo=%d ultimo=%s\n",
            in->num_pasado, in->num_futuro, ultimo);
    fclose(f);
}

int inicio_kind(const char *texto) {
    static const char *nombres[] = {
        "proyecto", "recurso", "escena", "actor",
        "instancia", "control", "conexion", "accion"
    };
    int i, n;
    if (!texto || !texto[0]) return 0;
    for (i = 0; i < 8; i++)
        if (_stricmp(texto, nombres[i]) == 0) return i + 1;
    n = numero(texto);
    return n > 0 ? n : 0;
}

int inicio_ejecutar(Inicio *in, const char *comandos, char *diagnostico,
                    size_t longitud) {
    const char *p;
    int num = 0;

    diag_fijar(diagnostico, longitud, "");
    if (!in || !comandos) {
        diag_fijar(diagnostico, longitud, "faltan datos para Inicio");
        return 0;
    }
    p = comandos;
    while (*p) {
        char cmd[32], a[PM_RUTA_MAX], b[PM_NOMBRE_MAX], c[32];
        char mensaje[PM_ERROR_MAX * 2];
        const char *detalle;
        int ok = 0;

        trozo(&p, cmd, sizeof(cmd));
        recortar(cmd);
        if (!cmd[0]) {
            if (*p == ';') p++;
            continue;
        }
        if (strcmp(cmd, "cargar") == 0) {
            trozo(&p, a, sizeof(a));
            recortar(a);
            ok = inicio_cargar(in, a, diagnostico, longitud);
        } else if (strcmp(cmd, "crear") == 0) {
            trozo(&p, a, sizeof(a));
            trozo(&p, b, sizeof(b));
            trozo(&p, c, sizeof(c));
            recortar(a);
            recortar(b);
            recortar(c);
            ok = inicio_crear(in, inicio_kind(a), b, numero(c));
        } else if (strcmp(cmd, "borrar") == 0) {
            trozo(&p, a, sizeof(a));
            recortar(a);
            ok = inicio_borrar(in, numero(a));
        } else if (strcmp(cmd, "reparentar") == 0) {
            trozo(&p, a, sizeof(a));
            trozo(&p, b, sizeof(b));
            recortar(a);
            recortar(b);
            ok = inicio_reparentar(in, numero(a), numero(b));
        } else if (strcmp(cmd, "deshacer") == 0) {
            ok = inicio_deshacer(in);
        } else if (strcmp(cmd, "rehacer") == 0) {
            ok = inicio_rehacer(in);
        } else if (strcmp(cmd, "volcar") == 0) {
            trozo(&p, a, sizeof(a));
            recortar(a);
            inicio_volcar(in, a);
            ok = a[0] != '\0';
        } else {
            diag_fijar(diagnostico, longitud, "comando desconocido");
        }
        if (!ok) {
            detalle = in->ultimo[0] ? in->ultimo : "operacion incorrecta";
            if (diagnostico && longitud && diagnostico[0]) detalle = diagnostico;
            snprintf(mensaje, sizeof(mensaje), "comando %d (%s): %s",
                     num + 1, cmd, detalle);
            diag_fijar(diagnostico, longitud, mensaje);
            return 0;
        }
        num++;
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
    }
    return 1;
}
