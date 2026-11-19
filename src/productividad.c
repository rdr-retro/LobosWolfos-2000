/* ==========================================================================
 * Fase 36: productividad del editor.
 *
 * Todo el estado de esta fase vive aqui y no se mete en `App` ni en el
 * archivo LOGIC3. Las instantaneas son copias completas del modelo en
 * memoria, el portapapeles copia un objeto con sus scripts y variables, y
 * las busquedas y metricas son de solo lectura. El escritor canonico y el
 * CRC no se tocan.
 * ========================================================================== */
#include "app.h"
#include "logic3.h"
#include <ctype.h>

#define PROD_HISTORIAL_MAX    12
#define PROD_BUSQUEDA_MAX     64
#define PROD_CLAVE_MAX        96
#define PROD_SNIPPET_MAX      24
#define PROD_SNIPPET_TEXTO_MAX 160

typedef struct {
    Proyecto3 modelo;
    int escena;
    int objeto;
    int pestana;
    char etiqueta[64];
} ProdInstantanea;

static ProdInstantanea historial[PROD_HISTORIAL_MAX];
static int num_historial;
static int indice_historial;

typedef struct {
    Objeto3 objeto;
    int tiene;
} ProdPortapapeles;

static ProdPortapapeles portapapeles;
static char ultimo_error[192];
static char ultima_busqueda[PROD_CLAVE_MAX];
static int resultados[PROD_BUSQUEDA_MAX];
static int num_resultados;
static int avanzado;
static char snippets[PROD_SNIPPET_MAX][PROD_SNIPPET_TEXTO_MAX];
static int num_snippets;

/* ------------------------------------------------------------------ */
 /* Utilidades                                                          */
 /* ------------------------------------------------------------------ */
static void prod_error(const char *formato, ...) {
    va_list args;
    va_start(args, formato);
    _vsnprintf(ultimo_error, sizeof(ultimo_error) - 1, formato, args);
    va_end(args);
    ultimo_error[sizeof(ultimo_error) - 1] = 0;
    app_estado("%s", ultimo_error);
}

const char *productividad_ultimo_error(void) {
    return ultimo_error;
}

static void producto_iniciar(void) {
    static int iniciado;
    if (iniciado) return;
    iniciado = 1;
    memset(&portapapeles, 0, sizeof(portapapeles));
    memset(snippets, 0, sizeof(snippets));
    num_snippets = 0;
    /* Plantillas de script: siempre disponibles, even sin projecto. */
    snprintf(snippets[num_snippets++], PROD_SNIPPET_TEXTO_MAX,
             "inicio|decir hola");
    snprintf(snippets[num_snippets++], PROD_SNIPPET_TEXTO_MAX,
             "fotograma|si tecla arriba|mover 2 0");
    snprintf(snippets[num_snippets++], PROD_SNIPPET_TEXTO_MAX,
             "tocar|sumar puntos 1");
    snprintf(snippets[num_snippets++], PROD_SNIPPET_TEXTO_MAX,
             "recibir|si vivo|saltar");
}

/* Copia profunda de un proyecto: los unicos campos con puntero son objetos
 * y bloques. Las listas tambien reservan elementos aparte. */
