/* modelo_logic3.c - adaptador del ProjectModel a LOGIC3 (plan 16.5).
 *
 * El proyecto se escribe con el escritor canonico para no cambiar el CRC,
 * el formato ni el resultado del juego. La correspondencia es deliberada:
 * la cabecera `proyecto` toma sus valores de PM_KIND_PROYECTO; cada nodo
 * del grafo se convierte en un bloque `sistema.pieza` con los campos
 * ui, pieza, id, xy, nombre, enlace, valor y valor2; y cada conexion
 * pasa a ser el campo `enlace` del bloque de origen, igual que hace el
 * lector de `montaje`. La entidad de proyecto no crea pieza: solo describe
 * la cabecera, para no anadir actores ni escenas al juego ejecutable.
 */
#include "modelo_logic3.h"

#include "logic3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    Bloque3 *bloque;
} ReferenciaBloque;

static void ml3_copiar(char *dst, size_t tam, const char *src) {
    size_t n;
    if (!dst || tam == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= tam) n = tam - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static const PropiedadModelo *ml3_propiedad(const EntidadModelo *e,
                                            const char *clave) {
    int i;
    if (!e || !clave) return NULL;
    for (i = 0; i < e->num_propiedades; i++)
        if (strcmp(e->propiedades[i].clave, clave) == 0)
            return &e->propiedades[i];
    return NULL;
}

static const char *ml3_valor(const EntidadModelo *e, const char *clave) {
    const PropiedadModelo *p = ml3_propiedad(e, clave);
    return p ? p->valor : "";
}

static int ml3_entero(const EntidadModelo *e, const char *clave, int defecto) {
    const char *texto = ml3_valor(e, clave);
    char *fin;
    long n;
    if (!texto[0]) return defecto;
    n = strtol(texto, &fin, 10);
    if (*fin || n < -2147483647L || n > 2147483647L) return defecto;
    return (int)n;
}

static unsigned int ml3_color(const EntidadModelo *e, unsigned int defecto) {
    const char *texto = ml3_valor(e, "color");
    char *fin;
    unsigned long n;
    if (!texto[0]) return defecto;
    n = strtoul(texto, &fin, 16);
    if (*fin) return defecto;
    return (unsigned int)n;
}

static const char *ml3_tipo(const EntidadModelo *e) {
    const char *tipo = ml3_valor(e, "tipo");
    if (tipo[0]) return tipo;
    switch (e->kind) {
    case PM_KIND_ESCENA:    return "sistema.escena";
    case PM_KIND_ACTOR:     return "sistema.actor";
    case PM_KIND_INSTANCIA: return "sistema.clon";
    case PM_KIND_CONTROL:   return "sistema.teclado";
    case PM_KIND_ACCION:    return "sistema.accion.movimiento";
    case PM_KIND_RECURSO:   return "sistema.recurso.imagen";
    default:                return "";
    }
}

static int ml3_campo(Bloque3 *b, const char *clave, const char *valor) {
    int i;
    if (!b || !clave || !valor) return 0;
    for (i = 0; i < b->num_campos; i++) {
        if (strcmp(b->campos[i].clave, clave) != 0) continue;
        ml3_copiar(b->campos[i].valor, sizeof(b->campos[i].valor), valor);
        return 1;
    }
    if (b->num_campos >= L3_CAMPOS_MAX) return 0;
    ml3_copiar(b->campos[b->num_campos].clave,
               sizeof(b->campos[b->num_campos].clave), clave);
    ml3_copiar(b->campos[b->num_campos].valor,
               sizeof(b->campos[b->num_campos].valor), valor);
    b->campos[b->num_campos].linea = b->linea;
    b->num_campos++;
    return 1;
}

static void ml3_cabecera(Proyecto3 *p, const ProjectModel *m) {
    const EntidadModelo *proyecto = NULL;
    int i;
    for (i = 0; i < m->num_entidades; i++) {
        if (m->entidades[i].kind == PM_KIND_PROYECTO) {
            proyecto = &m->entidades[i];
            break;
        }
    }
    if (!proyecto) return;
    ml3_copiar(p->nombre, sizeof(p->nombre), proyecto->nombre);
    p->ancho = ml3_entero(proyecto, "ancho", p->ancho);
    p->alto = ml3_entero(proyecto, "alto", p->alto);
    p->limitar_cuadros = ml3_entero(proyecto, "capas", p->limitar_cuadros);
    p->fps = ml3_entero(proyecto, "fps", p->fps);
    p->velocidad = ml3_entero(proyecto, "modo", p->velocidad);
    p->fondo = ml3_color(proyecto, p->fondo);
}

static int ml3_crear_pieza(Proyecto3 *p, const EntidadModelo *e,
                           ReferenciaBloque *referencias, int *num_referencias) {
    Bloque3 *b = NULL;
    char numero[32];
    char xy[L3_TEXTO_MAX];
    const char *tipo = ml3_tipo(e);
    if (!tipo[0]) return 1;
    if (!logic3_bloque_nuevo(p, 1, 0, p->num_bloques, "sistema.pieza", &b))
        return 0;
    b->linea = e->origen.linea;
    snprintf(numero, sizeof(numero), "%d", e->id);
    snprintf(xy, sizeof(xy), "%d,%d", e->ui.x, e->ui.y);
    if (!ml3_campo(b, "ui", "1") ||
        !ml3_campo(b, "pieza", tipo) ||
        !ml3_campo(b, "id", numero) ||
        !ml3_campo(b, "xy", xy) ||
        !ml3_campo(b, "nombre", e->nombre[0] ? e->nombre : tipo) ||
        !ml3_campo(b, "enlace", ml3_valor(e, "enlace")) ||
        !ml3_campo(b, "valor", ml3_valor(e, "valor")) ||
        !ml3_campo(b, "valor2", ml3_valor(e, "valor2")))
        return 0;
    referencias[*num_referencias].id = e->id;
    referencias[*num_referencias].bloque = b;
    (*num_referencias)++;
    return 1;
}

static int ml3_id_referencia(const EntidadModelo *e, const char *nombre) {
    int i;
    if (!e) return 0;
    for (i = 0; i < e->num_referencias; i++)
        if (strcmp(e->referencias[i].nombre, nombre) == 0)
            return e->referencias[i].id;
    return 0;
}

static Bloque3 *ml3_bloque_id(ReferenciaBloque *referencias, int num, int id) {
    int i;
    for (i = 0; i < num; i++)
        if (referencias[i].id == id) return referencias[i].bloque;
    return NULL;
}

static void ml3_aplicar_conexiones(const ProjectModel *m,
                                   ReferenciaBloque *referencias, int num) {
    int i;
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        Bloque3 *origen;
        char numero[32];
        int destino;
        if (e->kind != PM_KIND_CONEXION) continue;
        origen = ml3_bloque_id(referencias, num,
                               ml3_id_referencia(e, "origen"));
        destino = ml3_id_referencia(e, "destino");
        if (!origen || destino <= 0) continue;
        snprintf(numero, sizeof(numero), "%d", destino);
        (void)ml3_campo(origen, "enlace", numero);
    }
}