static int modelo_copiar(Proyecto3 *dst, const Proyecto3 *src) {
    int i;
    if (!src) return 0;
    memset(dst, 0, sizeof(*dst));
    *dst = *src;
    dst->objetos = NULL;
    dst->bloques = NULL;
    dst->cap_objetos = 0;
    dst->cap_bloques = 0;
    if (src->num_objetos > 0) {
        dst->objetos = (Objeto3 *)malloc(sizeof(Objeto3) *
                                         (size_t)src->num_objetos);
        if (!dst->objetos) return 0;
        memcpy(dst->objetos, src->objetos,
               sizeof(Objeto3) * (size_t)src->num_objetos);
        dst->cap_objetos = src->num_objetos;
    }
    if (src->num_bloques > 0) {
        dst->bloques = (Bloque3 *)malloc(sizeof(Bloque3) *
                                         (size_t)src->num_bloques);
        if (!dst->bloques) {
            free(dst->objetos);
            dst->objetos = NULL;
            return 0;
        }
        memcpy(dst->bloques, src->bloques,
               sizeof(Bloque3) * (size_t)src->num_bloques);
        dst->cap_bloques = src->num_bloques;
    }
    for (i = 0; i < dst->num_listas; i++) {
        Lista3 *l = &dst->listas[i];
        l->elementos = NULL;
        if (src->listas[i].elementos && l->num_elementos > 0) {
            l->elementos = (char (*)[L3_TEXTO_MAX])malloc(
                (size_t)L3_TEXTO_MAX * (size_t)l->num_elementos);
            if (!l->elementos) {
                logic3_liberar(dst);
                return 0;
            }
            memcpy(l->elementos, src->listas[i].elementos,
                   (size_t)L3_TEXTO_MAX * (size_t)l->num_elementos);
        }
    }
    return 1;
}

static void modelo_soltar(Proyecto3 *m) {
    logic3_liberar(m);
}

static int instantanea_guardar(const char *etiqueta) {
    Proyecto3 *p = logic3_modelo_activo();
    ProdInstantanea *dst;
    int i;
    producto_iniciar();
    if (!p) {
        prod_error("Abre o crea un proyecto antes de guardar");
        return 0;
    }
    if (num_historial >= PROD_HISTORIAL_MAX) {
        for (i = 0; i < num_historial - 1; i++) {
            modelo_soltar(&historial[i].modelo);
            historial[i] = historial[i + 1];
        }
        memset(&historial[num_historial - 1], 0, sizeof(historial[0]));
        num_historial--;
        if (indice_historial > 0) indice_historial--;
    }
    for (i = indice_historial + 1; i < num_historial; i++)
        modelo_soltar(&historial[i].modelo);
    num_historial = indice_historial + 1;
    dst = &historial[num_historial++];
    memset(dst, 0, sizeof(*dst));
    if (!modelo_copiar(&dst->modelo, p)) {
        prod_error("Sin memoria para guardar el estado");
        num_historial--;
        return 0;
    }
    dst->escena = g.estudio_escena;
    dst->objeto = g.estudio_objeto;
    dst->pestana = g.estudio_pestana;
    snprintf(dst->etiqueta, sizeof(dst->etiqueta), "%s",
             etiqueta ? etiqueta : "estado");
    indice_historial = num_historial - 1;
    return 1;
}

/* Restaura el estado indicado. La instantanea actual se conserva para
 * poder rehacer. Se devuelve 1 cuando el modelo cambio. */
static int instantanea_restaurar(int indice) {
    Proyecto3 *p = logic3_modelo_activo();
    ProdInstantanea actual;
    ProdInstantanea *h;
    if (!p || indice < 0 || indice >= num_historial) return 0;
    memset(&actual, 0, sizeof(actual));
    if (!modelo_copiar(&actual.modelo, p)) {
        prod_error("Sin memoria para deshacer");
        return 0;
    }
    actual.escena = g.estudio_escena;
    actual.objeto = g.estudio_objeto;
    actual.pestana = g.estudio_pestana;
    snprintf(actual.etiqueta, sizeof(actual.etiqueta), "%s", "actual");
    h = &historial[indice];
    logic3_liberar(p);
    if (!modelo_copiar(p, &h->modelo)) {
        prod_error("Sin memoria para restaurar");
        modelo_soltar(&actual.modelo);
        return 0;
    }
    g.estudio_escena = h->escena;
    g.estudio_objeto = h->objeto;
    g.estudio_pestana = h->pestana;
    historial[indice] = actual;
    indice_historial = indice;
    app_invalidar();
    return 1;
}

void productividad_registrar(const char *etiqueta) {
    instantanea_guardar(etiqueta);
}

int productividad_deshacer(void) {
    producto_iniciar();
    if (indice_historial <= 0) {
        app_estado("No hay cambios para deshacer");
        return 0;
    }
    if (!instantanea_restaurar(indice_historial - 1)) return 0;
    app_estado("Deshecho: %s", historial[indice_historial + 1].etiqueta);
    return 1;
}

int productividad_rehacer(void) {
    producto_iniciar();
    if (indice_historial >= num_historial - 1) {
        app_estado("No hay cambios para rehacer");
        return 0;
    }
    if (!instantanea_restaurar(indice_historial + 1)) return 0;
    app_estado("Rehecho: %s", historial[indice_historial].etiqueta);
    return 1;
}

void productividad_reiniciar(void) {
    int i;
    for (i = 0; i < num_historial; i++)
        modelo_soltar(&historial[i].modelo);
    memset(historial, 0, sizeof(historial));
    num_historial = 0;
    indice_historial = 0;
    num_resultados = 0;
    ultima_busqueda[0] = 0;
    ultimo_error[0] = 0;
}

/* ------------------------------------------------------------------ */
 /* Portapapeles de objetos                                             */
 /* ------------------------------------------------------------------ */
static int objeto_indice_por_id(const Proyecto3 *p, int id) {
    int i;
    if (!p || id <= 0) return -1;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id == id) return i;
    return -1;
}

int productividad_objeto_copiar(void) {
    Proyecto3 *p = logic3_modelo_activo();
    int i;
    producto_iniciar();
    if (!p) {
        prod_error("Abre o crea un proyecto antes de copiar");
        return 0;
    }
    i = objeto_indice_por_id(p, g.estudio_objeto);
    if (i < 0) {
        prod_error("Selecciona un objeto para copiar");
        return 0;
    }
    memcpy(&portapapeles.objeto, &p->objetos[i], sizeof(Objeto3));
    portapapeles.tiene = 1;
    app_estado("Objeto copiado: %s", portapapeles.objeto.nombre);
    return 1;
}

static int escena_existe(const Proyecto3 *p, int id) {
    int i;
    if (!p) return 0;
    for (i = 0; i < p->num_escenas; i++)
        if (p->escenas[i].id == id) return 1;
    return 0;
}

static int max_objeto_id(const Proyecto3 *p) {
    int maximo = 0, i;
    for (i = 0; i < p->num_objetos; i++)
        if (p->objetos[i].id > maximo) maximo = p->objetos[i].id;
    return maximo;
}

static int max_evento_id(const Proyecto3 *p) {
    int maximo = 0, i, j;
    for (i = 0; i < p->num_objetos; i++)
        for (j = 0; j < p->objetos[i].num_eventos; j++)
            if (p->objetos[i].eventos[j].id > maximo)
                maximo = p->objetos[i].eventos[j].id;
    for (i = 0; i < p->num_funciones; i++)
        for (j = 0; j < p->funciones[i].num_eventos; j++)
            if (p->funciones[i].eventos[j].id > maximo)
                maximo = p->funciones[i].eventos[j].id;
    return maximo;
}

/* Reasigna los ids de eventos del duplicado y de los bloques que apuntan
 * a ellos. Los bloques nuevos reciben ids libres. */