int ml3_generar(const ProjectModel *m, const char *ruta,
                char *diagnostico, size_t longitud) {
    Proyecto3 proyecto;
    ReferenciaBloque referencias[PM_ENTIDADES_MAX];
    char error[L3_ERROR_MAX];
    const char *mensaje = "";
    int num_referencias = 0;
    int ok = 0;
    int i;

    if (diagnostico && longitud > 0) diagnostico[0] = '\0';
    if (!m || !ruta || !ruta[0]) {
        ml3_copiar(diagnostico, longitud, "falta el modelo o la ruta de salida");
        return 0;
    }

    logic3_iniciar(&proyecto);
    error[0] = '\0';
    ml3_cabecera(&proyecto, m);
    for (i = 0; i < m->num_entidades; i++) {
        const EntidadModelo *e = &m->entidades[i];
        if (e->kind == PM_KIND_PROYECTO || e->kind == PM_KIND_CONEXION)
            continue;
        if (num_referencias >= PM_ENTIDADES_MAX) {
            mensaje = "demasiadas piezas para el LOGIC3";
            goto fin;
        }
        if (!ml3_crear_pieza(&proyecto, e, referencias, &num_referencias)) {
            mensaje = "no cabe la pieza en el LOGIC3";
            goto fin;
        }
    }
    ml3_aplicar_conexiones(m, referencias, num_referencias);
    if (!logic3_validar(&proyecto, error, sizeof(error))) {
        mensaje = error;
        goto fin;
    }
    if (!logic3_guardar(&proyecto, ruta, error, sizeof(error))) {
        mensaje = error;
        goto fin;
    }
    ok = 1;

fin:
    if (!ok) ml3_copiar(diagnostico, longitud, mensaje);
    logic3_liberar(&proyecto);
    return ok;
}