static int objeto_pegar(const Objeto3 *fuente, const char *nombre,
                        int *id_salida) {
    Proyecto3 *p = logic3_modelo_activo();
    Objeto3 *tmp;
    Objeto3 *o;
    int viejo, nuevo, i, j, base, siguiente;
    if (!p) {
        prod_error("Abre o crea un proyecto antes de pegar");
        return 0;
    }
    if (p->num_objetos >= L3_OBJETOS_MAX) {
        prod_error("No caben mas objetos");
        return 0;
    }
    tmp = (Objeto3 *)realloc(p->objetos,
                             sizeof(Objeto3) * (size_t)(p->num_objetos + 1));
    if (!tmp) {
        prod_error("Sin memoria para pegar el objeto");
        return 0;
    }
    p->objetos = tmp;
    p->cap_objetos = p->num_objetos + 1;
    o = &p->objetos[p->num_objetos++];
    *o = *fuente;
    o->id = max_objeto_id(p) > 0 ? max_objeto_id(p) + 1 : 1;
    if (nombre && nombre[0])
        snprintf(o->nombre, sizeof(o->nombre), "%s", nombre);
    else {
        recortar_texto(o->nombre, fuente->nombre, sizeof(o->nombre) - 7);
        strncat(o->nombre, " copia", sizeof(o->nombre) - strlen(o->nombre) - 1);
    }
    if (p->num_escenas <= 0 || !escena_existe(p, o->escena))
        o->escena = p->num_escenas > 0 ? p->escenas[0].id : 0;
    o->x += 8;
    o->y += 8;

    base = max_evento_id(p) + 1;
    siguiente = base;
    for (i = 0; i < o->num_eventos; i++) {
        viejo = o->eventos[i].id;
        o->eventos[i].id = siguiente++;
        for (j = 0; j < p->num_bloques; j++)
            if (p->bloques[j].evento == viejo &&
                p->bloques[j].evento != o->eventos[i].id)
                p->bloques[j].evento = o->eventos[i].id;
    }
    (void)nuevo;
    g.estudio_objeto = o->id;
    g.estudio_escena = o->escena;
    if (id_salida) *id_salida = o->id;
    app_invalidar();
    return 1;
}

int productividad_objeto_pegar(const char *nombre) {
    producto_iniciar();
    if (!portapapeles.tiene) {
        prod_error("El portapapeles no tiene un objeto");
        return 0;
    }
    if (!instantanea_guardar("pegar objeto")) return 0;
    if (!objeto_pegar(&portapapeles.objeto, nombre, NULL)) return 0;
    app_estado("Objeto pegado con %d eventos",
              portapapeles.objeto.num_eventos);
    return 1;
}

int productividad_objeto_duplicar(const char *nombre) {
    Proyecto3 *p = logic3_modelo_activo();
    int i, id = 0;
    producto_iniciar();
    if (!p) {
        prod_error("Abre o crea un proyecto antes de duplicar");
        return 0;
    }
    i = objeto_indice_por_id(p, g.estudio_objeto);
    if (i < 0) {
        prod_error("Selecciona un objeto para duplicar");
        return 0;
    }
    memcpy(&portapapeles.objeto, &p->objetos[i], sizeof(Objeto3));
    portapapeles.tiene = 1;
    if (!instantanea_guardar("duplicar objeto")) return 0;
    if (!objeto_pegar(&portapapeles.objeto, nombre, &id)) return 0;
    app_estado("Objeto duplicado: id %d, %d variables, %d eventos",
              id, portapapeles.objeto.num_variables,
              portapapeles.objeto.num_eventos);
    return 1;
}

/* ------------------------------------------------------------------ */
 /* Busqueda global                                                    */
 /* ------------------------------------------------------------------ */
static void resultado_anadir(int tipo, int indice) {
    if (num_resultados >= PROD_BUSQUEDA_MAX) return;
    resultados[num_resultados++] = tipo * 100000 + indice;
}

static void buscar_texto(const char *aguja, const char *texto,
                         int tipo, int indice) {
    if (aguja[0] && texto && strstr(texto, aguja))
        resultado_anadir(tipo, indice);
}

int productividad_buscar(const char *texto) {
    Proyecto3 *p = logic3_modelo_activo();
    char aguja[PROD_CLAVE_MAX];
    int i, j;
    producto_iniciar();
    if (!texto || !texto[0]) {
        prod_error("Escribe un texto para buscar");
        return 0;
    }
    recortar_texto(aguja, texto, sizeof(aguja));
    num_resultados = 0;
    recortar_texto(ultima_busqueda, aguja, sizeof(ultima_busqueda));
    if (!p) return 0;
    /* 1 = variable, 2 = lista, 3 = evento, 4 = bloque, 5 = traje. */
    for (i = 0; i < p->num_variables; i++)
        buscar_texto(aguja, p->variables[i].nombre, 1, i);
    for (i = 0; i < p->num_listas; i++)
        buscar_texto(aguja, p->listas[i].nombre, 2, i);
    for (i = 0; i < p->num_objetos; i++) {
        Objeto3 *o = &p->objetos[i];
        for (j = 0; j < o->num_variables; j++)
            buscar_texto(aguja, o->variables[j].nombre, 1, 1000 + i * 64 + j);
        for (j = 0; j < o->num_eventos; j++)
            buscar_texto(aguja, o->eventos[j].nombre, 3, 1000 + i * 64 + j);
        for (j = 0; j < o->num_trajes; j++)
            buscar_texto(aguja, o->trajes[j].imagen, 5, 1000 + i * 32 + j);
    }
    for (i = 0; i < p->num_funciones; i++)
        for (j = 0; j < p->funciones[i].num_eventos; j++)
            buscar_texto(aguja, p->funciones[i].eventos[j].nombre, 3,
                         10000 + i * 64 + j);
    for (i = 0; i < p->num_bloques; i++) {
        buscar_texto(aguja, p->bloques[i].tipo, 4, i);
        for (j = 0; j < p->bloques[i].num_campos; j++)
            buscar_texto(aguja, p->bloques[i].campos[j].valor, 4, i);
    }
    app_estado("%d resultados para %s", num_resultados, aguja);
    return num_resultados;
}

/* ------------------------------------------------------------------ */
 /* Duplicacion en lote                                                 */
 /* ------------------------------------------------------------------ */
int productividad_duplicar_seleccion(int cantidad, const char *prefijo) {
    Proyecto3 *p = logic3_modelo_activo();
    int i, j, id_origen, creados = 0, cantidad_real;
    char nombre[80];
    producto_iniciar();
    if (!p) {
        prod_error("Abre o crea un proyecto antes de duplicar");
        return 0;
    }
    id_origen = g.estudio_objeto;
    i = objeto_indice_por_id(p, id_origen);
    if (i < 0) {
        prod_error("Selecciona un objeto para duplicar en lote");
        return 0;
    }
    cantidad_real = cantidad < 1 ? 1 : cantidad;
    if (cantidad_real > 32) cantidad_real = 32;
    memcpy(&portapapeles.objeto, &p->objetos[i], sizeof(Objeto3));
    portapapeles.tiene = 1;
    if (!instantanea_guardar("duplicar lote")) return 0;
    for (j = 0; j < cantidad_real; j++) {
        if (prefijo && prefijo[0])
            snprintf(nombre, sizeof(nombre), "%s %d", prefijo, j + 1);
        else
            snprintf(nombre, sizeof(nombre), "%s copia %d",
                     portapapeles.objeto.nombre, j + 1);
        if (objeto_pegar(&portapapeles.objeto, nombre, NULL)) creados++;
        else break;
    }
    app_estado("Duplicados %d objetos", creados);
    return creados;
}

/* ------------------------------------------------------------------ */
 /* Plantillas, ayuda contextual y modo                                 */
 /* ------------------------------------------------------------------ */
int productividad_snippet_obtener(int indice, char *dst, int tam) {
    producto_iniciar();
    if (indice < 0 || indice >= num_snippets || !dst || tam <= 0) return 0;
    recortar_texto(dst, snippets[indice], tam);
    return 1;
}

int productividad_snippet_crear(const char *texto) {
    producto_iniciar();
    if (!texto || !texto[0]) return 0;
    if (num_snippets >= PROD_SNIPPET_MAX) return 0;
    recortar_texto(snippets[num_snippets], texto, PROD_SNIPPET_TEXTO_MAX);
    num_snippets++;
    app_estado("Snippet guardado");
    return 1;
}

const char *productividad_ayuda(const char *bloque) {
    static char respuesta[256];
    producto_iniciar();
    if (!bloque || !bloque[0]) bloque = "inicio";
    if (strstr(bloque, "mover"))
        snprintf(respuesta, sizeof(respuesta),
                 "Mover: 'mover dx dy' desplaza el objeto. Ejemplo: mover 2 0");
    else if (strstr(bloque, "si"))
        snprintf(respuesta, sizeof(respuesta),
                 "Si: 'si condicion' abre un bloque. Ejemplo: si tecla arriba");
    else if (strstr(bloque, "decir"))
        snprintf(respuesta, sizeof(respuesta),
                 "Decir: 'decir texto' muestra un mensaje. Ejemplo: decir hola");
    else
        snprintf(respuesta, sizeof(respuesta),
                 "%s: evento o bloque. Ejemplo: al inicio, decir hola",
                 bloque);
    return respuesta;
}

void productividad_modo_avanzado(int activo) {
    avanzado = activo ? 1 : 0;
}

int productividad_modo(void) {
    return avanzado;
}

/* ------------------------------------------------------------------ */
 /* Metricas y volcado                                                  */
 /* ------------------------------------------------------------------ */
void productividad_metricas(int *escenas, int *objetos, int *eventos,
                            int *bloques, int *variables, int *listas,
                            int *elementos, int *bytes) {
    Proyecto3 *p = logic3_modelo_activo();
    int i, ev = 0, var = 0, elem = 0;
    if (escenas) *escenas = 0;
    if (objetos) *objetos = 0;
    if (eventos) *eventos = 0;
    if (bloques) *bloques = 0;
    if (variables) *variables = 0;
    if (listas) *listas = 0;
    if (elementos) *elementos = 0;
    if (bytes) *bytes = 0;
    if (!p) return;
    for (i = 0; i < p->num_objetos; i++) {
        ev += p->objetos[i].num_eventos;
        var += p->objetos[i].num_variables;
    }
    for (i = 0; i < p->num_funciones; i++) ev += p->funciones[i].num_eventos;
    for (i = 0; i < p->num_listas; i++) elem += p->listas[i].num_elementos;
    for (i = 0; i < p->num_bloques; i++) var += p->bloques[i].num_campos;
    if (escenas) *escenas = p->num_escenas;
    if (objetos) *objetos = p->num_objetos;
    if (eventos) *eventos = ev;
    if (bloques) *bloques = p->num_bloques;
    if (variables) *variables = p->num_variables + var;
    if (listas) *listas = p->num_listas;
    if (elementos) *elementos = elem;
    if (bytes) {
        size_t total = sizeof(Proyecto3);
        total += (size_t)p->cap_objetos * sizeof(Objeto3);
        total += (size_t)p->cap_bloques * sizeof(Bloque3);
        for (i = 0; i < p->num_listas; i++)
            total += (size_t)p->listas[i].num_elementos * L3_TEXTO_MAX;
        *bytes = (int)(total > 0x7fffffffu ? 0x7fffffff : total);
    }
}

void productividad_volcar(const char *ruta) {
    FILE *f;
    int escenas = 0, objetos = 0, eventos = 0, bloques = 0;
    int variables = 0, listas = 0, elementos = 0, bytes = 0;
    int i;
    producto_iniciar();
    if (!ruta || !ruta[0]) return;
    f = fopen(ruta, "w");
    if (!f) return;
    productividad_metricas(&escenas, &objetos, &eventos, &bloques,
                           &variables, &listas, &elementos, &bytes);
    fprintf(f, "escenas %d\n", escenas);
    fprintf(f, "objetos %d\n", objetos);
    fprintf(f, "eventos %d\n", eventos);
    fprintf(f, "bloques %d\n", bloques);
    fprintf(f, "variables %d\n", variables);
    fprintf(f, "listas %d\n", listas);
    fprintf(f, "elementos %d\n", elementos);
    fprintf(f, "memoria %d\n", bytes);
    fprintf(f, "historial %d/%d\n", indice_historial + 1, num_historial);
    fprintf(f, "portapapeles %d\n", portapapeles.tiene);
    fprintf(f, "snippets %d\n", num_snippets);
    fprintf(f, "modo %s\n", avanzado ? "avanzado" : "principiante");
    fprintf(f, "busqueda %s\n", ultima_busqueda[0] ? ultima_busqueda : "-");
    for (i = 0; i < num_resultados; i++)
        fprintf(f, "resultado %d\n", resultados[i]);
    fclose(f);
}
